#include "sheet_xml_reader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>
#include <unordered_map>

#include "a1_reference.h"
#include "cell_format.h"
#include "number_format.h"
#include "xml_parsers.h"
#include "zip_reader.h"

namespace baja_xlsx {

namespace {

// ---------------------------------------------------------------------------
// Text helpers
//
// The template renderer has its own copies of the first three: it scans the
// same sheet repeatedly (once per render, or once per cache life) with random
// access, while this reader walks a big sheet exactly twice with a forward
// cursor. Keeping the two shapes apart is deliberate; the escaping rules must
// stay identical (see unescapeXml in xlsx_template.cpp).
// ---------------------------------------------------------------------------

bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string trimmed(const std::string& text) {
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return std::string();
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string unescapeXml(const std::string& text) {
    if (text.find('&') == std::string::npos) return text;

    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '&') {
            out.push_back(text[i]);
            continue;
        }
        const size_t semi = text.find(';', i);
        if (semi == std::string::npos || semi - i > 8) {
            out.push_back(text[i]);
            continue;
        }
        const std::string entity = text.substr(i + 1, semi - i - 1);
        if (entity == "amp") out.push_back('&');
        else if (entity == "lt") out.push_back('<');
        else if (entity == "gt") out.push_back('>');
        else if (entity == "quot") out.push_back('"');
        else if (entity == "apos") out.push_back('\'');
        else if (!entity.empty() && entity[0] == '#') {
            const bool hex = entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X');
            const std::string digits = entity.substr(hex ? 2 : 1);
            unsigned long code = 0;
            bool ok = !digits.empty();
            for (char c : digits) {
                int value = 0;
                if (c >= '0' && c <= '9') value = c - '0';
                else if (hex && c >= 'a' && c <= 'f') value = c - 'a' + 10;
                else if (hex && c >= 'A' && c <= 'F') value = c - 'A' + 10;
                else { ok = false; break; }
                code = code * (hex ? 16ul : 10ul) + static_cast<unsigned long>(value);
            }
            if (!ok || code > 0x10FFFF) {
                out.append(text, i, semi - i + 1);
                i = semi;
                continue;
            }
            if (code < 0x80) {
                out.push_back(static_cast<char>(code));
            } else if (code < 0x800) {
                out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            } else if (code < 0x10000) {
                out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            } else {
                out.push_back(static_cast<char>(0xF0 | (code >> 18)));
                out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
        } else {
            out.append(text, i, semi - i + 1);
        }
        i = semi;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Element scanning (forward only)
// ---------------------------------------------------------------------------

struct Span {
    size_t start = 0;
    size_t end = 0;
    size_t openEnd = 0;
    bool selfClosing = false;
};

bool findElement(const std::string& xml, const std::string& tag, size_t from, Span& out) {
    const size_t start = xmlp::findTagOpen(xml, tag, from);
    if (start == std::string::npos) return false;
    const size_t openEnd = xml.find('>', start);
    if (openEnd == std::string::npos) return false;

    out.start = start;
    out.openEnd = openEnd;
    out.selfClosing = xml[openEnd - 1] == '/';
    if (out.selfClosing) {
        out.end = openEnd + 1;
        return true;
    }
    const std::string closeTag = "</" + tag + ">";
    const size_t close = xml.find(closeTag, openEnd);
    if (close == std::string::npos) return false;
    out.end = close + closeTag.size();
    return true;
}

std::string attribute(const std::string& xml, size_t elementStart, const char* name) {
    std::string value;
    if (!xmlp::getAttribute(xml, elementStart, name, value)) return std::string();
    return value;
}

// Text of the `<t>` runs inside `[from, to)`, XML-unescaped: shared string
// entries and inline strings are both a sequence of runs.
std::string collectRuns(const std::string& xml, size_t from, size_t to) {
    std::string text;
    size_t pos = from;
    while (true) {
        Span run;
        if (!findElement(xml, "t", pos, run)) break;
        if (run.start >= to) break;
        pos = run.end;
        if (run.selfClosing) continue;
        text += unescapeXml(xml.substr(run.openEnd + 1, run.end - run.openEnd - 5));
    }
    return text;
}

std::string elementValue(const std::string& xml, const Span& cell) {
    Span value;
    if (!findElement(xml, "v", cell.openEnd, value) || value.start >= cell.end) {
        return std::string();
    }
    if (value.selfClosing) return std::string();
    return xml.substr(value.openEnd + 1, value.end - value.openEnd - 5);
}

// ---------------------------------------------------------------------------
// sharedStrings
// ---------------------------------------------------------------------------

bool parseSharedStrings(const std::string& xml, std::vector<std::string>& out) {
    size_t pos = 0;
    while (true) {
        Span item;
        if (!findElement(xml, "si", pos, item)) break;
        pos = item.end;
        out.push_back(collectRuns(xml, item.openEnd + 1, item.end));
    }
    return true;
}

// ---------------------------------------------------------------------------
// styles.xml -> one date/time classification per cell style
// ---------------------------------------------------------------------------

// Builtin formats (ECMA-376). Only the letters matter for classification, but the
// codes are kept in their usual spelling. Ids not listed here are either declared
// in <numFmts> or unknown, and unknown ones are treated as plain.
const char* builtinFormatCode(size_t id) {
    switch (id) {
        case 0: return "General";
        case 1: return "0";
        case 2: return "0.00";
        case 3: return "#,##0";
        case 4: return "#,##0.00";
        case 5: return "$#,##0_);($#,##0)";
        case 6: return "$#,##0_);[Red]($#,##0)";
        case 7: return "$#,##0.00_);($#,##0.00)";
        case 8: return "$#,##0.00_);[Red]($#,##0.00)";
        case 9: return "0%";
        case 10: return "0.00%";
        case 11: return "0.00E+00";
        case 12: return "# ?/?";
        case 13: return "# ??/??";
        case 14: return "mm-dd-yy";
        case 15: return "d-mmm-yy";
        case 16: return "d-mmm";
        case 17: return "mmm-yy";
        case 18: return "h:mm AM/PM";
        case 19: return "h:mm:ss AM/PM";
        case 20: return "h:mm";
        case 21: return "h:mm:ss";
        case 22: return "m/d/yy h:mm";
        case 37: return "#,##0 ;(#,##0)";
        case 38: return "#,##0 ;[Red](#,##0)";
        case 39: return "#,##0.00;(#,##0.00)";
        case 40: return "#,##0.00;[Red](#,##0.00)";
        case 45: return "mm:ss";
        case 46: return "[h]:mm:ss";
        case 47: return "mmss.0";
        case 48: return "##0.0E+0";
        case 49: return "@";
        default: return nullptr;
    }
}

struct StyleTable {
    // Bit flags per style index, so a cell only looks up two booleans.
    std::vector<uint8_t> flags;

    static constexpr uint8_t kDate = 1;
    static constexpr uint8_t kTime = 2;

    bool isDate(size_t style) const {
        return style < flags.size() && (flags[style] & kDate) != 0;
    }
    bool isTime(size_t style) const {
        return style < flags.size() && (flags[style] & kTime) != 0;
    }
};

uint8_t classifyFormat(const std::string& code) {
    const FormatTokens tokens = scanFormatTokens(code);
    uint8_t flags = 0;
    if (tokens.date) flags |= StyleTable::kDate;
    if (tokens.time) flags |= StyleTable::kTime;
    return flags;
}

void parseStyles(const std::string& xml, StyleTable& out) {
    std::map<size_t, std::string> custom;

    // <numFmt numFmtId="164" formatCode="yyyy-mm-dd"/> entries. They are
    // self-closing in practice, so only the opening tag matters and the
    // attributes are read straight from it; <numFmts> is skipped by the tag
    // boundary rule, so no wrapper handling is needed.
    {
        size_t pos = 0;
        while (true) {
            const size_t tag = xmlp::findTagOpen(xml, "numFmt", pos);
            if (tag == std::string::npos) break;
            const size_t openEnd = xml.find('>', tag);
            if (openEnd == std::string::npos) break;
            pos = openEnd + 1;

            const std::string id = attribute(xml, tag, "numFmtId");
            const std::string code = attribute(xml, tag, "formatCode");
            if (!id.empty() && !code.empty()) {
                custom.emplace(static_cast<size_t>(std::strtoul(id.c_str(), nullptr, 10)),
                               unescapeXml(code));
            }
        }
    }

    Span cellXfs;
    if (!findElement(xml, "cellXfs", 0, cellXfs)) return;

    size_t pos = cellXfs.openEnd + 1;
    while (true) {
        Span xf;
        if (!findElement(xml, "xf", pos, xf)) break;
        if (xf.start >= cellXfs.end) break;
        pos = xf.end;

        const std::string id = attribute(xml, xf.start, "numFmtId");
        if (id.empty()) {
            out.flags.push_back(classifyFormat("General"));
            continue;
        }
        const size_t numFmtId = static_cast<size_t>(std::strtoul(id.c_str(), nullptr, 10));
        auto customIt = custom.find(numFmtId);
        if (customIt != custom.end()) {
            out.flags.push_back(classifyFormat(customIt->second));
            continue;
        }
        const char* builtin = builtinFormatCode(numFmtId);
        out.flags.push_back(classifyFormat(builtin ? std::string(builtin) : std::string()));
    }
}

// ---------------------------------------------------------------------------
// Worksheet scanning
// ---------------------------------------------------------------------------

bool parseReference(const std::string& reference, size_t& column, size_t& row) {
    size_t i = 0;
    size_t col = 0;
    while (i < reference.size() && std::isalpha(static_cast<unsigned char>(reference[i]))) {
        col = col * 26 +
              static_cast<size_t>(std::toupper(static_cast<unsigned char>(reference[i])) - 'A' + 1);
        ++i;
    }
    if (col == 0 || i >= reference.size()) return false;

    size_t value = 0;
    while (i < reference.size() && std::isdigit(static_cast<unsigned char>(reference[i]))) {
        value = value * 10 + static_cast<size_t>(reference[i] - '0');
        ++i;
    }
    if (value == 0 || i != reference.size()) return false;

    column = col;
    row = value;
    return true;
}

// Extent of the sheet, taken from the row and cell references the way xlnt
// derives highest_row()/highest_column(). That is what the caps and the
// truncation warnings are measured against.
struct SheetExtent {
    size_t maxRow = 0;
    size_t maxCol = 0;
    bool hasCellA1 = false;
};

void measureSheet(const std::string& xml, SheetExtent& extent) {
    size_t pos = 0;
    size_t lastRow = 0;
    while (true) {
        Span row;
        if (!findElement(xml, "row", pos, row)) break;
        pos = row.end;

        const std::string rowRef = attribute(xml, row.start, "r");
        size_t rowIndex = rowRef.empty()
            ? lastRow + 1
            : static_cast<size_t>(std::strtoul(rowRef.c_str(), nullptr, 10));
        if (rowIndex == 0) rowIndex = lastRow + 1;
        lastRow = rowIndex;
        extent.maxRow = std::max(extent.maxRow, rowIndex);
        if (row.selfClosing) continue;

        size_t cellPos = row.openEnd + 1;
        while (true) {
            Span cell;
            if (!findElement(xml, "c", cellPos, cell)) break;
            if (cell.start >= row.end) break;
            cellPos = cell.end;

            size_t column = 0;
            size_t cellRow = 0;
            const std::string reference = attribute(xml, cell.start, "r");
            if (reference.empty() || !parseReference(reference, column, cellRow)) continue;
            extent.maxCol = std::max(extent.maxCol, column);
            if (column == 1 && cellRow == 1) extent.hasCellA1 = true;
        }
    }
}

// One cell's text, following the same rules as XlsxReader::cellToString.
enum class CellStatus { Ok, Unsupported };

CellStatus cellText(const std::string& xml, const Span& cell, const std::vector<std::string>& shared,
                    const StyleTable& styles, std::string& out) {
    const std::string type = attribute(xml, cell.start, "t");
    const std::string styleRef = attribute(xml, cell.start, "s");

    if (type == "s") {
        const std::string raw = trimmed(elementValue(xml, cell));
        if (raw.empty()) {
            out.clear();
            return CellStatus::Ok;
        }
        if (!std::all_of(raw.begin(), raw.end(),
                         [](char c) { return c >= '0' && c <= '9'; })) {
            return CellStatus::Unsupported;
        }
        const size_t index = static_cast<size_t>(std::strtoul(raw.c_str(), nullptr, 10));
        if (index >= shared.size()) return CellStatus::Unsupported;
        out = shared[index];
        return CellStatus::Ok;
    }

    if (type == "inlineStr") {
        out = collectRuns(xml, cell.openEnd + 1, cell.end);
        return CellStatus::Ok;
    }

    if (type == "str") {
        out = unescapeXml(elementValue(xml, cell));
        // WPS marks embedded images with a DISPIMG formula whose string result is
        // stored here: =DISPIMG("ID_xxx", 1) -> "__IMAGE_CELL__:ID_xxx".
        if (out.find("DISPIMG") != std::string::npos) {
            const size_t idStart = out.find('"');
            if (idStart != std::string::npos) {
                const size_t idEnd = out.find('"', idStart + 1);
                if (idEnd != std::string::npos) {
                    out = "__IMAGE_CELL__:" + out.substr(idStart + 1, idEnd - idStart - 1);
                    return CellStatus::Ok;
                }
            }
            out = "__IMAGE_CELL__";
        }
        return CellStatus::Ok;
    }

    if (type == "b") {
        out = trimmed(elementValue(xml, cell)) == "1" ? "true" : "false";
        return CellStatus::Ok;
    }

    if (type == "e") {
        out = unescapeXml(elementValue(xml, cell));
        return CellStatus::Ok;
    }

    if (!type.empty() && type != "n") {
        // "d" (ISO dates) and anything unknown are handed back to xlnt.
        return CellStatus::Unsupported;
    }

    const std::string raw = trimmed(elementValue(xml, cell));
    if (raw.empty()) {
        out.clear();
        return CellStatus::Ok;
    }

    char* end = nullptr;
    const double number = std::strtod(raw.c_str(), &end);
    if (end == raw.c_str()) {
        return CellStatus::Unsupported;
    }

    const size_t style = styleRef.empty()
        ? 0
        : static_cast<size_t>(std::strtoul(styleRef.c_str(), nullptr, 10));

    if (styles.isDate(style)) {
        out = formatDateSerial(number);
        return CellStatus::Ok;
    }
    if (styles.isTime(style)) {
        out = formatTimeOfDay(number);
        return CellStatus::Ok;
    }
    out = formatDouble(number);
    return CellStatus::Ok;
}

// Maps the requested columns onto 1-based indices, exactly like
// XlsxReader::resolveColumns does for the xlnt path.
bool resolveColumns(const std::vector<std::string>& headers,
                    const std::vector<std::string>& requested, size_t maxCol,
                    std::vector<size_t>& out, std::vector<std::string>& warnings,
                    std::string& error) {
    // `headers` is indexed by 1-based column (index 0 is unused), so the index is
    // the column number.
    std::unordered_map<std::string, size_t> byHeader;
    for (size_t col = 1; col < headers.size(); ++col) {
        const std::string text = trimmed(headers[col]);
        if (!text.empty()) {
            byHeader.emplace(text, col);
        }
    }

    for (const std::string& raw : requested) {
        const std::string item = trimmed(raw);
        if (item.empty()) continue;

        auto headerIt = byHeader.find(item);
        if (headerIt != byHeader.end()) {
            out.push_back(headerIt->second);
            continue;
        }

        size_t first = 0;
        size_t last = 0;
        if (parseColumnReference(item, first, last)) {
            if (first > maxCol) {
                std::ostringstream oss;
                oss << "Column '" << raw << "' lies outside the sheet (last column is "
                    << columnLetters(maxCol) << "); skipped";
                warnings.push_back(oss.str());
                continue;
            }
            for (size_t col = first; col <= std::min(last, maxCol); ++col) {
                out.push_back(col);
            }
            continue;
        }

        warnings.push_back("Column '" + raw + "' matches no header text and is not a valid "
                           "column reference; skipped");
    }

    // De-duplicate, keeping the requested order.
    std::vector<size_t> unique;
    for (size_t col : out) {
        if (std::find(unique.begin(), unique.end(), col) == unique.end()) {
            unique.push_back(col);
        }
    }
    out.swap(unique);

    if (out.empty()) {
        error = "INVALID_OPTIONS|None of the requested columns were found in the sheet";
        return false;
    }
    return true;
}

// Emits one finished row: into the sheet, or to the streaming sink.
bool emitRow(std::vector<CellValue>&& row, bool streaming, const RowBatchSink* sink,
             size_t effectiveBatch, std::vector<std::vector<CellValue>>& batch, SheetData& sheet) {
    if (!streaming) {
        sheet.data.push_back(std::move(row));
        return true;
    }

    batch.push_back(std::move(row));
    if (batch.size() < effectiveBatch) return true;

    const bool keepGoing = (*sink)(std::move(batch));
    batch.clear();
    batch.reserve(effectiveBatch);
    return keepGoing;
}

// Parses `<row>`'s cells into a dense vector of `colCap + 1` texts (index 0 is
// unused), the shape both output paths work with.
bool readRowCells(const std::string& sheetXml, const Span& row, size_t colCap,
                  const std::vector<std::string>& shared, const StyleTable& styles,
                  std::vector<std::string>& out) {
    out.assign(colCap + 1, std::string());
    if (row.selfClosing) return true;

    size_t cellPos = row.openEnd + 1;
    while (true) {
        Span cell;
        if (!findElement(sheetXml, "c", cellPos, cell)) break;
        if (cell.start >= row.end) break;
        cellPos = cell.end;

        size_t column = 0;
        size_t cellRow = 0;
        const std::string reference = attribute(sheetXml, cell.start, "r");
        if (reference.empty() || !parseReference(reference, column, cellRow)) return false;
        if (column > colCap) continue;

        std::string text;
        if (cellText(sheetXml, cell, shared, styles, text) != CellStatus::Ok) return false;
        out[column] = std::move(text);
    }
    return true;
}

// Locates one row and returns its dense texts; a row that is not in the XML
// yields empty cells, which is what the xlnt path reports for it too.
bool readRowTexts(const std::string& sheetXml, size_t wanted, size_t colCap,
                  const std::vector<std::string>& shared, const StyleTable& styles,
                  std::vector<std::string>& out) {
    out.assign(colCap + 1, std::string());
    if (wanted == 0) return true;

    size_t implied = 0;
    size_t pos = 0;
    while (true) {
        Span row;
        if (!findElement(sheetXml, "row", pos, row)) break;
        pos = row.end;

        const std::string rowRef = attribute(sheetXml, row.start, "r");
        const size_t declared = rowRef.empty()
            ? implied + 1
            : static_cast<size_t>(std::strtoul(rowRef.c_str(), nullptr, 10));
        if (declared == 0) return false;
        implied = declared;
        if (declared != wanted) continue;

        return readRowCells(sheetXml, row, colCap, shared, styles, out);
    }
    return true;
}

// One output row from the parsed texts: only the projected columns when a
// projection is active, otherwise every column up to the cap. The texts are
// moved out, so a cell is never copied on its way to the output.
std::vector<CellValue> makeRow(std::vector<std::string>& cells, bool projecting,
                               const std::vector<size_t>& projected, size_t colCap) {
    std::vector<CellValue> row;
    if (projecting) {
        row.reserve(projected.size());
        for (size_t column : projected) {
            CellValue value;
            value.text = std::move(cells[column]);
            row.push_back(std::move(value));
        }
        return row;
    }

    row.resize(colCap);
    for (size_t column = 1; column <= colCap; ++column) {
        row[column - 1].text = std::move(cells[column]);
    }
    return row;
}

// Worksheet names as workbook.xml declares them, in the same order as
// listWorksheetParts().
bool sheetNamesFromArchive(zip_t* archive, std::vector<std::string>& names) {
    std::string workbook;
    std::string ignored;
    if (!pkg::readEntry(archive, "xl/workbook.xml", workbook, ignored)) return false;

    names.clear();
    size_t pos = 0;
    while (true) {
        const size_t sheetTag = xmlp::findTagOpen(workbook, "sheet", pos);
        if (sheetTag == std::string::npos) break;
        pos = sheetTag + 5;

        const std::string name = attribute(workbook, sheetTag, "name");
        if (!name.empty()) {
            names.push_back(unescapeXml(name));
        }
    }
    return true;
}

} // namespace

DirectReadStatus readSheetFromXml(const TemplateSource& source, const ReadOptions& options,
                                  SheetData& sheet, const RowBatchSink* sink, size_t batchSize,
                                  std::vector<std::string>& warnings, std::string& error) {
    // Anything this reader cannot open or model is handed back to xlnt, so a
    // failure is only reported from a path that would have reported the same
    // error before (see DirectReadStatus).
    zip_t* archive = nullptr;
    if (!pkg::openTemplate(source, archive, error)) return DirectReadStatus::Unsupported;
    pkg::ArchiveCloser closer(archive);

    std::string part;
    if (!pkg::findSheetPart(archive, options.sheetName, part, error)) {
        // Let the xlnt path report a missing sheet (it words the error).
        error.clear();
        return DirectReadStatus::Unsupported;
    }

    std::string sheetXml;
    if (!pkg::readEntry(archive, part, sheetXml, error)) {
        error.clear();
        return DirectReadStatus::Unsupported;
    }

    // The worksheet's name as the package declares it: the truncation warnings
    // and the drawing-anchor lookup both need it, and it is not derivable from
    // the part name alone.
    {
        std::vector<std::string> parts;
        std::vector<std::string> names;
        std::string ignored;
        if (pkg::listWorksheetParts(archive, parts, ignored) &&
            sheetNamesFromArchive(archive, names)) {
            for (size_t i = 0; i < parts.size() && i < names.size(); ++i) {
                if (parts[i] == part) {
                    sheet.name = names[i];
                    break;
                }
            }
        }
        if (sheet.name.empty()) sheet.name = options.sheetName;
    }

    std::vector<std::string> shared;
    {
        std::string sharedXml;
        std::string ignored;
        if (pkg::readEntry(archive, "xl/sharedStrings.xml", sharedXml, ignored)) {
            parseSharedStrings(sharedXml, shared);
        }
    }

    StyleTable styles;
    {
        std::string stylesXml;
        std::string ignored;
        if (pkg::readEntry(archive, "xl/styles.xml", stylesXml, ignored)) {
            parseStyles(stylesXml, styles);
        }
    }

    SheetExtent extent;
    measureSheet(sheetXml, extent);
    if (!extent.hasCellA1) {
        // Matches the xlnt path, which treats a sheet without A1 as empty.
        sheet.data.clear();
        return DirectReadStatus::Ok;
    }

    const size_t rowCap = (options.maxRows > 0 && options.maxRows < extent.maxRow)
                              ? options.maxRows
                              : extent.maxRow;
    const size_t colCap = (options.maxCols > 0 && options.maxCols < extent.maxCol)
                              ? options.maxCols
                              : extent.maxCol;
    if (rowCap < extent.maxRow) {
        std::ostringstream oss;
        oss << "Sheet '" << sheet.name << "' truncated to " << rowCap << " of " << extent.maxRow
            << " rows (maxRows)";
        warnings.push_back(oss.str());
    }
    if (colCap < extent.maxCol) {
        std::ostringstream oss;
        oss << "Sheet '" << sheet.name << "' truncated to " << colCap << " of " << extent.maxCol
            << " columns (maxCols)";
        warnings.push_back(oss.str());
    }

    // Projection has to be resolved before the first row is emitted, so the
    // header row is read on its own here instead of during the main pass.
    const bool needProjection = !options.columns.empty();
    std::vector<size_t> projected;
    if (needProjection) {
        std::vector<std::string> header;
        if (!readRowTexts(sheetXml, options.headerRow + 1, colCap, shared, styles, header)) {
            return DirectReadStatus::Unsupported;
        }
        if (!resolveColumns(header, options.columns, colCap, projected, warnings, error)) {
            return DirectReadStatus::Failed;
        }
        for (size_t column : projected) {
            sheet.headers.push_back(trimmed(header[column]));
        }
        sheet.projectedColumns = projected;
    }

    const bool streaming = sink != nullptr;
    const size_t effectiveBatch = batchSize > 0 ? batchSize : 1000;
    std::vector<std::vector<CellValue>> batch;
    if (streaming) {
        batch.reserve(effectiveBatch);
    }
    bool stopped = false;

    size_t rowIndex = 0;
    size_t pos = 0;
    const size_t projectedCount = needProjection ? projected.size() : colCap;
    const auto emitEmptyRow = [&]() {
        std::vector<CellValue> empty(projectedCount, CellValue());
        return emitRow(std::move(empty), streaming, sink, effectiveBatch, batch, sheet);
    };

    while (rowIndex < rowCap && !stopped) {
        Span row;
        if (!findElement(sheetXml, "row", pos, row)) break;
        pos = row.end;

        const std::string rowRef = attribute(sheetXml, row.start, "r");
        const size_t declared = rowRef.empty()
            ? rowIndex + 1
            : static_cast<size_t>(std::strtoul(rowRef.c_str(), nullptr, 10));
        if (declared == 0) {
            return DirectReadStatus::Unsupported;
        }

        // Rows may be sparse: everything between the previous one and this one is
        // an empty row. Only the requested window is emitted.
        const size_t rowNumber = std::min(declared, rowCap);
        while (rowIndex + 1 < rowNumber && !stopped) {
            ++rowIndex;
            stopped = !emitEmptyRow();
        }
        if (rowIndex + 1 > rowNumber) {
            // Out-of-order rows: xlnt sorts them, this reader hands the file back.
            return DirectReadStatus::Unsupported;
        }
        ++rowIndex;

        std::vector<std::string> cells;
        if (!readRowCells(sheetXml, row, colCap, shared, styles, cells)) {
            return DirectReadStatus::Unsupported;
        }
        // `cells` is consumed here, which is what keeps the full read from
        // copying every cell text twice.
        stopped = !emitRow(makeRow(cells, needProjection, projected, colCap), streaming, sink,
                           effectiveBatch, batch, sheet);
    }

    // Fill in the rest of the window when the XML ended early.
    while (rowIndex < rowCap && !stopped) {
        ++rowIndex;
        stopped = !emitEmptyRow();
    }

    if (streaming && !stopped && !batch.empty()) {
        (*sink)(std::move(batch));
    }

    return DirectReadStatus::Ok;
}

bool readSheetNames(const TemplateSource& source, std::vector<std::string>& names,
                    std::string& error) {
    zip_t* archive = nullptr;
    if (!pkg::openTemplate(source, archive, error)) return false;
    pkg::ArchiveCloser closer(archive);
    return sheetNamesFromArchive(archive, names);
}

} // namespace baja_xlsx
