#include "xlsx_writer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace baja_xlsx {

namespace {

const char* kXmlDeclaration =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n";

bool isXmlIllegal(unsigned char c) {
    // XML 1.0 forbids most C0 controls; tab/newline/carriage return are allowed.
    return (c < 0x20 && c != '\t' && c != '\n' && c != '\r');
}

void appendEscaped(std::string& out, const std::string& text, bool attribute) {
    out.reserve(out.size() + text.size() + 8);
    for (char raw : text) {
        const unsigned char c = static_cast<unsigned char>(raw);
        if (isXmlIllegal(c)) {
            continue; // dropped rather than written as a corrupt byte
        }
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += attribute ? "&quot;" : "\""; break;
            case '\'': out += attribute ? "&apos;" : "'"; break;
            default: out.push_back(static_cast<char>(c)); break;
        }
    }
}

long long daysFromCivil(long long year, unsigned month, unsigned day) {
    year -= month <= 2;
    const long long era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153u * (month + (month > 2 ? -3u : 9u)) + 2u) / 5u + day - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097ll + static_cast<long long>(doe) - 719468ll;
}

// Excel's day 0 is 1899-12-30 (the 1900 leap year bug is already baked into
// that epoch for every date from 1900-03-01 onwards).
const long long kExcelEpochDays = daysFromCivil(1899, 12, 30);

size_t utf8Length(const std::string& text) {
    size_t count = 0;
    for (unsigned char c : text) {
        if ((c & 0xC0) != 0x80) ++count; // count non-continuation bytes
    }
    return count;
}

bool needsPreserveSpace(const std::string& text) {
    if (text.empty()) return false;
    const char first = text.front();
    const char last = text.back();
    if (first == ' ' || first == '\t' || first == '\n' || first == '\r') return true;
    if (last == ' ' || last == '\t' || last == '\n' || last == '\r') return true;
    return text.find('\n') != std::string::npos || text.find('\t') != std::string::npos;
}

std::string buildContentTypes() {
    std::string xml = kXmlDeclaration;
    xml += "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">";
    xml += "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>";
    xml += "<Default Extension=\"xml\" ContentType=\"application/xml\"/>";
    xml += "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>";
    xml += "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>";
    xml += "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>";
    xml += "</Types>";
    return xml;
}

std::string buildRootRels() {
    std::string xml = kXmlDeclaration;
    xml += "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
    xml += "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>";
    xml += "</Relationships>";
    return xml;
}

std::string buildWorkbook(const std::string& sheetName) {
    std::string xml = kXmlDeclaration;
    xml += "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
           "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">";
    xml += "<sheets><sheet name=\"";
    xml += escapeXmlAttribute(sheetName);
    xml += "\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>";
    return xml;
}

std::string buildWorkbookRels() {
    std::string xml = kXmlDeclaration;
    xml += "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
    xml += "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>";
    xml += "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>";
    xml += "</Relationships>";
    return xml;
}

} // namespace

size_t SheetStyles::styleForFormat(const std::string& formatCode, const std::string& align) {
    if (formatCode.empty() && align.empty()) {
        return 0;
    }

    size_t numFmtId = 0;
    if (!formatCode.empty()) {
        auto formatIt = formatIds.find(formatCode);
        if (formatIt == formatIds.end()) {
            numFmtId = firstNumFmtId + numFmtCodes.size();
            formatIds.emplace(formatCode, numFmtId);
            numFmtCodes.push_back(formatCode);
        } else {
            numFmtId = formatIt->second;
        }
    }

    std::ostringstream xf;
    xf << "<xf numFmtId=\"" << numFmtId
       << "\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"";
    if (numFmtId != 0) {
        xf << " applyNumberFormat=\"1\"";
    }
    if (align.empty()) {
        xf << "/>";
    } else {
        xf << " applyAlignment=\"1\"><alignment horizontal=\""
           << escapeXmlAttribute(align) << "\"/></xf>";
    }
    const std::string body = xf.str();

    size_t index = 0;
    auto xfIt = xfIds.find(body);
    if (xfIt != xfIds.end()) {
        index = xfIt->second;
    } else {
        index = xfBaseIndex + xfBodies.size();
        xfIds.emplace(body, index);
        xfBodies.push_back(body);
    }
    if (!formatCode.empty()) {
        formatStyleIds[formatCode] = index;
    }
    return index;
}

SheetStyles buildSheetStyles(const WritePlan& plan, size_t firstNumFmtId, size_t xfBaseIndex) {
    SheetStyles styles;
    styles.firstNumFmtId = firstNumFmtId;
    styles.xfBaseIndex = xfBaseIndex;
    styles.styleIndexByColumn.assign(plan.columns.size(), 0);
    styles.explicitFormat.assign(plan.columns.size(), false);

    for (size_t i = 0; i < plan.columns.size(); ++i) {
        const WriteColumn& column = plan.columns[i];
        styles.explicitFormat[i] = !column.numberFormat.empty();
        if (column.numberFormat.empty() && column.align.empty()) {
            continue;
        }
        styles.styleIndexByColumn[i] = styles.styleForFormat(column.numberFormat, column.align);
    }

    // Date formats are resolved from the values while rows stream, so they are
    // allocated up front: a cell that needs one only picks an index that is
    // already there, which keeps both the "styles first" (template) and
    // "styles last" (new workbook) orders correct.
    styles.dateStyleIndex = styles.styleForFormat("yyyy-mm-dd");
    styles.dateTimeStyleIndex = styles.styleForFormat("yyyy-mm-dd hh:mm:ss");
    return styles;
}

std::string escapeXmlText(const std::string& text) {
    std::string out;
    appendEscaped(out, text, false);
    return out;
}

std::string escapeXmlAttribute(const std::string& text) {
    std::string out;
    appendEscaped(out, text, true);
    return out;
}

std::string columnLetters(size_t index) {
    std::string name;
    while (index > 0) {
        const size_t rem = (index - 1) % 26;
        name.insert(name.begin(), static_cast<char>('A' + static_cast<int>(rem)));
        index = (index - 1) / 26;
    }
    return name;
}

std::string formatNumber(double value) {
    if (!std::isfinite(value)) {
        return "0";
    }
    char buffer[64];
    if (value == std::floor(value) && std::fabs(value) < 1e15) {
        std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
        return std::string(buffer);
    }
    for (int precision = 15; precision <= 17; ++precision) {
        std::snprintf(buffer, sizeof(buffer), "%.*g", precision, value);
        if (std::strtod(buffer, nullptr) == value) {
            break;
        }
    }
    return std::string(buffer);
}

double toExcelSerial(int year, int month, int day, int hour, int minute, int second,
                     int millisecond) {
    const long long days = daysFromCivil(year, static_cast<unsigned>(month),
                                         static_cast<unsigned>(day)) - kExcelEpochDays;
    const double fraction =
        (hour * 3600.0 + minute * 60.0 + second + millisecond / 1000.0) / 86400.0;
    return static_cast<double>(days) + fraction;
}

bool isValidSheetName(const std::string& name, std::string& reason) {
    reason.clear();
    if (name.empty()) {
        reason = "sheet name must not be empty";
        return false;
    }
    if (utf8Length(name) > 31) {
        reason = "sheet name must be at most 31 characters";
        return false;
    }
    const char* forbidden = "[]:*?/\\";
    if (name.find_first_of(forbidden) != std::string::npos) {
        reason = "sheet name must not contain any of [ ] : * ? / \\";
        return false;
    }
    if (name.front() == '\'' || name.back() == '\'') {
        reason = "sheet name must not start or end with an apostrophe";
        return false;
    }
    return true;
}

void appendCellXml(std::string& out, const std::string& letters, size_t row,
                   const WriteCell& cell, size_t styleIndex) {
    if (cell.kind == WriteCell::Kind::Empty) {
        return; // an absent <c> is an empty cell
    }

    out += "<c r=\"";
    out += letters;
    out += std::to_string(row);
    out += "\"";

    if (cell.kind == WriteCell::Kind::Text) {
        out += " t=\"inlineStr\"";
    } else if (cell.kind == WriteCell::Kind::Boolean) {
        out += " t=\"b\"";
    }
    if (styleIndex != 0) {
        out += " s=\"";
        out += std::to_string(styleIndex);
        out += "\"";
    }
    out += ">";

    switch (cell.kind) {
        case WriteCell::Kind::Number:
            out += "<v>";
            out += formatNumber(cell.number);
            out += "</v>";
            break;
        case WriteCell::Kind::Boolean:
            out += "<v>";
            out += cell.boolean ? "1" : "0";
            out += "</v>";
            break;
        case WriteCell::Kind::Text:
            out += "<is><t";
            if (needsPreserveSpace(cell.text)) {
                out += " xml:space=\"preserve\"";
            }
            out += ">";
            appendEscaped(out, cell.text, false);
            out += "</t></is>";
            break;
        case WriteCell::Kind::Empty:
            break;
    }
    out += "</c>";
}

std::string buildStylesXml(const SheetStyles& styles) {
    std::string xml = kXmlDeclaration;
    xml += "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">";

    if (!styles.numFmtCodes.empty()) {
        xml += "<numFmts count=\"" + std::to_string(styles.numFmtCodes.size()) + "\">";
        for (size_t i = 0; i < styles.numFmtCodes.size(); ++i) {
            xml += "<numFmt numFmtId=\"" + std::to_string(styles.firstNumFmtId + i) +
                   "\" formatCode=\"" + escapeXmlAttribute(styles.numFmtCodes[i]) + "\"/>";
        }
        xml += "</numFmts>";
    }

    xml += "<fonts count=\"1\"><font><sz val=\"11\"/><name val=\"Calibri\"/></font></fonts>";
    xml += "<fills count=\"2\"><fill><patternFill patternType=\"none\"/></fill>"
           "<fill><patternFill patternType=\"gray125\"/></fill></fills>";
    xml += "<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders>";
    xml += "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>";
    xml += "<cellXfs count=\"" + std::to_string(styles.xfBodies.size() + 1) + "\">";
    xml += "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>";
    for (const std::string& xf : styles.xfBodies) {
        xml += xf;
    }
    xml += "</cellXfs>";
    xml += "<cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles>";
    xml += "</styleSheet>";
    return xml;
}

SheetRowStream::SheetRowStream(const WritePlan& plan, RowSource& source, SheetStyles& styles)
    : plan_(plan), source_(source), styles_(styles) {
    letters_.resize(plan.columns.size());
    for (size_t i = 0; i < plan.columns.size(); ++i) {
        letters_[i] = columnLetters(i + 1);
    }
    totalRows_ = source.rowCount() + (plan.includeHeader ? 1 : 0);
    row_.reserve(plan.columns.size());
}

bool SheetRowStream::next(std::string& chunk) {
    chunk.clear();
    if (emitted_ >= totalRows_) {
        return false;
    }

    const size_t columnCount = plan_.columns.size();

    if (plan_.includeHeader && emitted_ == 0) {
        chunk += "<row r=\"1\">";
        for (size_t i = 0; i < columnCount; ++i) {
            WriteCell cell;
            cell.kind = WriteCell::Kind::Text;
            cell.text = plan_.columns[i].header;
            appendCellXml(chunk, letters_[i], 1, cell, 0);
        }
        chunk += "</row>";
        ++emitted_;
        return true;
    }

    const size_t dataIndex = emitted_ - (plan_.includeHeader ? 1 : 0);
    if (dataIndex >= source_.rowCount()) {
        emitted_ = totalRows_;
        return false;
    }

    row_.clear();
    if (!source_.nextRow(row_)) {
        row_.assign(columnCount, WriteCell());
    }

    const size_t rowNumber = emitted_ + 1;
    chunk += "<row r=\"" + std::to_string(rowNumber) + "\">";
    for (size_t i = 0; i < columnCount; ++i) {
        const WriteCell& cell = i < row_.size() ? row_[i] : WriteCell();
        size_t style = styles_.styleIndexByColumn[i];
        if (cell.isDate && !styles_.explicitFormat[i]) {
            // Excel would show the raw serial without this.
            const bool withTime = (cell.number - std::floor(cell.number)) > 1e-9;
            style = withTime ? styles_.dateTimeStyleIndex : styles_.dateStyleIndex;
        }
        appendCellXml(chunk, letters_[i], rowNumber, cell, style);
    }
    chunk += "</row>";
    ++emitted_;
    return true;
}

bool writeNewWorkbook(const WritePlan& plan, RowSource& source,
                      zipio::ZipWriter::Compression compression,
                      std::vector<uint8_t>& out, std::string& error) {
    const size_t columnCount = plan.columns.size();
    if (columnCount == 0) {
        error = "INVALID_OPTIONS|At least one column is required";
        return false;
    }
    if (!isValidSheetName(plan.sheetName, error)) {
        error = "INVALID_OPTIONS|" + error;
        return false;
    }

    SheetStyles styles = buildSheetStyles(plan);

    zipio::ZipWriter writer(out);

    if (!writer.addEntry("[Content_Types].xml", buildContentTypes(), compression, error) ||
        !writer.addEntry("_rels/.rels", buildRootRels(), compression, error) ||
        !writer.addEntry("xl/workbook.xml", buildWorkbook(plan.sheetName), compression, error) ||
        !writer.addEntry("xl/_rels/workbook.xml.rels", buildWorkbookRels(), compression, error)) {
        return false;
    }

    const bool hasColumns = std::any_of(plan.columns.begin(), plan.columns.end(),
                                        [](const WriteColumn& c) { return c.width > 0; });

    SheetRowStream rows(plan, source, styles);
    bool preambleSent = false;
    bool closingSent = false;

    const bool sheetOk = writer.addStreamedEntry(
        "xl/worksheets/sheet1.xml",
        [&](std::string& chunk) -> bool {
            if (!preambleSent) {
                preambleSent = true;
                chunk += kXmlDeclaration;
                chunk += "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">";
                if (plan.freezeHeader) {
                    chunk += "<sheetViews><sheetView tabSelected=\"1\" workbookViewId=\"0\">"
                             "<pane ySplit=\"1\" topLeftCell=\"A2\" activePane=\"bottomLeft\" "
                             "state=\"frozen\"/></sheetView></sheetViews>";
                }
                chunk += "<sheetFormatPr defaultRowHeight=\"15\"/>";
                if (hasColumns) {
                    chunk += "<cols>";
                    for (size_t i = 0; i < columnCount; ++i) {
                        if (plan.columns[i].width <= 0) continue;
                        chunk += "<col min=\"" + std::to_string(i + 1) + "\" max=\"" +
                                 std::to_string(i + 1) + "\" width=\"" +
                                 formatNumber(plan.columns[i].width) +
                                 "\" customWidth=\"1\"/>";
                    }
                    chunk += "</cols>";
                }
                chunk += "<sheetData>";
                return true;
            }
            if (rows.next(chunk)) {
                return true;
            }
            if (!closingSent) {
                closingSent = true;
                chunk += "</sheetData></worksheet>";
                return true;
            }
            return false;
        },
        compression, error);

    if (!sheetOk) {
        return false;
    }
    if (!closingSent) {
        error = "WRITE_FAILED|Worksheet generation stopped early";
        return false;
    }
    return writer.addEntry("xl/styles.xml", buildStylesXml(styles), compression, error) &&
           writer.finish(error);
}

} // namespace baja_xlsx
