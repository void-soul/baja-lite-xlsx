#ifndef XLSX_TEMPLATE_H
#define XLSX_TEMPLATE_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "xlsx_patch.h"
#include "zip_writer.h"

namespace baja_xlsx {

// One flattened template value. Values stay typed so a numeric placeholder can
// be written as a real number (and keep the cell's number format) instead of
// being stringified.
struct TemplateValue {
    enum class Kind : uint8_t { Text, Number, Boolean };

    Kind kind = Kind::Text;
    std::string text;
    double number = 0;
    bool boolean = false;
};

// Flattened values keyed by path: "title", "user.name", "items.0.amount",
// "items.length". Arrays are flattened by index, which keeps the renderer free
// of any host-language callback.
using TemplateValues = std::unordered_map<std::string, TemplateValue>;

struct TemplatePlan {
    std::string sheetName; // empty = every worksheet
    bool strict = true;    // unknown placeholder -> error instead of empty text
};

// Renders ${path} placeholders and {{#each path}} ... {{/each}} blocks in a
// template workbook.
//
// Repeated rows are produced by copying the template row's XML and renumbering
// it, so formatting, merged cells, row heights and conditional formats survive
// untouched. Only sheets that actually contain markers are regenerated; every
// other part of the package is copied as compressed bytes.
//
// `renderedSheets` receives the part names that were rewritten.
bool renderTemplate(const TemplateSource& source, const TemplateValues& values,
                    const TemplatePlan& plan, zipio::ZipWriter::Compression compression,
                    std::vector<uint8_t>& out, std::vector<std::string>& renderedSheets,
                    std::string& error);

} // namespace baja_xlsx

#endif // XLSX_TEMPLATE_H
