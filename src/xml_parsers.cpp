#include "xml_parsers.h"
#include <cctype>

namespace baja_xlsx { namespace xmlp {

namespace {

inline bool isTagBoundary(char c) {
    return c == ' ' || c == '>' || c == '/' || c == '\t' || c == '\n' || c == '\r';
}

inline bool isNameChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' ||
           c == '-' || c == ':';
}

// Core attribute scan bounded to [start, end); requires a word boundary
// before the attribute name so "Id" does not match "sheetId=".
bool scanAttribute(const std::string& xml, size_t start, size_t end,
                   const std::string& attr, std::string& out) {
    out.clear();
    if (attr.empty() || end <= start || end > xml.size()) return false;

    size_t searchFrom = start;
    while (searchFrom < end) {
        size_t namePos = xml.find(attr, searchFrom);
        if (namePos == std::string::npos || namePos >= end) return false;

        // Word boundary before the attribute name.
        if (namePos > 0 && isNameChar(xml[namePos - 1])) {
            searchFrom = namePos + attr.size();
            continue;
        }

        size_t p = namePos + attr.size();
        while (p < end && (xml[p] == ' ' || xml[p] == '\t' || xml[p] == '\r' || xml[p] == '\n')) ++p;
        if (p >= end || xml[p] != '=') {
            searchFrom = namePos + attr.size();
            continue;
        }
        ++p;
        while (p < end && (xml[p] == ' ' || xml[p] == '\t' || xml[p] == '\r' || xml[p] == '\n')) ++p;
        if (p >= end || (xml[p] != '"' && xml[p] != '\'')) {
            searchFrom = namePos + attr.size();
            continue;
        }
        char quote = xml[p];
        size_t valueStart = p + 1;
        size_t valueEnd = xml.find(quote, valueStart);
        if (valueEnd == std::string::npos || valueEnd > end) return false;
        out = xml.substr(valueStart, valueEnd - valueStart);
        return true;
    }
    return false;
}

} // namespace

size_t findTagOpen(const std::string& xml, const std::string& tag, size_t from) {
    if (tag.empty()) return std::string::npos;
    const std::string needle = "<" + tag;
    size_t pos = from;
    while ((pos = xml.find(needle, pos)) != std::string::npos) {
        size_t nextIdx = pos + needle.size();
        char next = nextIdx < xml.size() ? xml[nextIdx] : '\0';
        if (isTagBoundary(next)) return pos;
        ++pos; // e.g. "<drawing1" while searching "<drawing": keep scanning
    }
    return std::string::npos;
}

bool getAttributeInRange(const std::string& xml, size_t start, size_t end,
                         const std::string& attr, std::string& out) {
    return scanAttribute(xml, start, end, attr, out);
}

bool getAttribute(const std::string& xml, size_t elementStart,
                  const std::string& attr, std::string& out) {
    out.clear();
    if (elementStart >= xml.size()) return false;
    // Bound the search to the end of this element's opening tag.
    size_t gt = xml.find('>', elementStart);
    if (gt == std::string::npos) return false;
    return scanAttribute(xml, elementStart, gt, attr, out);
}

bool getElementText(const std::string& xml, const std::string& tag,
                    size_t from, std::string& out) {
    out.clear();
    size_t open = findTagOpen(xml, tag, from);
    if (open == std::string::npos) return false;
    size_t gt = xml.find('>', open);
    if (gt == std::string::npos) return false;
    if (gt > open && xml[gt - 1] == '/') return true; // self-closing: empty
    const std::string closing = "</" + tag + ">";
    size_t close = xml.find(closing, gt);
    if (close == std::string::npos) return false;
    out = xml.substr(gt + 1, close - gt - 1);
    return true;
}

std::map<std::string, std::string> parseRelationships(const std::string& xmlContent) {
    std::map<std::string, std::string> rIdMap;
    size_t pos = 0;
    while ((pos = findTagOpen(xmlContent, "Relationship", pos)) != std::string::npos) {
        std::string id;
        std::string target;
        const bool hasId = getAttribute(xmlContent, pos, "Id", id);
        const bool hasTarget = getAttribute(xmlContent, pos, "Target", target);

        if (hasId && hasTarget && !id.empty() && !target.empty()) {
            const size_t lastSlash = target.find_last_of('/');
            const std::string filename =
                (lastSlash != std::string::npos) ? target.substr(lastSlash + 1) : target;
            if (!filename.empty()) {
                rIdMap[id] = filename;
            }
        }

        // Advance past this element's opening tag.
        size_t gt = xmlContent.find('>', pos);
        size_t selfClose = xmlContent.find("/>", pos);
        if (selfClose != std::string::npos && selfClose < gt) {
            pos = selfClose + 2;
        } else if (gt != std::string::npos) {
            pos = gt + 1;
        } else {
            break;
        }
    }
    return rIdMap;
}

bool parseNonNegativeInt(const std::string& s, int& out) {
    size_t i = 0;
    size_t j = s.size();
    while (i < j && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
    while (j > i && (s[j - 1] == ' ' || s[j - 1] == '\t' || s[j - 1] == '\r' || s[j - 1] == '\n')) --j;
    if (i >= j || j - i > 9) return false; // reject empty, >9 digits (overflow guard)
    long v = 0;
    for (size_t k = i; k < j; ++k) {
        if (s[k] < '0' || s[k] > '9') return false;
        v = v * 10 + (s[k] - '0');
    }
    out = static_cast<int>(v);
    return true;
}

}} // namespace baja_xlsx::xmlp
