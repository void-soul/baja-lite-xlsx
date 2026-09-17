#include "xlsx_reader.h"
#include "image_extractor.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>

namespace baja_xlsx {

namespace {

// Shortest round-trip formatting for doubles; replaces the previous
// std::to_string(double) which forced 6 fixed decimals
// (AUDIT-20260917-023).
std::string formatDouble(double v) {
    char buf[64];
    auto res = std::to_chars(buf, buf + sizeof(buf), v);
    if (res.ec == std::errc()) {
        return std::string(buf, res.ptr);
    }
    std::snprintf(buf, sizeof(buf), "%g", v);
    return std::string(buf);
}

// Converts an Excel date serial (days since 1899-12-30) to a deterministic
// string. Replaces xlnt's locale-affected cell.to_string() for dates
// (AUDIT-20260917-031).
std::string formatDateSerial(double serial) {
    if (!std::isfinite(serial) || serial < 0) {
        return std::string();
    }
    long long days = static_cast<long long>(std::floor(serial));
    double frac = serial - static_cast<double>(days);
    long long secs = static_cast<long long>(std::llround(frac * 86400.0));
    if (secs >= 86400) {
        secs -= 86400;
        days += 1;
    }

    // Days since 1899-12-30 -> days since 1970-01-01 (Excel serial 25569).
    long long z = days - 25569;
    z += 719468; // Howard Hinnant's civil_from_days offset
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = static_cast<unsigned>(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = static_cast<long long>(yoe) + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1;
    unsigned m = (mp < 10) ? mp + 3 : mp - 9;
    y += (m <= 2) ? 1 : 0;

    unsigned hh = static_cast<unsigned>(secs / 3600);
    unsigned mm = static_cast<unsigned>((secs % 3600) / 60);
    unsigned ss = static_cast<unsigned>(secs % 60);

    char buf[32];
    if (hh || mm || ss) {
        std::snprintf(buf, sizeof(buf), "%04lld-%02u-%02u %02u:%02u:%02u",
                      y, m, d, hh, mm, ss);
    } else {
        std::snprintf(buf, sizeof(buf), "%04lld-%02u-%02u", y, m, d);
    }
    return std::string(buf);
}

inline std::string cellKey(size_t row, size_t col) {
    return std::to_string(row) + "\x1F" + std::to_string(col);
}

} // namespace

XlsxReader::XlsxReader() : loaded_(false) {
}

bool XlsxReader::load(const std::string& filepath) {
    try {
        workbook_.load(filepath);
        loaded_ = true;
        lastError_.clear();
        return true;
    } catch (const std::exception& e) {
        // AUDIT-20260917-001 is addressed by callers on Windows: pass a
        // filesystem path encoded for the platform (see index.js and README).
        lastError_ = std::string("FILE_OPEN_FAILED|Failed to load file: ") + e.what();
        loaded_ = false;
        return false;
    }
}

std::string XlsxReader::cellToString(const xlnt::cell& cell) {
    if (!cell.has_value()) {
        return std::string();
    }
    try {
        switch (cell.data_type()) {
            case xlnt::cell_type::number:
                return formatDouble(cell.value<double>());
            case xlnt::cell_type::boolean:
                return cell.value<bool>() ? "true" : "false";
            case xlnt::cell_type::shared_string:
            case xlnt::cell_type::inline_string:
                return cell.to_string();
            case xlnt::cell_type::formula_string: {
                std::string formula = cell.to_string();
                // DISPIMG marks WPS embedded images:
                // =DISPIMG("ID_xxx", 1) -> "__IMAGE_CELL__:ID_xxx"
                if (formula.find("DISPIMG") != std::string::npos) {
                    size_t idStart = formula.find('"');
                    if (idStart != std::string::npos) {
                        size_t idEnd = formula.find('"', idStart + 1);
                        if (idEnd != std::string::npos) {
                            return "__IMAGE_CELL__:" + formula.substr(idStart + 1, idEnd - idStart - 1);
                        }
                    }
                    warnings_.push_back("DISPIMG formula found but image ID could not be extracted");
                    return "__IMAGE_CELL__";
                }
                return formula;
            }
            case xlnt::cell_type::date: {
                std::string formatted = formatDateSerial(cell.value<double>());
                if (formatted.empty()) {
                    warnings_.push_back("Unsupported date value converted to empty string");
                }
                return formatted;
            }
            default:
                return cell.to_string();
        }
    } catch (...) {
        // No longer fully silent (AUDIT-20260917-008): record a warning.
        warnings_.push_back("Cell value conversion failed; empty string substituted");
        return std::string();
    }
}

std::vector<SheetData> XlsxReader::readSheetData(size_t maxRows, size_t maxCols) {
    std::vector<SheetData> sheets;

    if (!loaded_) {
        lastError_ = "NO_FILE_LOADED|No file loaded";
        return sheets;
    }

    try {
        for (auto ws : workbook_) {
            SheetData sheetData;
            sheetData.name = ws.title();

            if (!ws.has_cell(xlnt::cell_reference("A1"))) {
                sheets.push_back(sheetData);
                continue;
            }

            auto maxRow = ws.highest_row();
            auto maxCol = ws.highest_column();

            // Optional read caps (AUDIT-20260917-010); truncation is
            // reported as a warning, never silent.
            const size_t rowCap = (maxRows > 0 && maxRows < static_cast<size_t>(maxRow))
                                      ? maxRows : static_cast<size_t>(maxRow);
            const size_t colCap = (maxCols > 0 && maxCols < static_cast<size_t>(maxCol.index))
                                      ? maxCols : static_cast<size_t>(maxCol.index);
            if (rowCap < static_cast<size_t>(maxRow)) {
                std::ostringstream oss;
                oss << "Sheet '" << sheetData.name << "' truncated to " << rowCap
                    << " of " << maxRow << " rows (maxRows)";
                warnings_.push_back(oss.str());
            }
            if (colCap < static_cast<size_t>(maxCol.index)) {
                std::ostringstream oss;
                oss << "Sheet '" << sheetData.name << "' truncated to " << colCap
                    << " of " << maxCol.index << " columns (maxCols)";
                warnings_.push_back(oss.str());
            }

            for (size_t row = 1; row <= rowCap; ++row) {
                std::vector<CellValue> rowData;
                rowData.reserve(static_cast<size_t>(colCap));
                for (size_t col = 1; col <= colCap; ++col) {
                    try {
                        auto cell = ws.cell(xlnt::column_t::index_t(col),
                                            static_cast<xlnt::row_t>(row));
                        CellValue value;
                        value.text = cellToString(cell);
                        rowData.push_back(std::move(value));
                    } catch (...) {
                        CellValue value;
                        value.text = "";
                        rowData.push_back(std::move(value));
                    }
                }
                sheetData.data.push_back(std::move(rowData));
            }

            sheets.push_back(std::move(sheetData));
        }
    } catch (const std::exception& e) {
        lastError_ = std::string("READ_FAILED|Failed to read sheet data: ") + e.what();
    }

    return sheets;
}

ExcelData XlsxReader::readExcel(const std::string& filepath,
                                size_t maxRows, size_t maxCols) {
    ExcelData data;

    try {
        if (!load(filepath)) {
            return data;
        }

        data.sheets = readSheetData(maxRows, maxCols);

        ImageExtractor extractor;
        std::vector<ImageInfo> imageInfos;
        std::vector<DrawingAnchor> anchors;
        std::vector<CellImageInfo> cellImages;

        // Fail-open for the image side: sheet data stays usable even when
        // media extraction fails (AUDIT-20260917-019).
        if (!extractor.extractFromXlsx(filepath, imageInfos, anchors,
                                       cellImages, data.warnings)) {
            std::ostringstream oss;
            oss << "Image extraction failed: " << extractor.getLastError();
            data.warnings.push_back(oss.str());
            return data;
        }

        // Register images; exact filename -> index (first wins on
        // duplicates, duplicates are reported).
        std::map<std::string, int> imageIndexByName;
        for (size_t i = 0; i < imageInfos.size(); ++i) {
            ImageData img;
            img.name = imageInfos[i].filename;
            img.data = imageInfos[i].data;
            img.type = imageInfos[i].contentType;
            const std::string& name = img.name;
            if (imageIndexByName.find(name) != imageIndexByName.end()) {
                std::ostringstream oss;
                oss << "Duplicate image filename '" << name << "'; the first occurrence wins";
                data.warnings.push_back(oss.str());
                continue;
            }
            imageIndexByName[name] = static_cast<int>(data.images.size());
            data.images.push_back(std::move(img));
        }

        // Convert anchors to positions.
        for (const auto& anchor : anchors) {
            ImagePosition pos;
            pos.imageName = anchor.imageName;
            pos.sheetName = anchor.sheetName;
            pos.fromCol = anchor.fromCol;
            pos.fromRow = anchor.fromRow;
            pos.toCol = anchor.toCol;
            pos.toRow = anchor.toRow;
            data.imagePositions.push_back(pos);
            if (anchor.sheetName.empty()) {
                data.warnings.push_back(
                    "Drawing anchor for image '" + anchor.imageName +
                    "' could not be mapped to a sheet; image left unattached");
            }
        }

        // WPS DISPIMG: imageId -> image index.
        std::map<std::string, int> wpsIdToImage;
        for (const auto& cellImg : cellImages) {
            CellImageMapping mapping;
            mapping.imageId = cellImg.imageId;
            mapping.imageName = cellImg.imageName;
            data.cellImageMappings.push_back(mapping);

            auto nameIt = imageIndexByName.find(cellImg.imageName);
            if (nameIt != imageIndexByName.end()) {
                wpsIdToImage[cellImg.imageId] = nameIt->second;
            } else {
                std::ostringstream oss;
                oss << "WPS cell image '" << cellImg.imageId
                    << "' references missing media '" << cellImg.imageName << "'";
                data.warnings.push_back(oss.str());
            }
        }

        // Anchor-based attachment: (sheetName, row, col) -> anchor list.
        std::map<std::string, std::vector<const ImagePosition*>> anchorByCell;
        std::map<std::string, int> sheetIndexByName;
        for (size_t s = 0; s < data.sheets.size(); ++s) {
            sheetIndexByName[data.sheets[s].name] = static_cast<int>(s);
        }
        for (const auto& pos : data.imagePositions) {
            if (pos.sheetName.empty()) continue;
            auto sheetIt = sheetIndexByName.find(pos.sheetName);
            if (sheetIt == sheetIndexByName.end()) {
                std::ostringstream oss;
                oss << "Drawing anchor references unknown sheet '" << pos.sheetName << "'";
                data.warnings.push_back(oss.str());
                continue;
            }
            anchorByCell[pos.sheetName + "\x1F" + cellKey(
                static_cast<size_t>(pos.fromRow), static_cast<size_t>(pos.fromCol))]
                .push_back(&pos);
        }

        // Resolve attachments into cells.
        for (size_t s = 0; s < data.sheets.size(); ++s) {
            SheetData& sheet = data.sheets[s];
            for (size_t r = 0; r < sheet.data.size(); ++r) {
                for (size_t c = 0; c < sheet.data[r].size(); ++c) {
                    CellValue& cell = sheet.data[r][c];

                    // 1) WPS DISPIMG markers.
                    if (cell.text.rfind("__IMAGE_CELL__:", 0) == 0) {
                        const std::string imageId = cell.text.substr(15);
                        auto idIt = wpsIdToImage.find(imageId);
                        if (idIt != wpsIdToImage.end()) {
                            cell.imageIndices.push_back(idIt->second);
                            cell.text.clear();
                        } else {
                            std::ostringstream oss;
                            oss << "DISPIMG id '" << imageId
                                << "' has no matching image in sheet '" << sheet.name << "'";
                            data.warnings.push_back(oss.str());
                            cell.text.clear();
                        }
                    } else if (cell.text == "__IMAGE_CELL__") {
                        cell.text.clear();
                    }

                    // 2) Anchor-based images (exact-name lookup only;
                    // the previous substring fuzzy match that could attach
                    // the wrong image is gone -- AUDIT-20260917-014).
                    const std::string key = cellKey(r, c);
                    auto anchorIt = anchorByCell.find(sheet.name + "\x1F" + key);
                    if (anchorIt == anchorByCell.end()) {
                        continue;
                    }
                    for (const ImagePosition* pos : anchorIt->second) {
                        auto imgIt = imageIndexByName.find(pos->imageName);
                        if (imgIt == imageIndexByName.end()) {
                            std::ostringstream oss;
                            oss << "Anchor references missing image '" << pos->imageName
                                << "' in sheet '" << sheet.name << "'";
                            data.warnings.push_back(oss.str());
                            continue;
                        }
                        const int idx = imgIt->second;
                        bool alreadyAttached = false;
                        for (int existing : cell.imageIndices) {
                            if (existing == idx) {
                                alreadyAttached = true;
                                break;
                            }
                        }
                        if (!alreadyAttached) {
                            cell.imageIndices.push_back(idx);
                            cell.text.clear();
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        lastError_ = std::string("READ_FAILED|Exception in readExcel: ") + e.what();
    } catch (...) {
        lastError_ = "READ_FAILED|Unknown exception in readExcel";
    }

    return data;
}

} // namespace baja_xlsx
