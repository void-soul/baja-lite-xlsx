#include "xlsx_patch.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>

#include "xml_parsers.h"
#include "zip_reader.h"

namespace baja_xlsx {

namespace {

// ---------------------------------------------------------------------------
// Package access
// ---------------------------------------------------------------------------

std::string normalizeName(const std::string& name) {
    std::string out = name;
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}

// Reads a part, tolerating backslash entry names (WPS writes those).
bool readEntry(zip_t* archive, const std::string& wanted, std::string& out, std::string& error) {
    std::vector<uint8_t> data;
    std::string readError;
    if (zipio::readFile(archive, wanted, data, zipio::kMaxEntryBytes, readError)) {
        out.assign(data.begin(), data.end());
        return true;
    }

    const zip_int64_t count = zip_get_num_entries(archive, 0);
    for (zip_int64_t i = 0; i < count; ++i) {
        const char* rawName = zip_get_name(archive, i, 0);
        if (!rawName || normalizeName(rawName) != wanted) continue;
        std::vector<uint8_t> raw;
        std::string secondError;
        if (zipio::readFile(archive, rawName, raw, zipio::kMaxEntryBytes, secondError)) {
            out.assign(raw.begin(), raw.end());
            return true;
        }
        error = secondError;
        return false;
    }
    error = "Missing part in the template: " + wanted;
    return false;
}

// Full "Id" -> "Target" pairs of a .rels document. xmlp::parseRelationships only
// keeps the file name, which is not enough to locate a part.
std::map<std::string, std::string> parseRelationshipTargets(const std::string& xml) {
    std::map<std::string, std::string> targets;
    size_t pos = 0;
    while (true) {
        const size_t elementStart = xmlp::findTagOpen(xml, "Relationship", pos);
        if (elementStart == std::string::npos) break;
        pos = elementStart + 12;

        std::string id;
        std::string target;
        if (!xmlp::getAttribute(xml, elementStart, "Id", id)) continue;
        if (!xmlp::getAttribute(xml, elementStart, "Target", target)) continue;
        targets.emplace(id, target);
    }
    return targets;
}

bool entryExists(zip_t* archive, const std::string& name) {
    if (zip_name_locate(archive, name.c_str(), 0) >= 0) return true;
    const zip_int64_t count = zip_get_num_entries(archive, 0);
    for (zip_int64_t i = 0; i < count; ++i) {
        const char* rawName = zip_get_name(archive, i, 0);
        if (rawName && normalizeName(rawName) == name) return true;
    }
    return false;
}

std::string baseName(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

// Turns a relationship target into an entry name that actually exists. Targets
// are relative to the part that declares them, but packages in the wild are not
// always consistent about it, so a few candidates are tried before giving up.
bool resolvePartName(zip_t* archive, const std::string& target, std::string& out) {
    std::string cleaned = normalizeName(target);
    if (!cleaned.empty() && cleaned[0] == '/') {
        cleaned = cleaned.substr(1);
    }

    std::vector<std::string> candidates;
    candidates.push_back(cleaned);
    if (cleaned.rfind("xl/", 0) != 0) {
        candidates.push_back("xl/" + cleaned);
    }
    if (cleaned.rfind("worksheets/", 0) == 0) {
        candidates.push_back("xl/" + cleaned);
    } else {
        candidates.push_back("xl/worksheets/" + baseName(cleaned));
    }

    for (const std::string& candidate : candidates) {
        if (!candidate.empty() && entryExists(archive, candidate)) {
            out = candidate;
            return true;
        }
    }

    // Last resort: a unique entry whose name ends with "/<base name>".
    const std::string base = baseName(cleaned);
    std::string match;
    const zip_int64_t count = zip_get_num_entries(archive, 0);
    for (zip_int64_t i = 0; i < count; ++i) {
        const char* rawName = zip_get_name(archive, i, 0);
        if (!rawName) continue;
        const std::string name = normalizeName(rawName);
        if (baseName(name) != base) continue;
        if (!match.empty()) return false; // ambiguous
        match = name;
    }
    if (match.empty()) return false;
    out = match;
    return true;
}

// Maps a sheet name (empty = first sheet) to its part inside the package.
bool findSheetPart(zip_t* archive, const std::string& sheetName, std::string& part,
                   std::string& error) {
    std::string workbook;
    if (!readEntry(archive, "xl/workbook.xml", workbook, error)) return false;
    std::string rels;
    if (!readEntry(archive, "xl/_rels/workbook.xml.rels", rels, error)) return false;

    const std::map<std::string, std::string> relationships = parseRelationshipTargets(rels);

    size_t pos = 0;
    std::string firstName;
    std::string firstTarget;
    while (true) {
        const size_t elementStart = xmlp::findTagOpen(workbook, "sheet", pos);
        if (elementStart == std::string::npos) break;
        pos = elementStart + 5;

        std::string name;
        std::string relationshipId;
        xmlp::getAttribute(workbook, elementStart, "name", name);
        xmlp::getAttribute(workbook, elementStart, "r:id", relationshipId);

        auto it = relationships.find(relationshipId);
        if (relationshipId.empty() || it == relationships.end()) continue;

        if (firstName.empty()) {
            firstName = name;
            firstTarget = it->second;
        }
        if (!sheetName.empty() && name == sheetName) {
            if (!resolvePartName(archive, it->second, part)) {
                error = "WRITE_FAILED|Sheet part for '" + name + "' not found in the package";
                return false;
            }
            return true;
        }
    }

    if (sheetName.empty() && !firstTarget.empty()) {
        if (resolvePartName(archive, firstTarget, part)) return true;
        error = "WRITE_FAILED|The first sheet's part was not found in the package";
        return false;
    }

    error = "SHEET_NOT_FOUND|Sheet \"" + sheetName + "\" not found in the template";
    return false;
}

// ---------------------------------------------------------------------------
// styles.xml patching
// ---------------------------------------------------------------------------

bool isAllDigits(const std::string& text) {
    return !text.empty() &&
           std::all_of(text.begin(), text.end(),
                       [](char c) { return c >= '0' && c <= '9'; });
}

size_t countTagOccurrences(const std::string& xml, const char* tag, size_t from, size_t to) {
    size_t count = 0;
    size_t pos = from;
    while (true) {
        const size_t at = xmlp::findTagOpen(xml, tag, pos);
        if (at == std::string::npos || at >= to) break;
        ++count;
        pos = at + 1;
    }
    return count;
}

// Replaces the value of `attribute` on the element starting at `elementStart`.
bool setCountAttribute(std::string& xml, size_t elementStart, const char* attribute,
                       size_t newValue) {
    const std::string doubleQuoted = std::string(attribute) + "=\"";
    size_t at = xml.find(doubleQuoted, elementStart);
    if (at != std::string::npos) {
        const size_t valueStart = at + doubleQuoted.size();
        const size_t valueEnd = xml.find('"', valueStart);
        if (valueEnd == std::string::npos) return false;
        xml.replace(valueStart, valueEnd - valueStart, std::to_string(newValue));
        return true;
    }

    const std::string singleQuoted = std::string(attribute) + "='";
    at = xml.find(singleQuoted, elementStart);
    if (at == std::string::npos) return false;
    const size_t valueStart = at + singleQuoted.size();
    const size_t valueEnd = xml.find('\'', valueStart);
    if (valueEnd == std::string::npos) return false;
    xml.replace(valueStart, valueEnd - valueStart, std::to_string(newValue));
    return true;
}

// Where new styles may be appended: the first free custom numFmt id and the
// number of existing cellXfs entries.
struct TemplateStyleBase {
    size_t firstNumFmtId = 164;
    size_t xfBaseIndex = 1;
};

bool readTemplateStyleBase(zip_t* archive, TemplateStyleBase& base, std::string& stylesXml,
                           std::string& error) {
    if (!readEntry(archive, "xl/styles.xml", stylesXml, error)) {
        error = "WRITE_FAILED|The template has no xl/styles.xml, so number formats cannot be "
                "added (" + error + ")";
        return false;
    }

    const size_t cellXfsStart = xmlp::findTagOpen(stylesXml, "cellXfs", 0);
    if (cellXfsStart == std::string::npos) {
        error = "WRITE_FAILED|The template styles.xml has no cellXfs element";
        return false;
    }

    std::string countText;
    if (xmlp::getAttribute(stylesXml, cellXfsStart, "count", countText) &&
        isAllDigits(countText)) {
        base.xfBaseIndex = static_cast<size_t>(std::stoul(countText));
    } else {
        const size_t closeXfs = stylesXml.find("</cellXfs>", cellXfsStart);
        base.xfBaseIndex = countTagOccurrences(stylesXml, "xf", cellXfsStart,
                                               closeXfs == std::string::npos
                                                   ? stylesXml.size()
                                                   : closeXfs);
    }

    size_t maxNumFmtId = 163;
    size_t pos = 0;
    while (true) {
        const size_t at = xmlp::findTagOpen(stylesXml, "numFmt", pos);
        if (at == std::string::npos) break;
        pos = at + 6;
        std::string idText;
        if (xmlp::getAttribute(stylesXml, at, "numFmtId", idText) && isAllDigits(idText)) {
            maxNumFmtId = std::max(maxNumFmtId, static_cast<size_t>(std::stoul(idText)));
        }
    }
    base.firstNumFmtId = maxNumFmtId + 1;
    return true;
}

bool applyTemplateStyles(std::string& stylesXml, const SheetStyles& styles, std::string& error) {
    const size_t cellXfsStart = xmlp::findTagOpen(stylesXml, "cellXfs", 0);
    if (cellXfsStart == std::string::npos) {
        error = "WRITE_FAILED|The template styles.xml has no cellXfs element";
        return false;
    }

    if (!styles.xfBodies.empty()) {
        const size_t closeXfs = stylesXml.find("</cellXfs>", cellXfsStart);
        if (closeXfs == std::string::npos) {
            error = "WRITE_FAILED|The template styles.xml has no </cellXfs>";
            return false;
        }
        std::string inserted;
        for (const std::string& body : styles.xfBodies) {
            inserted += body;
        }
        stylesXml.insert(closeXfs, inserted);

        std::string countText;
        size_t existing = countTagOccurrences(stylesXml, "xf", cellXfsStart, closeXfs);
        if (xmlp::getAttribute(stylesXml, cellXfsStart, "count", countText) &&
            isAllDigits(countText)) {
            existing = static_cast<size_t>(std::stoul(countText));
        }
        setCountAttribute(stylesXml, cellXfsStart, "count",
                          existing + styles.xfBodies.size());
    }

    if (styles.numFmtCodes.empty()) {
        return true;
    }

    std::string inserted;
    for (size_t i = 0; i < styles.numFmtCodes.size(); ++i) {
        inserted += "<numFmt numFmtId=\"" + std::to_string(styles.firstNumFmtId + i) +
                    "\" formatCode=\"" + escapeXmlAttribute(styles.numFmtCodes[i]) + "\"/>";
    }

    const size_t numFmtsStart = xmlp::findTagOpen(stylesXml, "numFmts", 0);
    if (numFmtsStart != std::string::npos) {
        const size_t closeNumFmts = stylesXml.find("</numFmts>", numFmtsStart);
        if (closeNumFmts == std::string::npos) {
            error = "WRITE_FAILED|The template styles.xml has no </numFmts>";
            return false;
        }
        std::string countText;
        size_t existing = countTagOccurrences(stylesXml, "numFmt", numFmtsStart, closeNumFmts);
        if (xmlp::getAttribute(stylesXml, numFmtsStart, "count", countText) &&
            isAllDigits(countText)) {
            existing = static_cast<size_t>(std::stoul(countText));
        }
        stylesXml.insert(closeNumFmts, inserted);
        setCountAttribute(stylesXml, numFmtsStart, "count",
                          existing + styles.numFmtCodes.size());
        return true;
    }

    // Schema order requires numFmts first: insert right after <styleSheet>.
    const size_t styleSheetStart = xmlp::findTagOpen(stylesXml, "styleSheet", 0);
    if (styleSheetStart == std::string::npos) {
        error = "WRITE_FAILED|The template styles.xml has no styleSheet element";
        return false;
    }
    const size_t openEnd = stylesXml.find('>', styleSheetStart);
    if (openEnd == std::string::npos) {
        error = "WRITE_FAILED|Malformed styleSheet element";
        return false;
    }
    stylesXml.insert(openEnd + 1,
                     "<numFmts count=\"" + std::to_string(styles.numFmtCodes.size()) + "\">" +
                         inserted + "</numFmts>");
    return true;
}

// ---------------------------------------------------------------------------
// Sheet XML patching
// ---------------------------------------------------------------------------

struct CellRef {
    size_t row = 0;
    size_t column = 0;
    std::string letters;
};

bool parseCellReference(const std::string& reference, CellRef& out) {
    size_t i = 0;
    std::string letters;
    while (i < reference.size() && std::isalpha(static_cast<unsigned char>(reference[i]))) {
        letters.push_back(static_cast<char>(
            std::toupper(static_cast<unsigned char>(reference[i]))));
        ++i;
    }
    if (letters.empty() || i >= reference.size()) return false;

    size_t row = 0;
    while (i < reference.size() && std::isdigit(static_cast<unsigned char>(reference[i]))) {
        row = row * 10 + static_cast<size_t>(reference[i] - '0');
        ++i;
    }
    if (row == 0 || i != reference.size()) return false;

    size_t column = 0;
    for (char c : letters) {
        column = column * 26 + static_cast<size_t>(c - 'A' + 1);
    }
    out.row = row;
    out.column = column;
    out.letters = letters;
    return true;
}

// [start, end) of the existing <c r="REF"> element, if any.
bool findCellSpan(const std::string& xml, const std::string& reference, size_t& start,
                  size_t& end) {
    start = xml.find("<c r=\"" + reference + "\"");
    if (start == std::string::npos) return false;

    const size_t openEnd = xml.find('>', start);
    if (openEnd == std::string::npos) return false;

    if (xml[openEnd - 1] == '/') {
        end = openEnd + 1;
        return true;
    }
    const size_t close = xml.find("</c>", openEnd);
    if (close == std::string::npos) return false;
    end = close + 4;
    return true;
}

// Inserts `cellXml` into its row, keeping column order; creates the row when it
// does not exist yet.
bool insertCellIntoSheet(std::string& xml, const CellRef& ref, const std::string& cellXml) {
    const std::string rowNeedle = "<row r=\"" + std::to_string(ref.row) + "\"";
    const size_t rowStart = xml.find(rowNeedle);

    if (rowStart != std::string::npos) {
        const size_t rowOpenEnd = xml.find('>', rowStart);
        if (rowOpenEnd == std::string::npos) return false;

        if (xml[rowOpenEnd - 1] == '/') { // <row .../> -> expand in place
            xml.replace(rowOpenEnd - 1, 2, ">" + cellXml + "</row>");
            return true;
        }

        const size_t rowEnd = xml.find("</row>", rowOpenEnd);
        if (rowEnd == std::string::npos) return false;

        size_t insertAt = rowEnd;
        size_t scan = rowOpenEnd + 1;
        while (true) {
            const size_t cellStart = xml.find("<c r=\"", scan);
            if (cellStart == std::string::npos || cellStart >= rowEnd) break;
            std::string reference;
            CellRef existing;
            if (xmlp::getAttribute(xml, cellStart, "r", reference) &&
                parseCellReference(reference, existing) && existing.column > ref.column) {
                insertAt = cellStart;
                break;
            }
            scan = cellStart + 6;
        }
        xml.insert(insertAt, cellXml);
        return true;
    }

    // No such row: create it before the first row that sorts after it.
    const std::string newRow =
        "<row r=\"" + std::to_string(ref.row) + "\">" + cellXml + "</row>";
    size_t scan = 0;
    while (true) {
        const size_t at = xmlp::findTagOpen(xml, "row", scan);
        if (at == std::string::npos) break;
        scan = at + 3;
        std::string numberText;
        if (!xmlp::getAttribute(xml, at, "r", numberText) || !isAllDigits(numberText)) continue;
        if (static_cast<size_t>(std::stoul(numberText)) > ref.row) {
            xml.insert(at, newRow);
            return true;
        }
    }

    const size_t sheetDataClose = xml.find("</sheetData>");
    if (sheetDataClose == std::string::npos) return false;
    xml.insert(sheetDataClose, newRow);
    return true;
}

size_t styleIndexForUpdate(const CellUpdate& update, const SheetStyles& styles) {
    std::string code = update.numberFormat;
    if (code.empty() && update.value.isDate) {
        const bool withTime = (update.value.number - std::floor(update.value.number)) > 1e-9;
        code = withTime ? "yyyy-mm-dd hh:mm:ss" : "yyyy-mm-dd";
    }
    if (code.empty()) return 0;

    auto it = styles.formatStyleIds.find(code);
    return it == styles.formatStyleIds.end() ? 0 : it->second;
}

// Rewrites the listed cells inside one worksheet document.
bool patchSheetCells(std::string& sheetXml, const std::vector<CellUpdate>& updates,
                     const SheetStyles& styles, std::string& error) {
    for (const CellUpdate& update : updates) {
        CellRef ref;
        if (!parseCellReference(update.cell, ref)) {
            error = "INVALID_OPTIONS|Invalid cell reference \"" + update.cell + "\"";
            return false;
        }

        size_t start = 0;
        size_t end = 0;
        const bool exists = findCellSpan(sheetXml, update.cell, start, end);

        size_t styleIndex = styleIndexForUpdate(update, styles);
        if (styleIndex == 0 && exists) {
            // Keep whatever style the cell already had.
            std::string existingStyle;
            if (xmlp::getAttribute(sheetXml, start, "s", existingStyle) &&
                isAllDigits(existingStyle)) {
                styleIndex = static_cast<size_t>(std::stoul(existingStyle));
            }
        }

        std::string cellXml;
        appendCellXml(cellXml, ref.letters, ref.row, update.value, styleIndex);

        if (exists) {
            sheetXml.replace(start, end - start, cellXml);
        } else if (!insertCellIntoSheet(sheetXml, ref, cellXml)) {
            error = "WRITE_FAILED|Could not insert cell " + update.cell;
            return false;
        }
    }
    return true;
}

// Removes a stale <dimension> so Excel does not show a wrong used range.
void dropDimensionElement(std::string& xml) {
    const size_t start = xmlp::findTagOpen(xml, "dimension", 0);
    if (start == std::string::npos) return;
    const size_t openEnd = xml.find('>', start);
    if (openEnd == std::string::npos) return;
    if (xml[openEnd - 1] == '/') {
        xml.erase(start, openEnd + 1 - start);
        return;
    }
    const size_t close = xml.find("</dimension>", openEnd);
    if (close == std::string::npos) return;
    xml.erase(start, close + 12 - start);
}

// ---------------------------------------------------------------------------
// Package assembly
// ---------------------------------------------------------------------------

struct Replacement {
    std::string name;
    const std::string* content = nullptr;
    const std::function<bool(std::string& chunk)>* stream = nullptr;
};

// Copies every part of the template, substituting the listed ones. Untouched
// parts travel as compressed bytes: no recompression, no re-serialization.
bool assemblePackage(zip_t* archive, const std::vector<Replacement>& replacements,
                     zipio::ZipWriter::Compression compression, std::vector<uint8_t>& out,
                     std::string& error) {
    zipio::ZipWriter writer(out);
    const zip_int64_t count = zip_get_num_entries(archive, 0);

    for (zip_int64_t i = 0; i < count; ++i) {
        const char* rawName = zip_get_name(archive, i, 0);
        if (!rawName) continue;
        const std::string name = normalizeName(rawName);
        if (name.empty() || name.back() == '/') continue; // directory entries

        const Replacement* replacement = nullptr;
        for (const Replacement& candidate : replacements) {
            if (candidate.name == name) {
                replacement = &candidate;
                break;
            }
        }

        if (replacement) {
            const bool ok = replacement->content
                ? writer.addEntry(name, *replacement->content, compression, error)
                : writer.addStreamedEntry(name, *replacement->stream, compression, error);
            if (!ok) return false;
            continue;
        }

        if (writer.copyEntryFrom(archive, i, name, error)) {
            continue;
        }
        if (!error.empty()) return false;

        std::vector<uint8_t> data;
        std::string readError;
        if (!zipio::readFile(archive, rawName, data, zipio::kMaxEntryBytes, readError)) {
            error = "WRITE_FAILED|Failed to copy '" + name + "': " + readError;
            return false;
        }
        if (!writer.addEntry(name, std::string(data.begin(), data.end()), compression, error)) {
            return false;
        }
    }

    return writer.finish(error);
}

bool openTemplate(const TemplateSource& source, zip_t*& archive, std::string& error) {
    if (source.path) {
        archive = zipio::openReadOnly(*source.path, error);
    } else if (source.bytes) {
        archive = zipio::openReadOnlyMemory(*source.bytes, error);
    } else {
        error = "INVALID_OPTIONS|A template is required";
        return false;
    }
    if (!archive) {
        error = "FILE_OPEN_FAILED|" + error;
        return false;
    }
    return true;
}

class ArchiveCloser {
public:
    explicit ArchiveCloser(zip_t* archive) : archive_(archive) {}
    ~ArchiveCloser() {
        if (archive_) zip_close(archive_);
    }

private:
    zip_t* archive_;
};

std::vector<std::string> sortedUnique(std::vector<std::string> values) {
    values.erase(std::remove(values.begin(), values.end(), std::string()), values.end());
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

} // namespace

bool replaceSheetData(const TemplateSource& source, const WritePlan& plan, RowSource& table,
                      zipio::ZipWriter::Compression compression, std::vector<uint8_t>& out,
                      std::string& error) {
    zip_t* archive = nullptr;
    if (!openTemplate(source, archive, error)) return false;
    ArchiveCloser closer(archive);

    std::string part;
    if (!findSheetPart(archive, plan.sheetName, part, error)) return false;

    std::string sheetXml;
    if (!readEntry(archive, part, sheetXml, error)) return false;

    const size_t sheetDataStart = xmlp::findTagOpen(sheetXml, "sheetData", 0);
    if (sheetDataStart == std::string::npos) {
        error = "WRITE_FAILED|Sheet '" + plan.sheetName + "' has no sheetData element";
        return false;
    }
    const size_t sheetDataOpenEnd = sheetXml.find('>', sheetDataStart);
    if (sheetDataOpenEnd == std::string::npos) {
        error = "WRITE_FAILED|Malformed sheetData element";
        return false;
    }
    const bool selfClosing = sheetXml[sheetDataOpenEnd - 1] == '/';

    size_t contentEnd = sheetDataOpenEnd;
    if (!selfClosing) {
        const size_t close = sheetXml.find("</sheetData>", sheetDataOpenEnd);
        if (close == std::string::npos) {
            error = "WRITE_FAILED|Malformed sheetData element";
            return false;
        }
        contentEnd = close;
    }

    std::string prefix = sheetXml.substr(0, sheetDataStart);
    dropDimensionElement(prefix);
    const std::string suffix = selfClosing ? sheetXml.substr(sheetDataOpenEnd + 1)
                                           : sheetXml.substr(contentEnd + 12);

    TemplateStyleBase base;
    std::string stylesXml;
    if (!readTemplateStyleBase(archive, base, stylesXml, error)) return false;

    SheetStyles styles = buildSheetStyles(plan, base.firstNumFmtId, base.xfBaseIndex);
    if (!applyTemplateStyles(stylesXml, styles, error)) return false;

    SheetRowStream rows(plan, table, styles);
    bool prefixSent = false;
    bool closingSent = false;
    const std::function<bool(std::string&)> stream = [&](std::string& chunk) -> bool {
        if (!prefixSent) {
            prefixSent = true;
            chunk = prefix;
            chunk += "<sheetData>";
            return true;
        }
        if (rows.next(chunk)) return true;
        if (!closingSent) {
            closingSent = true;
            chunk = "</sheetData>";
            chunk += suffix;
            return true;
        }
        return false;
    };

    std::vector<Replacement> replacements;
    replacements.push_back({part, nullptr, &stream});
    replacements.push_back({"xl/styles.xml", &stylesXml, nullptr});
    return assemblePackage(archive, replacements, compression, out, error);
}

bool updateCells(const TemplateSource& source, const std::vector<CellUpdate>& updates,
                 zipio::ZipWriter::Compression compression, std::vector<uint8_t>& out,
                 std::string& error) {
    if (updates.empty()) {
        error = "INVALID_OPTIONS|At least one update is required";
        return false;
    }

    zip_t* archive = nullptr;
    if (!openTemplate(source, archive, error)) return false;
    ArchiveCloser closer(archive);

    std::map<std::string, std::vector<CellUpdate>> updatesByPart;
    std::vector<std::string> formats;
    for (const CellUpdate& update : updates) {
        std::string part;
        if (!findSheetPart(archive, update.sheet, part, error)) return false;
        updatesByPart[part].push_back(update);

        if (!update.numberFormat.empty()) {
            formats.push_back(update.numberFormat);
        } else if (update.value.isDate) {
            const bool withTime = (update.value.number - std::floor(update.value.number)) > 1e-9;
            formats.push_back(withTime ? "yyyy-mm-dd hh:mm:ss" : "yyyy-mm-dd");
        }
    }
    formats = sortedUnique(std::move(formats));

    TemplateStyleBase base;
    std::string stylesXml;
    SheetStyles styles;
    bool stylesChanged = false;
    if (!formats.empty()) {
        if (!readTemplateStyleBase(archive, base, stylesXml, error)) return false;
        styles.firstNumFmtId = base.firstNumFmtId;
        styles.xfBaseIndex = base.xfBaseIndex;
        for (const std::string& code : formats) {
            styles.styleForFormat(code);
        }
        if (!applyTemplateStyles(stylesXml, styles, error)) return false;
        stylesChanged = true;
    }

    // Patch the affected sheets first so their storage stays put while the
    // replacements point at it.
    std::vector<std::string> parts;
    std::vector<std::string> contents;
    parts.reserve(updatesByPart.size());
    contents.reserve(updatesByPart.size());
    for (auto& entry : updatesByPart) {
        std::string sheetXml;
        if (!readEntry(archive, entry.first, sheetXml, error)) return false;
        if (!patchSheetCells(sheetXml, entry.second, styles, error)) return false;
        // The declared used range is stale once cells were added or changed;
        // Excel recalculates it, a wrong one would only confuse the scroll area.
        dropDimensionElement(sheetXml);
        parts.push_back(entry.first);
        contents.push_back(std::move(sheetXml));
    }

    std::vector<Replacement> replacements;
    replacements.reserve(parts.size() + 1);
    for (size_t i = 0; i < parts.size(); ++i) {
        replacements.push_back({parts[i], &contents[i], nullptr});
    }
    if (stylesChanged) {
        replacements.push_back({"xl/styles.xml", &stylesXml, nullptr});
    }

    return assemblePackage(archive, replacements, compression, out, error);
}

} // namespace baja_xlsx
