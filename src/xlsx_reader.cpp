#include "xlsx_reader.h"
#include "image_extractor.h"
#include <algorithm>
#include <sstream>

namespace baja_xlsx {

XlsxReader::XlsxReader() : loaded_(false) {
}

XlsxReader::~XlsxReader() {
}

bool XlsxReader::load(const std::string& filepath) {
    try {
        workbook_.load(filepath);
        loaded_ = true;
        lastError_ = "";
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to load file: ") + e.what();
        loaded_ = false;
        return false;
    }
}

std::string XlsxReader::cellToString(const xlnt::cell& cell) {
    if (cell.has_value()) {
        try {
            switch (cell.data_type()) {
                case xlnt::cell_type::number:
                    return std::to_string(cell.value<double>());
                case xlnt::cell_type::boolean:
                    return cell.value<bool>() ? "true" : "false";
                case xlnt::cell_type::shared_string:
                case xlnt::cell_type::inline_string:
                case xlnt::cell_type::formula_string:
                    return cell.to_string();
                case xlnt::cell_type::date:
                    return cell.to_string();
                default:
                    return cell.to_string();
            }
        } catch (...) {
            return "";
        }
    }
    return "";
}

std::vector<SheetData> XlsxReader::readSheetData() {
    std::vector<SheetData> sheets;
    
    if (!loaded_) {
        lastError_ = "No file loaded";
        return sheets;
    }
    
    try {
        for (auto ws : workbook_) {
            SheetData sheetData;
            sheetData.name = ws.title();
            
            // Check if sheet has any cells
            if (!ws.has_cell(xlnt::cell_reference("A1"))) {
                // Empty sheet
                sheets.push_back(sheetData);
                continue;
            }
            
            // Get sheet dimensions using xlnt 1.6.1 compatible API
            auto maxRow = ws.highest_row();
            auto maxCol = ws.highest_column();
            
            // Start from row 1, column 1 (Excel is 1-based)
            for (xlnt::row_t row = 1; row <= maxRow; ++row) {
                std::vector<std::string> rowData;
                for (xlnt::column_t::index_t col = 1; col <= maxCol.index; ++col) {
                    try {
                        auto cell = ws.cell(xlnt::column_t(col), row);
                        rowData.push_back(cellToString(cell));
                    } catch (...) {
                        // Cell doesn't exist or error accessing it
                        rowData.push_back("");
                    }
                }
                sheetData.data.push_back(rowData);
            }
            
            sheets.push_back(sheetData);
        }
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to read sheet data: ") + e.what();
    }
    
    return sheets;
}

std::string XlsxReader::getImageType(const std::string& filename) {
    std::string lower_filename = filename;
    std::transform(lower_filename.begin(), lower_filename.end(), 
                   lower_filename.begin(), ::tolower);
    
    if (lower_filename.find(".png") != std::string::npos) {
        return "image/png";
    } else if (lower_filename.find(".jpg") != std::string::npos || 
               lower_filename.find(".jpeg") != std::string::npos) {
        return "image/jpeg";
    } else if (lower_filename.find(".gif") != std::string::npos) {
        return "image/gif";
    } else if (lower_filename.find(".bmp") != std::string::npos) {
        return "image/bmp";
    } else if (lower_filename.find(".emf") != std::string::npos) {
        return "image/x-emf";
    } else if (lower_filename.find(".wmf") != std::string::npos) {
        return "image/x-wmf";
    }
    return "application/octet-stream";
}

std::vector<ImageData> XlsxReader::extractImages() {
    std::vector<ImageData> images;
    
    if (!loaded_) {
        lastError_ = "No file loaded";
        return images;
    }
    
    // Use ImageExtractor to directly parse the XLSX ZIP file
    // Note: We need the original file path, which we should store when loading
    // For now, this is a placeholder - the actual extraction will be done in readExcel
    
    lastError_ = "";
    return images;
}

std::vector<ImagePosition> XlsxReader::getImagePositions() {
    std::vector<ImagePosition> positions;
    
    if (!loaded_) {
        lastError_ = "No file loaded";
        return positions;
    }
    
    // Will be populated by ImageExtractor in readExcel
    lastError_ = "";
    return positions;
}

ExcelData XlsxReader::readExcel(const std::string& filepath) {
    ExcelData data;
    
    try {
        if (!load(filepath)) {
            return data;
        }
        
        // Read sheet data using xlnt
        data.sheets = readSheetData();
        
        // Extract images using ImageExtractor (direct ZIP parsing)
        ImageExtractor extractor;
        std::vector<ImageInfo> imageInfos;
        std::vector<DrawingAnchor> anchors;
        
        if (extractor.extractFromXlsx(filepath, imageInfos, anchors)) {
            // Convert ImageInfo to ImageData
            for (const auto& info : imageInfos) {
                ImageData img;
                img.name = info.filename;
                img.data = info.data;
                img.type = info.contentType;
                data.images.push_back(img);
            }
            
            // Convert DrawingAnchor to ImagePosition
            for (const auto& anchor : anchors) {
                ImagePosition pos;
                pos.imageName = anchor.imageName;
                pos.sheetName = anchor.sheetName;
                pos.fromCol = anchor.fromCol;
                pos.fromRow = anchor.fromRow;
                pos.toCol = anchor.toCol;
                pos.toRow = anchor.toRow;
                data.imagePositions.push_back(pos);
            }
        } else {
            lastError_ = extractor.getLastError();
        }
    } catch (const std::exception& e) {
        lastError_ = std::string("Exception in readExcel: ") + e.what();
    } catch (...) {
        lastError_ = "Unknown exception in readExcel";
    }
    
    return data;
}

} // namespace baja_xlsx

