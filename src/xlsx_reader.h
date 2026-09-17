#ifndef XLSX_READER_H
#define XLSX_READER_H

#include <string>
#include <vector>
#include <xlnt/xlnt.hpp>

namespace baja_xlsx {

struct ImageData {
    std::string name;
    std::vector<uint8_t> data;
    std::string type;
};

struct ImagePosition {
    std::string imageName;
    std::string sheetName;
    int fromCol;
    int fromRow;
    int toCol;
    int toRow;
};

// WPS Excel embedded image ID to filename mapping
struct CellImageMapping {
    std::string imageId;      // e.g., "ID_C6F9C8CE7BB34DB9B1BB9835C5297155"
    std::string imageName;    // e.g., "image1.png"
};

// A single cell: plain text plus optional attached image indices.
// Image attachment (business logic) is resolved here in C++ so that the
// N-API boundary layer only converts values (AUDIT-20260917-034).
struct CellValue {
    std::string text;
    std::vector<int> imageIndices; // indices into ExcelData::images
};

struct SheetData {
    std::string name;
    std::vector<std::vector<CellValue>> data;
};

struct ExcelData {
    std::vector<SheetData> sheets;
    std::vector<ImageData> images;
    std::vector<ImagePosition> imagePositions;
    std::vector<CellImageMapping> cellImageMappings;
    // Non-fatal diagnostics (skipped entries, unmapped drawings, ...).
    std::vector<std::string> warnings;
};

class XlsxReader {
public:
    XlsxReader();

    // Reads complete Excel data (sheets + images + attachments).
    // maxRows / maxCols: optional caps on rows/columns read per sheet
    // (0 = no explicit cap beyond the Excel format maximum).
    // Failures are reported via lastError_ as "CODE|message".
    ExcelData readExcel(const std::string& filepath,
                        size_t maxRows = 0,
                        size_t maxCols = 0);

    // Last error in "CODE|message" form; empty when no error occurred.
    std::string getLastError() const { return lastError_; }

private:
    bool load(const std::string& filepath);
    std::vector<SheetData> readSheetData(size_t maxRows, size_t maxCols);
    std::string cellToString(const xlnt::cell& cell);

    xlnt::workbook workbook_;
    std::string lastError_;
    bool loaded_;
    std::vector<std::string> warnings_;
};

} // namespace baja_xlsx

#endif // XLSX_READER_H
