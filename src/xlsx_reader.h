#ifndef XLSX_READER_H
#define XLSX_READER_H

#include <string>
#include <vector>
#include <map>
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

struct SheetData {
    std::string name;
    std::vector<std::vector<std::string>> data;
};

struct ExcelData {
    std::vector<SheetData> sheets;
    std::vector<ImageData> images;
    std::vector<ImagePosition> imagePositions;
};

class XlsxReader {
public:
    XlsxReader();
    ~XlsxReader();
    
    // Load Excel file
    bool load(const std::string& filepath);
    
    // Read all sheet data
    std::vector<SheetData> readSheetData();
    
    // Extract all images from the workbook
    std::vector<ImageData> extractImages();
    
    // Get image positions in worksheets
    std::vector<ImagePosition> getImagePositions();
    
    // Read complete Excel data (sheets + images + positions)
    ExcelData readExcel(const std::string& filepath);
    
    // Get last error message
    std::string getLastError() const { return lastError_; }

private:
    xlnt::workbook workbook_;
    std::string lastError_;
    bool loaded_;
    
    // Helper function to convert cell value to string
    std::string cellToString(const xlnt::cell& cell);
    
    // Helper function to determine image type from extension
    std::string getImageType(const std::string& filename);
};

} // namespace baja_xlsx

#endif // XLSX_READER_H


