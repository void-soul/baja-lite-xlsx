#include "xlsx_patch.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>

#include "xml_parsers.h"
#include "zip_reader.h"

namespace baja_xlsx {

// Shared package plumbing: locating parts and assembling an archive. Used by
// every write mode, so it lives in its own namespace instead of being local to
// this file (see xlsx_patch.h).
namespace pkg {

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

// Every worksheet part, in workbook order. Needed by the template mode, which
// renders all sheets unless the caller names one.
bool listWorksheetParts(zip_t* archive, std::vector<std::string>& parts, std::string& error) {
    std::string workbook;
    if (!readEntry(archive, "xl/workbook.xml", workbook, error)) return false;
    std::string rels;
    if (!readEntry(archive, "xl/_rels/workbook.xml.rels", rels, error)) return false;

    const std::map<std::string, std::string> relationships = parseRelationshipTargets(rels);
    parts.clear();

    size_t pos = 0;
    while (true) {
        const size_t elementStart = xmlp::findTagOpen(workbook, "sheet", pos);
        if (elementStart == std::string::npos) break;
        pos = elementStart + 5;

        std::string relationshipId;
        xmlp::getAttribute(workbook, elementStart, "r:id", relationshipId);
        auto it = relationships.find(relationshipId);
        if (relationshipId.empty() || it == relationships.end()) continue;

        std::string part;
        if (resolvePartName(archive, it->second, part)) {
            parts.push_back(part);
        }
    }
    return true;
}

} // namespace pkg

using namespace pkg;

namespace {

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

} // namespace

namespace pkg {

// ---------------------------------------------------------------------------
// Package assembly (Replacement is declared in the header)
// ---------------------------------------------------------------------------

// Copies every part of the template, substituting the listed ones. Untouched
// parts travel as compressed bytes: no recompression, no re-serialization.
bool assemblePackage(zip_t* archive, const std::vector<Replacement>& replacements,
                     zipio::ZipWriter::Compression compression, std::vector<uint8_t>& out,
                     std::string& error) {
    zipio::ZipWriter writer(out);
    const zip_int64_t count = zip_get_num_entries(archive, 0);
    std::vector<bool> matched(replacements.size(), false);

    for (zip_int64_t i = 0; i < count; ++i) {
        const char* rawName = zip_get_name(archive, i, 0);
        if (!rawName) continue;
        const std::string name = normalizeName(rawName);
        if (name.empty() || name.back() == '/') continue; // directory entries

        const Replacement* replacement = nullptr;
        for (size_t r = 0; r < replacements.size(); ++r) {
            if (replacements[r].name == name) {
                replacement = &replacements[r];
                matched[r] = true;
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

    // Replacements for parts the archive does not have yet are NEW entries:
    // this is how a created worksheet joins the package.
    for (size_t r = 0; r < replacements.size(); ++r) {
        if (matched[r]) continue;
        const Replacement& rep = replacements[r];
        const bool ok = rep.content
            ? writer.addEntry(rep.name, *rep.content, compression, error)
            : writer.addStreamedEntry(rep.name, *rep.stream, compression, error);
        if (!ok) return false;
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

// ArchiveCloser lives in the header so every write mode shares it.

std::vector<std::string> sortedUnique(std::vector<std::string> values) {
    values.erase(std::remove(values.begin(), values.end(), std::string()), values.end());
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

} // namespace pkg

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
    { std::printf("[dbg] A2\n"); std::fflush(stdout); }
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

// ---------------------------------------------------------------------------
// Mode 1c: appending rows (and creating sheets)
// ---------------------------------------------------------------------------

namespace {

// Locates a worksheet part by name without treating absence as an error, so
// the append mode can create the sheet instead. An empty `sheetName` selects
// the first sheet.
bool tryFindSheetPart(zip_t* archive, const std::string& sheetName, std::string& part) {
    std::string ignored;
    std::string workbook;
    if (!readEntry(archive, "xl/workbook.xml", workbook, ignored)) return false;
    std::string rels;
    if (!readEntry(archive, "xl/_rels/workbook.xml.rels", rels, ignored)) return false;

    const std::map<std::string, std::string> relationships = parseRelationshipTargets(rels);

    size_t pos = 0;
    while (true) {
        const size_t elementStart = xmlp::findTagOpen(workbook, "sheet", pos);
        if (elementStart == std::string::npos) return false;
        pos = elementStart + 5;

        std::string name;
        std::string relationshipId;
        xmlp::getAttribute(workbook, elementStart, "name", name);
        xmlp::getAttribute(workbook, elementStart, "r:id", relationshipId);
        if (!sheetName.empty() && name != sheetName) continue;

        const auto it = relationships.find(relationshipId);
        if (it == relationships.end()) return false;
        return resolvePartName(archive, it->second, part);
    }
}

// Largest row number among the <row r="N"> elements in [from, to).
size_t lastRowNumber(const std::string& sheetXml, size_t from, size_t to) {
    size_t last = 0;
    size_t pos = from;
    while (true) {
        const size_t at = xmlp::findTagOpen(sheetXml, "row", pos);
        if (at == std::string::npos || at >= to) break;
        pos = at + 3;
        std::string text;
        if (xmlp::getAttribute(sheetXml, at, "r", text) && isAllDigits(text)) {
            last = std::max(last, static_cast<size_t>(std::stoul(text)));
        }
    }
    return last;
}

// A part name like xl/worksheets/sheet3.xml that no entry uses yet.
std::string freeWorksheetPartName(zip_t* archive) {
    static const std::string prefix = "xl/worksheets/sheet";
    std::set<size_t> used;
    const int count = zip_get_num_entries(archive, 0);
    for (int i = 0; i < count; ++i) {
        const char* name = zip_get_name(archive, i, 0);
        if (!name) continue;
        const std::string normalized = normalizeName(name);
        if (normalized.compare(0, prefix.size(), prefix) != 0) continue;
        const size_t digitsEnd = normalized.find(".xml", prefix.size());
        if (digitsEnd == std::string::npos) continue;
        const std::string digits = normalized.substr(prefix.size(), digitsEnd - prefix.size());
        if (!digits.empty() && isAllDigits(digits)) {
            used.insert(static_cast<size_t>(std::stoul(digits)));
        }
    }
    size_t n = 1;
    while (used.count(n) != 0) ++n;
    return "xl/worksheets/sheet" + std::to_string(n) + ".xml";
}

// The three edits a new sheet needs, filled by registerWorksheet and handed to
// the assembly step by the caller. Kept as plain data: no shared mutable state
// between concurrent workers.
struct WorkbookEdits {
    std::string workbook;
    std::string rels;
    std::string types;
};

// Adds the new sheet to workbook.xml, its relationships and [Content_Types].
bool registerWorksheet(zip_t* archive, const std::string& sheetName, const std::string& part,
                       WorkbookEdits& edits, std::string& error) {
    std::string workbook;
    if (!readEntry(archive, "xl/workbook.xml", workbook, error)) return false;
    std::string rels;
    if (!readEntry(archive, "xl/_rels/workbook.xml.rels", rels, error)) return false;
    std::string types;
    if (!readEntry(archive, "[Content_Types].xml", types, error)) return false;

    const std::map<std::string, std::string> relationships = parseRelationshipTargets(rels);

    size_t maxSheetId = 0;
    {
        size_t pos = 0;
        while (true) {
            const size_t at = xmlp::findTagOpen(workbook, "sheet", pos);
            if (at == std::string::npos) break;
            pos = at + 5;
            std::string text;
            if (xmlp::getAttribute(workbook, at, "sheetId", text) && isAllDigits(text)) {
                maxSheetId = std::max(maxSheetId, static_cast<size_t>(std::stoul(text)));
            }
        }
    }

    std::string relationshipId;
    for (size_t n = 1; relationshipId.empty(); ++n) {
        const std::string candidate = "rId" + std::to_string(n);
        if (relationships.find(candidate) == relationships.end()) {
            relationshipId = candidate;
        }
        if (n > relationships.size() + 1) {
            error = "WRITE_FAILED|Could not find a free relationship id";
            return false;
        }
    }

    const size_t sheetsClose = workbook.find("</sheets>");
    if (sheetsClose == std::string::npos) {
        error = "WRITE_FAILED|The template workbook.xml has no </sheets>";
        return false;
    }
    workbook.insert(sheetsClose,
                    "<sheet name=\"" + escapeXmlAttribute(sheetName) + "\" sheetId=\"" +
                        std::to_string(maxSheetId + 1) + "\" r:id=\"" + relationshipId + "\"/>");

    const size_t relsClose = rels.find("</Relationships>");
    if (relsClose == std::string::npos) {
        error = "WRITE_FAILED|The workbook relationships part is malformed";
        return false;
    }
    rels.insert(relsClose,
                "<Relationship Id=\"" + relationshipId +
                    "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
                    "relationships/worksheet\" Target=\"" + part.substr(3) + "\"/>");

    const size_t typesClose = types.find("</Types>");
    if (typesClose == std::string::npos) {
        error = "WRITE_FAILED|[Content_Types].xml is malformed";
        return false;
    }
    types.insert(typesClose,
                 "<Override PartName=\"/" + part +
                     "\" ContentType=\"application/vnd.openxmlformats-officedocument."
                     "spreadsheetml.worksheet+xml\"/>");

    edits.workbook = std::move(workbook);
    edits.rels = std::move(rels);
    edits.types = std::move(types);
    return true;
}

// Builds a complete worksheet part for a freshly created sheet: optional
// freeze pane, column widths and the streamed rows.
std::string buildNewSheetXml(const WritePlan& plan, SheetStyles& styles, RowSource& table) {
    const size_t columnCount = plan.columns.size();
    std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">";

    if (plan.freezeHeader) {
        xml += "<sheetViews><sheetView tabSelected=\"1\" workbookViewId=\"0\">"
               "<pane ySplit=\"1\" topLeftCell=\"A2\" activePane=\"bottomLeft\" "
               "state=\"frozen\"/></sheetView></sheetViews>";
    }
    xml += "<sheetFormatPr defaultRowHeight=\"15\"/>";

    const bool hasWidths =
        std::any_of(plan.columns.begin(), plan.columns.end(),
                    [](const WriteColumn& c) { return c.width > 0; });
    if (hasWidths) {
        xml += "<cols>";
        for (size_t i = 0; i < columnCount; ++i) {
            if (plan.columns[i].width <= 0) continue;
            xml += "<col min=\"" + std::to_string(i + 1) + "\" max=\"" + std::to_string(i + 1) +
                   "\" width=\"" + formatNumber(plan.columns[i].width) +
                   "\" customWidth=\"1\"/>";
        }
        xml += "</cols>";
    }

    xml += "<sheetData>";
    SheetRowStream rows(plan, table, styles, 1);
    std::string chunk;
    while (rows.next(chunk)) {
        xml += chunk;
    }
    xml += "</sheetData></worksheet>";
    return xml;
}

} // namespace

bool appendRows(const TemplateSource& source, const WritePlan& plan, RowSource& table,
                const std::string& headerMode, zipio::ZipWriter::Compression compression,
                std::vector<uint8_t>& out, std::string& error) {
    if (plan.columns.empty()) {
        error = "INVALID_OPTIONS|At least one column is required";
        return false;
    }
    if (!isValidSheetName(plan.sheetName, error) && !plan.sheetName.empty()) {
        error = "INVALID_OPTIONS|" + error;
        return false;
    }

    zip_t* archive = nullptr;
    if (!openTemplate(source, archive, error)) return false;
    ArchiveCloser closer(archive);

    std::string part;
    const bool exists = tryFindSheetPart(archive, plan.sheetName, part);
    bool header = headerMode == "yes";
    size_t startRow = 1;

    std::string createdXml;
    std::string prefix;
    std::string suffix;

    if (!exists) {
        if (plan.sheetName.empty()) {
            error = "SHEET_NOT_FOUND|The workbook has no sheets; pass a sheetName to create one";
            return false;
        }
        header = headerMode != "no";
        part = freeWorksheetPartName(archive);
    } else {
        std::string sheetXml;
        if (!readEntry(archive, part, sheetXml, error)) return false;

        const size_t sheetDataStart = xmlp::findTagOpen(sheetXml, "sheetData", 0);
        bool selfClosing = false;
        if (sheetDataStart != std::string::npos) {
            const size_t openEnd = sheetXml.find('>', sheetDataStart);
            if (openEnd == std::string::npos) {
                error = "WRITE_FAILED|Malformed sheetData element";
                return false;
            }
            selfClosing = sheetXml[openEnd - 1] == '/';
            size_t rowsFrom = openEnd + 1;
            size_t rowsTo = openEnd;
            if (selfClosing) {
                prefix = sheetXml.substr(0, sheetDataStart) + "<sheetData>";
                suffix = sheetXml.substr(openEnd + 1);
            } else {
                const size_t close = sheetXml.find("</sheetData>", openEnd);
                if (close == std::string::npos) {
                    error = "WRITE_FAILED|Malformed sheetData element";
                    return false;
                }
                rowsFrom = openEnd + 1;
                rowsTo = close;
                prefix = sheetXml.substr(0, close);
                suffix = sheetXml.substr(close + 12);
            }
            startRow = lastRowNumber(sheetXml, rowsFrom, rowsTo) + 1;
        } else {
            // A worksheet without sheetData: start one right before </worksheet>.
            const size_t close = sheetXml.find("</worksheet>");
            if (close == std::string::npos) {
                error = "WRITE_FAILED|Malformed worksheet part";
                return false;
            }
            prefix = sheetXml.substr(0, close) + "<sheetData>";
            suffix = "</worksheet>" + sheetXml.substr(close + 12);
        }

        header = headerMode == "yes" || (headerMode == "auto" && startRow == 1);
        dropDimensionElement(prefix);
    }

    // Styles: appended rows may need number formats the template does not have.
    { std::printf("[dbg] A3\n"); std::fflush(stdout); }
    TemplateStyleBase base;
    std::string stylesXml;
    if (!readTemplateStyleBase(archive, base, stylesXml, error)) return false;

    WritePlan effective = plan;
    effective.includeHeader = header;
    SheetStyles styles = buildSheetStyles(effective, base.firstNumFmtId, base.xfBaseIndex);
    if (!applyTemplateStyles(stylesXml, styles, error)) return false;

    // Declared at function scope: the stream producer handed to the assembly
    // step must outlive it, and so must everything it captures by reference.
    SheetRowStream rows(effective, table, styles, startRow);
    bool prefixSent = false;
    bool closingSent = false;
    const std::function<bool(std::string&)> stream = [&](std::string& chunk) -> bool {
        if (!prefixSent) {
            prefixSent = true;
            chunk = prefix;
            return true;
        }
        if (rows.next(chunk)) return true;
        if (!closingSent) {
            closingSent = true;
            chunk = "</sheetData>" + suffix;
            return true;
        }
        return false;
    };

    std::vector<Replacement> replacements;
    replacements.push_back({"xl/styles.xml", &stylesXml, nullptr});

    WorkbookEdits edits;
    if (!exists) {
        createdXml = buildNewSheetXml(effective, styles, table);
        if (!registerWorksheet(archive, plan.sheetName, part, edits, error)) return false;

        replacements.push_back({"xl/workbook.xml", &edits.workbook, nullptr});
        replacements.push_back({"xl/_rels/workbook.xml.rels", &edits.rels, nullptr});
        replacements.push_back({"[Content_Types].xml", &edits.types, nullptr});
        replacements.push_back({part, &createdXml, nullptr});
    } else {
        replacements.push_back({part, nullptr, &stream});
    }

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
