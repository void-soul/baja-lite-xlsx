#ifndef XLSX_PATCH_H
#define XLSX_PATCH_H

#include <cstdint>
#include <string>
#include <vector>

#include "xlsx_writer.h"
#include "zip_writer.h"

namespace baja_xlsx {

// Where the workbook to modify comes from. Exactly one of the two is set.
struct TemplateSource {
    const std::string* path = nullptr;
    const std::vector<uint8_t>* bytes = nullptr;

    bool valid() const { return path != nullptr || bytes != nullptr; }
};

// One cell rewrite for updateCells().
struct CellUpdate {
    std::string sheet;        // empty -> first sheet
    std::string cell;         // "B7"
    WriteCell value;
    std::string numberFormat; // optional; appended to the template's styles
};

// Mode 1b: replace the target sheet's data. Only the sheet (and styles.xml, when
// new number formats are needed) is regenerated; every other part of the
// template is copied as compressed bytes.
bool replaceSheetData(const TemplateSource& source, const WritePlan& plan,
                      RowSource& table, zipio::ZipWriter::Compression compression,
                      std::vector<uint8_t>& out, std::string& error);

// Mode 2: rewrite the listed cells only. The sheet is patched in place, the
// rest of the package is copied verbatim.
bool updateCells(const TemplateSource& source, const std::vector<CellUpdate>& updates,
                 zipio::ZipWriter::Compression compression,
                 std::vector<uint8_t>& out, std::string& error);

} // namespace baja_xlsx

#endif // XLSX_PATCH_H
