#include "xlsx_template.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <mutex>

#include "xml_parsers.h"

namespace baja_xlsx {

namespace {

const char* kPlaceholderOpen = "${";
const char* kEachOpen = "{{#each ";
const char* kEachClose = "{{/each}}";

// ---------------------------------------------------------------------------
// Small text helpers
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
                code = code * (hex ? 16u : 10u) + static_cast<unsigned long>(value);
            }
            if (!ok || code > 0x10FFFF) {
                out.append(text, i, semi - i + 1);
                i = semi;
                continue;
            }
            // UTF-8 encode.
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
// XML element scanning
// ---------------------------------------------------------------------------

struct ElementSpan {
    size_t start = 0;
    size_t end = 0;
    size_t openEnd = 0;
    bool selfClosing = false;
};

bool findElement(const std::string& xml, const std::string& tag, size_t from, ElementSpan& out) {
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

std::string attributeInTag(const std::string& xml, size_t elementStart, const char* name) {
    std::string value;
    if (!xmlp::getAttribute(xml, elementStart, name, value)) {
        return std::string();
    }
    return value;
}

// Replaces the row number of an `r="B5"` style attribute, keeping the column
// letters. Used to rebase copied cells and rows onto their new row.
bool setReferenceRow(std::string& tag, size_t newRow) {
    const size_t at = tag.find("r=\"");
    if (at == std::string::npos) return false;
    const size_t valueStart = at + 3;
    const size_t valueEnd = tag.find('"', valueStart);
    if (valueEnd == std::string::npos) return false;

    const std::string reference = tag.substr(valueStart, valueEnd - valueStart);
    std::string letters;
    for (char c : reference) {
        if (!std::isalpha(static_cast<unsigned char>(c))) break;
        letters.push_back(c);
    }
    tag.replace(valueStart, valueEnd - valueStart, letters + std::to_string(newRow));
    return true;
}

// Removes a stale <dimension> so Excel does not show a wrong used range.
void dropDimension(std::string& xml) {
    ElementSpan span;
    if (!findElement(xml, "dimension", 0, span)) return;
    xml.erase(span.start, span.end - span.start);
}

// ---------------------------------------------------------------------------
// Cell text
// ---------------------------------------------------------------------------

std::vector<std::string> parseSharedStrings(const std::string& xml) {
    std::vector<std::string> table;
    size_t pos = 0;
    while (true) {
        ElementSpan si;
        if (!findElement(xml, "si", pos, si)) break;
        pos = si.end;

        std::string text;
        size_t textPos = si.start;
        while (true) {
            ElementSpan t;
            if (!findElement(xml, "t", textPos, t)) break;
            if (t.start >= si.end) break;
            textPos = t.end;

            const std::string inner = t.selfClosing ? std::string()
                                                    : xml.substr(t.openEnd + 1,
                                                                 t.end - t.openEnd - 5);
            text += unescapeXml(inner);
        }
        table.push_back(text);
    }
    return table;
}

std::string collectInlineText(const std::string& xml, size_t from, size_t to) {
    std::string text;
    size_t pos = from;
    while (true) {
        ElementSpan t;
        if (!findElement(xml, "t", pos, t)) break;
        if (t.start >= to) break;
        pos = t.end;
        if (t.selfClosing) continue;
        text += unescapeXml(xml.substr(t.openEnd + 1, t.end - t.openEnd - 5));
    }
    return text;
}

// Text of a cell, from a shared string, an inline string or a cached formula
// result. Empty when the cell holds a plain number or boolean.
std::string cellText(const std::string& xml, const ElementSpan& cell,
                     const std::vector<std::string>& sharedStrings) {
    const std::string type = attributeInTag(xml, cell.start, "t");
    if (type == "s") {
        ElementSpan v;
        if (findElement(xml, "v", cell.openEnd, v) && v.start < cell.end) {
            // inner length = end - (openEnd + 1) - len("</v>")
            const std::string raw = trimmed(xml.substr(v.openEnd + 1, v.end - v.openEnd - 5));
            if (!raw.empty() &&
                std::all_of(raw.begin(), raw.end(),
                            [](char c) { return c >= '0' && c <= '9'; })) {
                const size_t index = static_cast<size_t>(std::stoul(raw));
                if (index < sharedStrings.size()) return sharedStrings[index];
            }
        }
        return std::string();
    }
    if (type == "inlineStr") {
        return collectInlineText(xml, cell.openEnd, cell.end);
    }
    if (type == "str") {
        ElementSpan v;
        if (findElement(xml, "v", cell.openEnd, v) && v.start < cell.end) {
            return unescapeXml(xml.substr(v.openEnd + 1, v.end - v.openEnd - 5));
        }
    }
    return std::string();
}

// ---------------------------------------------------------------------------
// Placeholders
// ---------------------------------------------------------------------------

enum class MarkerKind { None, EachStart, EachEnd };

struct Marker {
    MarkerKind kind = MarkerKind::None;
    std::string path;
};

Marker classifyMarker(const std::string& text) {
    const std::string value = trimmed(text);
    if (value == kEachClose) {
        Marker marker;
        marker.kind = MarkerKind::EachEnd;
        return marker;
    }
    if (value.rfind(kEachOpen, 0) == 0 && value.back() == '}') {
        const size_t close = value.rfind("}}");
        if (close != std::string::npos && close > std::strlen(kEachOpen)) {
            Marker marker;
            marker.kind = MarkerKind::EachStart;
            marker.path = trimmed(value.substr(std::strlen(kEachOpen),
                                              close - std::strlen(kEachOpen)));
            return marker;
        }
    }
    return Marker();
}

struct Substitution {
    bool ok = true;
    bool wholeCell = false; // the cell text was exactly one placeholder
    bool isNumber = false;
    bool isBoolean = false;
    double number = 0;
    bool boolean = false;
    std::string text;
};

// Drops one path segment, for "../name" style references out of a loop.
std::string stripLoopSegment(const std::string& prefix) {
    const size_t dot = prefix.find_last_of('.');
    return dot == std::string::npos ? std::string() : prefix.substr(0, dot);
}

const TemplateValue* lookupValue(const TemplateValues& values, const std::string& path,
                                 const std::string& loopPrefix, bool& isIndex, double& index) {
    isIndex = false;

    std::string candidate = path;
    if (candidate == "@index") {
        if (loopPrefix.empty()) return nullptr;
        const size_t dot = loopPrefix.find_last_of('.');
        const std::string digits = dot == std::string::npos ? loopPrefix
                                                            : loopPrefix.substr(dot + 1);
        if (digits.empty() ||
            !std::all_of(digits.begin(), digits.end(),
                         [](char c) { return c >= '0' && c <= '9'; })) {
            return nullptr;
        }
        isIndex = true;
        index = static_cast<double>(std::stoul(digits));
        return nullptr;
    }

    std::string prefix = loopPrefix;
    while (candidate.rfind("../", 0) == 0) {
        prefix = stripLoopSegment(prefix);
        candidate = candidate.substr(3);
    }

    if (candidate == "." ) {
        candidate = prefix;
    } else if (!prefix.empty()) {
        auto it = values.find(prefix + "." + candidate);
        if (it != values.end()) return &it->second;
    }

    if (candidate.empty()) return nullptr;
    auto it = values.find(candidate);
    return it == values.end() ? nullptr : &it->second;
}

Substitution substituteText(const std::string& text, const TemplateValues& values,
                            const std::string& loopPrefix, bool strict,
                            const std::string& where, std::string& error) {
    Substitution result;

    size_t cursor = 0;
    size_t placeholderCount = 0;
    std::string& out = result.text;

    while (true) {
        const size_t open = text.find(kPlaceholderOpen, cursor);
        if (open == std::string::npos) {
            out.append(text, cursor, std::string::npos);
            break;
        }
        const size_t close = text.find('}', open + 2);
        if (close == std::string::npos) {
            error = "TEMPLATE_ERROR|Unterminated placeholder in " + where + ": \"" + text + "\"";
            result.ok = false;
            return result;
        }

        out.append(text, cursor, open - cursor);
        const std::string path = trimmed(text.substr(open + 2, close - open - 2));
        ++placeholderCount;

        bool isIndex = false;
        double indexValue = 0;
        const TemplateValue* value = lookupValue(values, path, loopPrefix, isIndex, indexValue);

        if (isIndex) {
            out += formatNumber(indexValue);
            result.wholeCell = placeholderCount == 1 && open == 0 && close + 1 == text.size();
            if (result.wholeCell) {
                result.isNumber = true;
                result.number = indexValue;
            }
            cursor = close + 1;
            continue;
        }

        if (!value) {
            if (strict) {
                error = "TEMPLATE_ERROR|No value for placeholder \"${" + path + "}\" in " + where;
                result.ok = false;
                return result;
            }
            cursor = close + 1;
            continue;
        }

        const bool whole = placeholderCount == 1 && open == 0 && close + 1 == text.size();
        switch (value->kind) {
            case TemplateValue::Kind::Number:
                out += formatNumber(value->number);
                if (whole) {
                    result.isNumber = true;
                    result.number = value->number;
                }
                break;
            case TemplateValue::Kind::Boolean:
                out += value->boolean ? "true" : "false";
                if (whole) {
                    result.isBoolean = true;
                    result.boolean = value->boolean;
                }
                break;
            case TemplateValue::Kind::Text:
                out += value->text;
                break;
        }
        if (whole) result.wholeCell = true;
        cursor = close + 1;
    }

    return result;
}

// ---------------------------------------------------------------------------
// Sheet rendering
// ---------------------------------------------------------------------------

// Cell XML for a rewritten placeholder cell, keeping the original style so the
// template's number format and alignment still apply.
std::string rebuiltCell(const std::string& reference, const std::string& styleAttribute,
                        const Substitution& value) {
    std::string cell = "<c r=\"" + reference + "\"";
    if (!styleAttribute.empty()) {
        cell += " s=\"" + styleAttribute + "\"";
    }

    if (value.isNumber) {
        cell += "><v>" + formatNumber(value.number) + "</v></c>";
        return cell;
    }
    if (value.isBoolean) {
        cell += " t=\"b\"><v>";
        cell += value.boolean ? "1" : "0";
        cell += "</v></c>";
        return cell;
    }

    cell += " t=\"inlineStr\"><is><t";
    const std::string& text = value.text;
    if (!text.empty() &&
        (isSpace(text.front()) || isSpace(text.back()) ||
         text.find('\n') != std::string::npos || text.find('\t') != std::string::npos)) {
        cell += " xml:space=\"preserve\"";
    }
    cell += ">";
    cell += escapeXmlText(text);
    cell += "</t></is></c>";
    return cell;
}

// ---------------------------------------------------------------------------
// Parsed template structure
// ---------------------------------------------------------------------------
//
// Reading a template means decompressing its sheets and scanning every row for
// markers. That layout cannot change while the file does not, so it is kept
// apart from the per-render work (substitute values, deflate, assemble) and can
// be cached -- see TemplateCache.

// One sheet's parsed structure. `xml` owns the bytes `rows` points into, so the
// two must travel together.
struct CachedSheet {
    std::string part;
    std::string xml;
    std::vector<ElementSpan> rows;
    std::vector<Marker> markers;
};

struct CachedTemplate {
    std::vector<std::string> sharedStrings;
    std::vector<CachedSheet> sheets; // only the sheets that carry markers
    size_t bytes = 0;
};

// A sheet needs rendering when it contains a marker, either inline or -- the
// way Excel writes it -- inside a shared string.
bool hasMarkers(const std::string& text) {
    return text.find("${") != std::string::npos || text.find("{{") != std::string::npos;
}

bool hasMarkers(const std::vector<std::string>& sharedStrings) {
    for (const std::string& text : sharedStrings) {
        if (hasMarkers(text)) return true;
    }
    return false;
}

// Splits a sheet into its rows and records which row holds which marker. Runs
// once per template per cache life, instead of once per render.
void splitSheet(const std::string& sheetXml, const std::vector<std::string>& sharedStrings,
                std::vector<ElementSpan>& rows, std::vector<Marker>& markers) {
    rows.clear();
    markers.clear();

    ElementSpan sheetData;
    if (!findElement(sheetXml, "sheetData", 0, sheetData)) return;

    size_t pos = sheetData.openEnd + 1;
    while (!sheetData.selfClosing) {
        ElementSpan row;
        if (!findElement(sheetXml, "row", pos, row)) break;
        if (row.start >= sheetData.end) break;
        pos = row.end;
        rows.push_back(row);
    }

    markers.resize(rows.size());
    for (size_t i = 0; i < rows.size(); ++i) {
        for (size_t cellPos = rows[i].openEnd + 1; cellPos < rows[i].end;) {
            ElementSpan cell;
            if (!findElement(sheetXml, "c", cellPos, cell)) break;
            if (cell.start >= rows[i].end) break;
            cellPos = cell.end;
            const Marker marker = classifyMarker(cellText(sheetXml, cell, sharedStrings));
            if (marker.kind != MarkerKind::None) {
                markers[i] = marker;
                break;
            }
        }
    }
}

struct RenderContext {
    const TemplateValues& values;
    const TemplatePlan& plan;
    const std::vector<std::string>& sharedStrings;
    std::string sheetLabel;
    std::string error;
    bool ok = true;
};

// One template row, emitted once (loopPrefix empty) or once per loop item.
bool emitRow(RenderContext& context, const std::string& sheetXml, const ElementSpan& row,
             size_t newRowNumber, const std::string& loopPrefix, std::string& out) {
    std::string rowTag = sheetXml.substr(row.start, row.openEnd - row.start + 1);
    if (!setReferenceRow(rowTag, newRowNumber)) {
        rowTag = "<row r=\"" + std::to_string(newRowNumber) + "\">";
    }
    if (row.selfClosing) {
        // An empty row stays empty; it only carries its number.
        out += rowTag;
        return true;
    }

    std::string body;
    size_t pos = row.openEnd + 1;
    while (true) {
        ElementSpan cell;
        if (!findElement(sheetXml, "c", pos, cell)) break;
        if (cell.start >= row.end) break;
        pos = cell.end;

        const std::string reference = attributeInTag(sheetXml, cell.start, "r");
        const std::string text = cellText(sheetXml, cell, context.sharedStrings);

        // Control cells are consumed, not rendered.
        if (classifyMarker(text).kind != MarkerKind::None) {
            continue;
        }

        size_t column = 0;
        size_t rowNumber = 0;
        std::string letters;
        if (reference.empty() || !parseA1Reference(reference, column, rowNumber, letters)) {
            // No usable reference: copy the cell untouched.
            body.append(sheetXml, cell.start, cell.end - cell.start);
            continue;
        }
        const std::string rebased = letters + std::to_string(newRowNumber);

        if (text.find(kPlaceholderOpen) == std::string::npos) {
            // Copied verbatim (styles, formats, formulas) with only its row
            // number rebased, which is what preserves the template's look.
            std::string cellXml = sheetXml.substr(cell.start, cell.end - cell.start);
            setReferenceRow(cellXml, newRowNumber);
            body += cellXml;
            continue;
        }

        const std::string styleAttribute = attributeInTag(sheetXml, cell.start, "s");
        const std::string where = "sheet " + context.sheetLabel + " cell " + reference;
        Substitution value =
            substituteText(text, context.values, loopPrefix, context.plan.strict, where,
                           context.error);
        if (!value.ok) {
            context.ok = false;
            return false;
        }
        body += rebuiltCell(rebased, styleAttribute, value);
    }

    if (body.empty()) {
        // A row that held nothing but control cells disappears entirely.
        return true;
    }
    out += rowTag;
    out += body;
    out += "</row>";
    return true;
}

// True when a cell holds neither text nor a value/formula, so an empty -- but
// present -- cell does not make a marker row count as content.
bool cellIsBlank(const std::string& sheetXml, const ElementSpan& cell,
                 const std::vector<std::string>& sharedStrings) {
    if (!trimmed(cellText(sheetXml, cell, sharedStrings)).empty()) return false;

    ElementSpan inner;
    if (findElement(sheetXml, "v", cell.openEnd, inner) && inner.start < cell.end) return false;
    if (findElement(sheetXml, "f", cell.openEnd, inner) && inner.start < cell.end) return false;
    return true;
}

// A row "has content" when it holds anything besides markers: that is what
// separates "{{#each}} on its own row" (a delimiter, dropped) from
// "{{#each}} in the row to repeat" (the first repeated row).
bool rowHasRealContent(const std::string& sheetXml, const ElementSpan& row,
                       const std::vector<std::string>& sharedStrings) {
    for (size_t pos = row.openEnd + 1; pos < row.end;) {
        ElementSpan cell;
        if (!findElement(sheetXml, "c", pos, cell)) break;
        if (cell.start >= row.end) break;
        pos = cell.end;

        if (classifyMarker(cellText(sheetXml, cell, sharedStrings)).kind != MarkerKind::None) {
            continue;
        }
        if (!cellIsBlank(sheetXml, cell, sharedStrings)) {
            return true;
        }
    }
    return false;
}

size_t loopCount(const TemplateValues& values, const std::string& path) {
    auto lengthIt = values.find(path + ".length");
    if (lengthIt != values.end()) {
        if (lengthIt->second.kind == TemplateValue::Kind::Number) {
            const double count = lengthIt->second.number;
            if (count > 0 && count < 1e7) {
                return static_cast<size_t>(count);
            }
        }
        if (lengthIt->second.kind == TemplateValue::Kind::Text && !lengthIt->second.text.empty()) {
            const std::string& text = lengthIt->second.text;
            if (std::all_of(text.begin(), text.end(),
                            [](char c) { return c >= '0' && c <= '9'; })) {
                return static_cast<size_t>(std::stoul(text));
            }
        }
        return 0;
    }

    // No explicit length: derive it from the flattened keys.
    const std::string prefix = path + ".";
    size_t maxIndex = 0;
    bool any = false;
    for (const auto& entry : values) {
        if (entry.first.compare(0, prefix.size(), prefix) != 0) continue;
        const std::string rest = entry.first.substr(prefix.size());
        const size_t dot = rest.find('.');
        const std::string head = dot == std::string::npos ? rest : rest.substr(0, dot);
        if (head.empty() ||
            !std::all_of(head.begin(), head.end(),
                         [](char c) { return c >= '0' && c <= '9'; })) {
            continue;
        }
        any = true;
        maxIndex = std::max(maxIndex, static_cast<size_t>(std::stoul(head)));
    }
    return any ? maxIndex + 1 : 0;
}

bool renderSheet(RenderContext& context, const CachedSheet& cached, std::string& out) {
    // The row layout and marker positions were split once, when the template
    // structure was built or came out of the cache.
    const std::string& sheetXml = cached.xml;
    const std::vector<ElementSpan>& rows = cached.rows;
    const std::vector<Marker>& markers = cached.markers;

    ElementSpan sheetData;
    if (!findElement(sheetXml, "sheetData", 0, sheetData)) {
        out = sheetXml; // nothing to render
        return true;
    }

    std::string rendered;
    size_t outputRow = 0;

    for (size_t i = 0; i < rows.size();) {
        if (markers[i].kind == MarkerKind::EachEnd) {
            context.ok = false;
            context.error = "TEMPLATE_ERROR|{{/each}} without a matching {{#each}} in sheet " +
                            context.sheetLabel;
            return false;
        }

        if (markers[i].kind != MarkerKind::EachStart) {
            if (!emitRow(context, sheetXml, rows[i], ++outputRow, std::string(), rendered)) {
                return false;
            }
            ++i;
            continue;
        }

        // Find the matching {{/each}}, allowing nested blocks.
        size_t end = i + 1;
        size_t depth = 1;
        for (; end < rows.size(); ++end) {
            if (markers[end].kind == MarkerKind::EachStart) ++depth;
            else if (markers[end].kind == MarkerKind::EachEnd && --depth == 0) break;
        }
        if (end >= rows.size()) {
            context.ok = false;
            context.error = "TEMPLATE_ERROR|{{#each " + markers[i].path +
                            "}} is never closed in sheet " + context.sheetLabel;
            return false;
        }

        // A row holding nothing but the marker delimits the block instead of
        // being repeated (the common "marker on its own row" layout); a marker
        // row with data cells is the first repeated row.
        const size_t bodyStart =
            rowHasRealContent(sheetXml, rows[i], context.sharedStrings) ? i : i + 1;

        const size_t count = loopCount(context.values, markers[i].path);
        for (size_t item = 0; item < count; ++item) {
            const std::string prefix = markers[i].path + "." + std::to_string(item);
            for (size_t r = bodyStart; r < end; ++r) {
                if (!emitRow(context, sheetXml, rows[r], ++outputRow, prefix, rendered)) {
                    return false;
                }
            }
        }

        // Whatever the closing row carries besides its marker is emitted once,
        // after the repeated rows -- unless it is an empty row that only exists
        // to hold the marker itself.
        if (rowHasRealContent(sheetXml, rows[end], context.sharedStrings)) {
            if (!emitRow(context, sheetXml, rows[end], ++outputRow, std::string(), rendered)) {
                return false;
            }
        }
        i = end + 1;
    }

    std::string prefix = sheetXml.substr(0, sheetData.start);
    dropDimension(prefix);
    const std::string suffix = sheetData.selfClosing
        ? sheetXml.substr(sheetData.openEnd + 1)
        : sheetXml.substr(sheetData.end);

    out = prefix;
    out += "<sheetData>";
    out += rendered;
    out += "</sheetData>";
    out += suffix;
    return true;
}

// Reads and splits the sheets that carry markers. Sheets without markers are
// left out entirely, so they are copied verbatim during assembly and a report
// template that only fills one sheet keeps every other sheet byte-identical.
bool buildTemplateStructure(zip_t* archive, const TemplatePlan& plan, CachedTemplate& out,
                            std::string& error) {
    std::vector<std::string> candidates;
    if (!plan.sheetName.empty()) {
        std::string part;
        if (!pkg::findSheetPart(archive, plan.sheetName, part, error)) return false;
        candidates.push_back(part);
    } else if (!pkg::listWorksheetParts(archive, candidates, error)) {
        return false;
    }

    std::string sharedXml;
    std::string ignored;
    if (pkg::readEntry(archive, "xl/sharedStrings.xml", sharedXml, ignored)) {
        out.sharedStrings = parseSharedStrings(sharedXml);
        out.bytes += sharedXml.size();
    }

    // A template authored in Excel keeps its cell text -- markers included --
    // in sharedStrings, so the sheet XML alone cannot tell whether it needs
    // rendering. When the shared table carries markers every candidate has to be
    // scanned; otherwise the cheap inline check is enough and marker-free sheets
    // are copied verbatim.
    const bool sharedHasMarkers = hasMarkers(out.sharedStrings);

    for (const std::string& part : candidates) {
        std::string sheetXml;
        if (!pkg::readEntry(archive, part, sheetXml, error)) return false;
        if (!sharedHasMarkers && !hasMarkers(sheetXml)) {
            continue;
        }

        CachedSheet sheet;
        sheet.part = part;
        sheet.xml = std::move(sheetXml);
        splitSheet(sheet.xml, out.sharedStrings, sheet.rows, sheet.markers);

        out.bytes += sheet.xml.size() +
                     sheet.rows.size() * sizeof(ElementSpan) +
                     sheet.markers.size() * sizeof(Marker);
        out.sheets.push_back(std::move(sheet));
    }
    return true;
}

// ---------------------------------------------------------------------------
// Template cache
// ---------------------------------------------------------------------------

// Identity comes from the archive's central directory -- entry names, sizes and
// CRC32s -- so it is content based instead of timestamp based (a rewritten
// template is always noticed) and needs no stat, which keeps it portable. The
// scan touches no entry data, only the directory.
std::string archiveIdentity(zip_t* archive) {
    uint64_t hash = 1469598103934665603ull; // FNV-1a
    const auto mix = [&hash](uint64_t value) { hash = (hash ^ value) * 1099511628211ull; };

    const zip_int64_t count = zip_get_num_entries(archive, 0);
    mix(static_cast<uint64_t>(count));
    for (zip_int64_t i = 0; i < count; ++i) {
        zip_stat_t info;
        zip_stat_init(&info);
        if (zip_stat_index(archive, i, 0, &info) != 0) continue;

        const std::string name = info.name ? pkg::normalizeName(info.name) : std::string();
        for (char c : name) {
            mix(static_cast<unsigned char>(c));
        }
        mix(static_cast<uint64_t>(info.size));
        mix(static_cast<uint64_t>(info.crc));
    }
    return std::to_string(hash);
}

// Rendering with a different sheet filter parses a different structure.
std::string cacheKey(zip_t* archive, const std::string& sheetFilter) {
    std::string key = archiveIdentity(archive);
    if (!sheetFilter.empty()) {
        key += "|sheet:" + sheetFilter;
    }
    return key;
}

// Bounded LRU of parsed templates. Shared by every render -- including the
// async workers, which is why it is mutex-protected -- and evicted by both entry
// count and total bytes, so a big template cannot pin memory forever.
class TemplateCache {
public:
    static constexpr size_t kMaxEntries = 8;
    static constexpr size_t kMaxBytes = 64u * 1024u * 1024u;

    std::shared_ptr<const CachedTemplate> get(const std::string& key) {
        if (key.empty()) return nullptr;
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->first != key) continue;
            // Promote to most-recently-used.
            entries_.splice(entries_.begin(), entries_, it);
            return entries_.front().second;
        }
        return nullptr;
    }

    void put(const std::string& key, const std::shared_ptr<CachedTemplate>& entry) {
        if (key.empty() || !entry || entry->sheets.empty() || entry->bytes > kMaxBytes) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->first != key) continue;
            bytes_ -= it->second->bytes;
            entries_.erase(it);
            break;
        }

        entries_.push_front(std::make_pair(key, std::shared_ptr<const CachedTemplate>(entry)));
        bytes_ += entry->bytes;

        while (entries_.size() > kMaxEntries || bytes_ > kMaxBytes) {
            if (entries_.empty()) break;
            bytes_ -= entries_.back().second->bytes;
            entries_.pop_back();
        }
    }

    size_t entries() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size();
    }

private:
    mutable std::mutex mutex_;
    std::list<std::pair<std::string, std::shared_ptr<const CachedTemplate>>> entries_;
    size_t bytes_ = 0;
};

TemplateCache& templateCache() {
    // Function-local static: created on first use, destroyed at exit.
    static TemplateCache cache;
    return cache;
}

} // namespace

bool renderTemplate(const TemplateSource& source, const TemplateValues& values,
                    const TemplatePlan& plan, zipio::ZipWriter::Compression compression,
                    std::vector<uint8_t>& out, std::vector<std::string>& renderedSheets,
                    std::string& error) {
    renderedSheets.clear();
    zip_t* archive = nullptr;
    if (!pkg::openTemplate(source, archive, error)) return false;
    pkg::ArchiveCloser closer(archive);

    // The archive is opened in both cases -- assembly needs it to copy the
    // untouched parts -- so the identity is cheap to compute here, and a cache
    // hit skips reading and scanning every sheet.
    const std::string key = plan.cache ? cacheKey(archive, plan.sheetName) : std::string();
    std::shared_ptr<const CachedTemplate> structure;
    if (!key.empty()) {
        structure = templateCache().get(key);
    }

    if (!structure) {
        auto built = std::make_shared<CachedTemplate>();
        if (!buildTemplateStructure(archive, plan, *built, error)) return false;
        if (!key.empty()) {
            templateCache().put(key, built);
        }
        structure = std::move(built);
    }

    std::vector<std::string> parts;
    std::vector<std::string> contents;
    for (const CachedSheet& sheet : structure->sheets) {
        RenderContext context{values, plan, structure->sharedStrings, sheet.part, std::string(),
                              true};
        std::string rendered;
        if (!renderSheet(context, sheet, rendered)) {
            error = context.error;
            return false;
        }

        parts.push_back(sheet.part);
        contents.push_back(std::move(rendered));
    }

    std::vector<pkg::Replacement> replacements;
    replacements.reserve(parts.size());
    for (size_t i = 0; i < parts.size(); ++i) {
        replacements.push_back({parts[i], &contents[i], nullptr});
        renderedSheets.push_back(parts[i]);
    }

    return pkg::assemblePackage(archive, replacements, compression, out, error);
}

} // namespace baja_xlsx
