#include "xlsx_reader.h"
#include "a1_reference.h"
#include "cell_format.h"
#include "image_extractor.h"
#include "number_format.h"
#include "path_util.h"
#include "sheet_xml_reader.h"
#include "zip_reader.h"
#if defined(_WIN32)
#include <filesystem>
#endif
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>

namespace baja_xlsx {

namespace {

// formatDouble / formatDateSerial / formatTimeOfDay live in cell_format.cpp,
// shared with the direct XML reader so both produce identical text.

inline std::string cellKey(size_t row, size_t col) {
    return std::to_string(row) + "\x1F" + std::to_string(col);
}

// xlnt decides "this cell is a date" from its number format, but only when its
// own format parser recognises the code, and it never recognises a custom code
// it has not seen before. Files written by other tools -- and by our writer's
// custom formats -- then come back as raw serials. These helpers read the
// format code directly instead, which also keeps dates readable in workbooks we
// did not produce.
std::string numberFormatCode(const xlnt::cell& cell) {
    try {
        if (!cell.has_format()) return std::string();
        return cell.format().number_format().format_string();
    } catch (...) {
        return std::string();
    }
}

// xlnt only exposes the wide overload on MSVC; elsewhere the UTF-8 path is what
// the narrow API already expects. Without this, a UTF-8 path on Windows is read
// as ANSI and every non-ASCII file name fails to open.
void loadWorkbookFromPath(xlnt::workbook& workbook, const std::string& utf8Path) {
#if defined(_WIN32)
    workbook.load(std::filesystem::u8path(utf8Path).wstring());
#else
    workbook.load(utf8Path);
#endif
}

void trimInPlace(std::string& text) {
    const char* kSpace = " \t\r\n";
    const size_t first = text.find_first_not_of(kSpace);
    if (first == std::string::npos) {
        text.clear();
        return;
    }
    const size_t last = text.find_last_not_of(kSpace);
    text = text.substr(first, last - first + 1);
}

// parseColumnLetters / parseColumnReference / columnLetters live in
// a1_reference.cpp, shared with every other module.

} // namespace

XlsxReader::XlsxReader() : loaded_(false) {
}

bool XlsxReader::load(const std::string& filepath) {
    // Capture ANY failure of the first attempt: xlnt can also throw
    // non-std::exception types, and only catching both lets the sanitized
    // fallback below actually run.
    std::string firstError;
    bool firstOk = false;
    try {
        loadWorkbookFromPath(workbook_, filepath);
        firstOk = true;
    } catch (const std::exception& e) {
        firstError = e.what();
    } catch (...) {
        firstError = "unknown non-std exception";
    }

    if (firstOk) {
        loaded_ = true;
        lastError_.clear();
        return true;
    }

    // Vendor extensions break xlnt: WPS writes proprietary relationship
    // types (e.g. http://www.wps.cn/officeDocument/2020/cellImage) and
    // backslash ZIP entry names, which make it abort with "key not found in
    // container", rendering valid WPS files unreadable. Retry through a
    // sanitized copy that normalizes entry names and keeps only standard
    // relationship types. Images are still extracted from the ORIGINAL file,
    // so nothing is lost there.
    std::string tempPath;
    std::string sanitizeError;
    std::string retryNote;
    if (zipio::createSanitizedCopy(filepath, tempPath, sanitizeError)) {
        bool retryOk = false;
        try {
            loadWorkbookFromPath(workbook_, tempPath);
            retryOk = true;
        } catch (const std::exception& secondError) {
            retryNote = std::string("sanitized copy still rejected: ") + secondError.what();
        } catch (...) {
            retryNote = "sanitized copy still rejected (unknown non-std exception)";
        }
        pathutil::removeFile(tempPath);
        if (retryOk) {
            loaded_ = true;
            lastError_.clear();
            warnings_.push_back(
                "Loaded via sanitized copy: normalized entry names and stripped "
                "non-standard relationship types / content types "
                "(xlnt error: " + firstError + ")");
            return true;
        }
    } else {
        retryNote = "sanitized copy not created: " + sanitizeError;
    }

    // AUDIT-20260917-001 is addressed by callers on Windows: pass a
    // filesystem path encoded for the platform (see index.js and README).
    lastError_ = std::string("FILE_OPEN_FAILED|Failed to load file: ") + firstError +
                 " [" + retryNote + "]";
    loaded_ = false;
    return false;
}

bool XlsxReader::load(const std::vector<uint8_t>& bytes) {
    // P0-4: Buffer / base64 input is parsed straight from memory, so no
    // temporary file is written for the common case.
    std::string firstError;
    bool firstOk = false;
    try {
        workbook_.load(bytes);
        firstOk = true;
    } catch (const std::exception& e) {
        firstError = e.what();
    } catch (...) {
        firstError = "unknown non-std exception";
    }

    if (firstOk) {
        loaded_ = true;
        lastError_.clear();
        return true;
    }

    // Only the WPS sanitizing workaround needs a real file; stage one just
    // for that rare fallback.
    std::string tempPath;
    std::string stageError;
    if (zipio::writeTempWorkbook(bytes, tempPath, stageError)) {
        const bool ok = load(tempPath);
        pathutil::removeFile(tempPath);
        return ok;
    }

    lastError_ = std::string("FILE_OPEN_FAILED|Failed to load workbook data: ") + firstError +
                 " [" + stageError + "]";
    loaded_ = false;
    return false;
}

std::string XlsxReader::cellToString(const xlnt::cell& cell) {
    if (!cell.has_value()) {
        return std::string();
    }
    try {
        switch (cell.data_type()) {
            case xlnt::cell_type::number: {
                // Trust the format code rather than xlnt's classification, so
                // dates survive a round trip through a custom format.
                const FormatTokens tokens = scanFormatTokens(numberFormatCode(cell));
                const double value = cell.value<double>();
                if (tokens.date) {
                    return formatDateSerial(value);
                }
                if (tokens.time) {
                    return formatTimeOfDay(value);
                }
                return formatDouble(value);
            }
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
                const FormatTokens tokens = scanFormatTokens(numberFormatCode(cell));
                const double value = cell.value<double>();
                if (tokens.time && !tokens.date) {
                    // A time-only format should not be rendered with a date.
                    return formatTimeOfDay(value);
                }
                std::string formatted = formatDateSerial(value);
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

std::vector<size_t> XlsxReader::resolveColumns(xlnt::worksheet ws,
                                               const ReadOptions& options,
                                               SheetData& sheet) {
    std::vector<size_t> selected;
    if (options.columns.empty()) {
        return selected; // projection off: read every column
    }

    const size_t maxCol = static_cast<size_t>(ws.highest_column().index);
    const xlnt::row_t headerIndex = static_cast<xlnt::row_t>(options.headerRow + 1);

    // Header texts are only read once and only for the header row; every other
    // row then touches the requested columns exclusively.
    std::vector<std::string> headers(maxCol + 1);
    std::map<std::string, size_t> byHeader;
    for (size_t col = 1; col <= maxCol; ++col) {
        std::string text;
        xlnt::cell_reference ref(xlnt::column_t::index_t(col), headerIndex);
        if (ws.has_cell(ref)) {
            try {
                text = cellToString(ws.cell(ref));
            } catch (...) {
                text.clear();
            }
        }
        trimInPlace(text);
        headers[col] = text;
        if (!text.empty()) {
            byHeader.emplace(text, col);
        }
    }

    std::vector<size_t> ordered;
    for (const std::string& raw : options.columns) {
        std::string item = raw;
        trimInPlace(item);
        if (item.empty()) continue;

        auto headerIt = byHeader.find(item);
        if (headerIt != byHeader.end()) {
            ordered.push_back(headerIt->second);
            continue;
        }

        size_t first = 0;
        size_t last = 0;
        if (parseColumnReference(item, first, last)) {
            if (first > maxCol) {
                warnings_.push_back("Column '" + raw + "' lies outside the sheet (last column is " +
                                    columnLetters(maxCol) + "); skipped");
                continue;
            }
            for (size_t col = first; col <= std::min(last, maxCol); ++col) {
                ordered.push_back(col);
            }
            continue;
        }

        warnings_.push_back("Column '" + raw + "' matches no header text and is not a valid "
                            "column reference; skipped");
    }

    std::set<size_t> seen;
    for (size_t col : ordered) {
        if (col >= 1 && col <= maxCol && seen.insert(col).second) {
            selected.push_back(col);
        }
    }

    if (selected.empty()) {
        lastError_ = "INVALID_OPTIONS|None of the requested columns were found in the sheet";
        return selected;
    }

    for (size_t col : selected) {
        sheet.headers.push_back(headers[col]);
    }
    sheet.projectedColumns = selected;
    return selected;
}

void XlsxReader::readSheet(xlnt::worksheet ws, const ReadOptions& options,
                           const ImageAttachment& attach, const RowBatchSink* sink,
                           size_t batchSize, ExcelData& data) {
    const bool streaming = (sink != nullptr);
    SheetData sheet;
    sheet.name = ws.title();

    if (!ws.has_cell(xlnt::cell_reference("A1"))) {
        data.sheets.push_back(std::move(sheet));
        return;
    }

    const size_t maxRow = static_cast<size_t>(ws.highest_row());
    const size_t maxCol = static_cast<size_t>(ws.highest_column().index);

    // Optional read caps (AUDIT-20260917-010); truncation is
    // reported as a warning, never silent.
    const size_t rowCap = (options.maxRows > 0 && options.maxRows < maxRow)
                              ? options.maxRows : maxRow;
    const size_t colCap = (options.maxCols > 0 && options.maxCols < maxCol)
                              ? options.maxCols : maxCol;
    if (rowCap < maxRow) {
        std::ostringstream oss;
        oss << "Sheet '" << sheet.name << "' truncated to " << rowCap
            << " of " << maxRow << " rows (maxRows)";
        warnings_.push_back(oss.str());
    }
    if (colCap < maxCol) {
        std::ostringstream oss;
        oss << "Sheet '" << sheet.name << "' truncated to " << colCap
            << " of " << maxCol << " columns (maxCols)";
        warnings_.push_back(oss.str());
    }

    const std::vector<size_t> projected = resolveColumns(ws, options, sheet);
    const bool useProjection = !projected.empty();
    const size_t colCount = useProjection ? projected.size() : colCap;

    const size_t effectiveBatch = streaming ? (batchSize > 0 ? batchSize : 1000) : 0;
    std::vector<std::vector<CellValue>> batch;
    if (streaming) {
        batch.reserve(effectiveBatch);
    }

    bool stopped = false;
    for (size_t row = 1; row <= rowCap && !stopped; ++row) {
        std::vector<CellValue> rowData;
        rowData.reserve(colCount);
        const xlnt::row_t xlRow = static_cast<xlnt::row_t>(row);

        for (size_t i = 0; i < colCount; ++i) {
            const size_t col = useProjection ? projected[i] : (i + 1);
            CellValue value;

            // P0-2: probe before touching. worksheet::cell() creates -- and
            // keeps -- an empty cell for every coordinate that does not exist
            // yet, which is the dominant cost on sparse sheets and inflates
            // memory with cells that hold nothing.
            xlnt::cell_reference ref(xlnt::column_t::index_t(col), xlRow);
            if (ws.has_cell(ref)) {
                try {
                    value.text = cellToString(ws.cell(ref));
                } catch (...) {
                    value.text.clear();
                }
            }
            rowData.push_back(std::move(value));
        }

        // Images are attached here rather than in a post-pass, so streaming
        // never needs the whole sheet in memory (P2-1).
        attachRowImages(sheet.name, sheet.projectedColumns, row - 1, rowData, attach);

        if (!streaming) {
            sheet.data.push_back(std::move(rowData));
            continue;
        }

        batch.push_back(std::move(rowData));
        if (batch.size() >= effectiveBatch) {
            if (!(*sink)(std::move(batch))) {
                stopped = true;
            }
            batch.clear();
            batch.reserve(effectiveBatch);
        }
    }

    if (streaming && !stopped && !batch.empty()) {
        (*sink)(std::move(batch));
    }

    data.sheets.push_back(std::move(sheet));
}

void XlsxReader::attachRowImages(const std::string& sheetName,
                                 const std::vector<size_t>& projectedColumns,
                                 size_t rowIndex, std::vector<CellValue>& row,
                                 const ImageAttachment& attach) {
    if (!attach.enabled) {
        return;
    }

    for (size_t c = 0; c < row.size(); ++c) {
        CellValue& cell = row[c];

        // 1) WPS DISPIMG markers.
        if (cell.text.rfind("__IMAGE_CELL__:", 0) == 0) {
            const std::string imageId = cell.text.substr(15);
            auto idIt = attach.wpsIdToImage.find(imageId);
            if (idIt != attach.wpsIdToImage.end()) {
                cell.imageIndices.push_back(idIt->second);
                cell.text.clear();
            } else {
                std::ostringstream oss;
                oss << "DISPIMG id '" << imageId
                    << "' has no matching image in sheet '" << sheetName << "'";
                warnings_.push_back(oss.str());
                cell.text.clear();
            }
        } else if (cell.text == "__IMAGE_CELL__") {
            cell.text.clear();
        }

        // 2) Anchor-based images (exact-name lookup only; the previous
        // substring fuzzy match that could attach the wrong image is gone --
        // AUDIT-20260917-014). With column projection `c` is a projected
        // position, so translate it back to the source column index.
        const size_t sourceCol = projectedColumns.empty() ? c : (projectedColumns[c] - 1);
        const std::string key = cellKey(rowIndex, sourceCol);
        auto anchorIt = attach.anchorByCell.find(sheetName + "\x1F" + key);
        if (anchorIt == attach.anchorByCell.end()) {
            continue;
        }
        for (const ImagePosition* pos : anchorIt->second) {
            auto imgIt = attach.imageIndexByName.find(pos->imageName);
            if (imgIt == attach.imageIndexByName.end()) {
                std::ostringstream oss;
                oss << "Anchor references missing image '" << pos->imageName
                    << "' in sheet '" << sheetName << "'";
                warnings_.push_back(oss.str());
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

bool XlsxReader::readRequestedSheet(const ReadOptions& options,
                                    const ImageAttachment& attach,
                                    const RowBatchSink* sink, size_t batchSize,
                                    ExcelData& data) {
    // P0-1: only the worksheet the caller asked for is materialized.
    xlnt::worksheet ws;
    if (options.sheetName.empty()) {
        if (workbook_.sheet_count() == 0) {
            lastError_ = "NO_SHEETS|Excel file has no sheets";
            return false;
        }
        ws = workbook_.sheet_by_index(0);
    } else if (workbook_.contains(options.sheetName)) {
        ws = workbook_.sheet_by_title(options.sheetName);
    } else {
        lastError_ = "SHEET_NOT_FOUND|Sheet \"" + options.sheetName + "\" not found";
        return false;
    }

    readSheet(ws, options, attach, sink, batchSize, data);
    // Cell-level diagnostics (truncation, attachment failures, ...). Appended
    // rather than assigned so warnings from image extraction survive.
    data.warnings.insert(data.warnings.end(), warnings_.begin(), warnings_.end());
    return lastError_.empty();
}

void XlsxReader::readImages(const std::string* filepath,
                            const std::vector<uint8_t>* bytes,
                            const ReadOptions& options, ExcelData& data,
                            ImageAttachment& attach) {
    // P0-3: the image pipeline costs a second full pass over the archive.
    // Workbooks without media parts skip it entirely, and callers can
    // always opt out explicitly with includeImages: false.
    if (!options.includeImages) {
        return;
    }
    attach.enabled = true;
    if (filepath ? !zipio::packageHasMedia(*filepath)
                 : !zipio::packageHasMedia(*bytes)) {
        return;
    }

    ImageExtractor extractor;
    std::vector<ImageInfo> imageInfos;
    std::vector<DrawingAnchor> anchors;
    std::vector<CellImageInfo> cellImages;

    // Fail-open for the image side: sheet data stays usable even when
    // media extraction fails (AUDIT-20260917-019).
    const bool extracted = filepath
        ? extractor.extractFromXlsx(*filepath, imageInfos, anchors, cellImages, data.warnings)
        : extractor.extractFromMemory(*bytes, imageInfos, anchors, cellImages, data.warnings);
    if (!extracted) {
        std::ostringstream oss;
        oss << "Image extraction failed: " << extractor.getLastError();
        data.warnings.push_back(oss.str());
        return;
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
        // Built up-front so each row can be attached as it is read, which is
        // what lets streaming stay flat in memory (P2-1).
        for (const auto& pos : data.imagePositions) {
            if (pos.sheetName.empty()) continue;
            // The workbook model is only loaded on the xlnt path; the direct path
            // validates against the names read from the package.
            const bool known =
                loaded_ ? workbook_.contains(pos.sheetName)
                        : std::find(directSheetNames_.begin(), directSheetNames_.end(),
                                    pos.sheetName) != directSheetNames_.end();
            if (!known) {
                std::ostringstream oss;
                oss << "Drawing anchor references unknown sheet '" << pos.sheetName << "'";
                data.warnings.push_back(oss.str());
                continue;
            }
            attach.anchorByCell[pos.sheetName + "\x1F" + cellKey(
                static_cast<size_t>(pos.fromRow), static_cast<size_t>(pos.fromCol))]
                .push_back(&pos);
        }

        attach.imageIndexByName = imageIndexByName;
        attach.wpsIdToImage = wpsIdToImage;
}

namespace {

// Shared tail: post-processing failures must never discard table data that
// was read successfully (AUDIT-20260917-019).
void recordReadFailure(const std::string& message, bool hasSheets,
                       std::string& lastError, std::vector<std::string>& warnings) {
    if (hasSheets) {
        warnings.push_back(message);
    } else {
        lastError = message;
    }
}

} // namespace

// Shared pipeline: images first (so rows can be attached while they are
// produced), then the requested worksheet -- either buffered or streamed.
void XlsxReader::runPipeline(const std::string* filepath,
                             const std::vector<uint8_t>* bytes,
                             const ReadOptions& options,
                             const RowBatchSink* sink, size_t batchSize,
                             ExcelData& data) {
    ImageAttachment attach;
    readImages(filepath, bytes, options, data, attach);
    readRequestedSheet(options, attach, sink, batchSize, data);
}

bool XlsxReader::readDirect(const std::string* filepath, const std::vector<uint8_t>* bytes,
                            const ReadOptions& options, const RowBatchSink* sink, size_t batchSize,
                            ExcelData& data) {
    TemplateSource source;
    if (filepath) {
        source.path = filepath;
    } else {
        source.bytes = bytes;
    }

    // Streamed rows are handed over as they are produced, so images would have to
    // be attached inside the scan; the xlnt path already does that, so an image
    // read stays with it (see the note in README).
    if (sink != nullptr && options.includeImages) {
        return false;
    }

    std::string error;
    if (!readSheetNames(source, directSheetNames_, error)) {
        directSheetNames_.clear();
        return false;
    }

    // Cell-level diagnostics come first, then the image pipeline appends its own.
    warnings_.clear();

    SheetData sheet;
    std::vector<std::string> warnings;
    const DirectReadStatus status =
        readSheetFromXml(source, options, sheet, sink, batchSize, warnings, error);
    if (status == DirectReadStatus::Unsupported) {
        // Something in the package needs the full model: leave no trace behind.
        data = ExcelData();
        directSheetNames_.clear();
        warnings_.clear();
        return false;
    }
    if (status == DirectReadStatus::Failed) {
        lastError_ = error;
        return true;
    }

    warnings_.insert(warnings_.end(), warnings.begin(), warnings.end());
    data.warnings = warnings_;

    ImageAttachment attach;
    readImages(filepath, bytes, options, data, attach);

    // Images are attached per row, exactly like the xlnt path, so the streaming
    // and buffered shapes stay identical.
    if (attach.enabled) {
        for (size_t r = 0; r < sheet.data.size(); ++r) {
            attachRowImages(sheet.name, sheet.projectedColumns, r, sheet.data[r], attach);
        }
    }

    if (sink == nullptr) {
        data.sheets.push_back(std::move(sheet));
    } else {
        SheetData header;
        header.name = sheet.name;
        header.headers = sheet.headers;
        header.projectedColumns = sheet.projectedColumns;
        data.sheets.push_back(std::move(header));
    }
    return true;
}

ExcelData XlsxReader::readExcel(const std::string& filepath,
                                const ReadOptions& options) {
    ExcelData data;
    if (options.engine == ReadEngine::Xml &&
        readDirect(&filepath, nullptr, options, nullptr, 0, data)) {
        return data;
    }
    try {
        if (!load(filepath)) {
            return data;
        }
        runPipeline(&filepath, nullptr, options, nullptr, 0, data);
    } catch (const std::exception& e) {
        recordReadFailure(
            std::string("READ_FAILED|Exception while reading the workbook: ") + e.what(),
            !data.sheets.empty(), lastError_, data.warnings);
    } catch (...) {
        recordReadFailure(
            "READ_FAILED|Unknown non-std exception while reading the workbook",
            !data.sheets.empty(), lastError_, data.warnings);
    }
    return data;
}

ExcelData XlsxReader::readExcel(const std::vector<uint8_t>& bytes,
                                const ReadOptions& options) {
    ExcelData data;
    if (options.engine == ReadEngine::Xml &&
        readDirect(nullptr, &bytes, options, nullptr, 0, data)) {
        return data;
    }
    try {
        if (!load(bytes)) {
            return data;
        }
        runPipeline(nullptr, &bytes, options, nullptr, 0, data);
    } catch (const std::exception& e) {
        recordReadFailure(
            std::string("READ_FAILED|Exception while reading the workbook: ") + e.what(),
            !data.sheets.empty(), lastError_, data.warnings);
    } catch (...) {
        recordReadFailure(
            "READ_FAILED|Unknown non-std exception while reading the workbook",
            !data.sheets.empty(), lastError_, data.warnings);
    }
    return data;
}

void XlsxReader::readExcelStreamed(const std::string& filepath, const ReadOptions& options,
                                   const RowBatchSink& sink, size_t batchSize,
                                   ExcelData& data) {
    if (options.engine == ReadEngine::Xml &&
        readDirect(&filepath, nullptr, options, &sink, batchSize, data)) {
        return;
    }
    try {
        if (!load(filepath)) {
            return;
        }
        runPipeline(&filepath, nullptr, options, &sink, batchSize, data);
    } catch (const std::exception& e) {
        recordReadFailure(
            std::string("READ_FAILED|Exception while reading the workbook: ") + e.what(),
            !data.sheets.empty(), lastError_, data.warnings);
    } catch (...) {
        recordReadFailure(
            "READ_FAILED|Unknown non-std exception while reading the workbook",
            !data.sheets.empty(), lastError_, data.warnings);
    }
}

void XlsxReader::readExcelStreamed(const std::vector<uint8_t>& bytes, const ReadOptions& options,
                                   const RowBatchSink& sink, size_t batchSize,
                                   ExcelData& data) {
    if (options.engine == ReadEngine::Xml &&
        readDirect(nullptr, &bytes, options, &sink, batchSize, data)) {
        return;
    }
    try {
        if (!load(bytes)) {
            return;
        }
        runPipeline(nullptr, &bytes, options, &sink, batchSize, data);
    } catch (const std::exception& e) {
        recordReadFailure(
            std::string("READ_FAILED|Exception while reading the workbook: ") + e.what(),
            !data.sheets.empty(), lastError_, data.warnings);
    } catch (...) {
        recordReadFailure(
            "READ_FAILED|Unknown non-std exception while reading the workbook",
            !data.sheets.empty(), lastError_, data.warnings);
    }
}

} // namespace baja_xlsx
