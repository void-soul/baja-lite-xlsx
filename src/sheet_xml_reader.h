#ifndef SHEET_XML_READER_H
#define SHEET_XML_READER_H

#include <string>
#include <vector>

#include "sheet_types.h" // CellValue / SheetData / ReadOptions
#include "xlsx_patch.h"  // TemplateSource

namespace baja_xlsx {

// Status of a direct read. `Unsupported` is not an error: it means the package
// uses something this reader does not model, and the caller should fall back to
// the xlnt path instead of reporting a failure (P3).
enum class DirectReadStatus {
    Ok,
    Unsupported,
    Failed
};

// Reads one worksheet straight out of the package -- shared strings, the style
// table and the sheet XML, nothing else -- producing exactly the same cell texts
// the xlnt path produces (see XlsxReader::cellToString).
//
// Column projection, the read caps and header resolution all happen while
// scanning, so the rows that were not asked for are never parsed. When `sink` is
// non-null, rows are pushed to it in batches of `batchSize` instead of being
// stored in `sheet.data`, exactly like the xlnt path's streaming mode.
//
// `sheet.name` is filled with the worksheet's name as the package declares it.
DirectReadStatus readSheetFromXml(const TemplateSource& source, const ReadOptions& options,
                                  SheetData& sheet, const RowBatchSink* sink, size_t batchSize,
                                  std::vector<std::string>& warnings, std::string& error);

// The sheet names the package declares, read from workbook.xml. Used to validate
// drawing anchors when the workbook model was never loaded.
bool readSheetNames(const TemplateSource& source, std::vector<std::string>& names,
                    std::string& error);

} // namespace baja_xlsx

#endif // SHEET_XML_READER_H
