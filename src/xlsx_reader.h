#ifndef XLSX_READER_H
#define XLSX_READER_H

#include <cstdint>
#include <functional>
#include <map>
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
    // Populated only when column projection is active: the resolved header
    // texts, aligned with the projected columns (empty text for unnamed ones).
    std::vector<std::string> headers;
    // 1-based source column of every projected column; empty when projection
    // is off. Needed to map drawing anchors back onto projected positions.
    std::vector<size_t> projectedColumns;
};

struct ExcelData {
    std::vector<SheetData> sheets;
    std::vector<ImageData> images;
    std::vector<ImagePosition> imagePositions;
    std::vector<CellImageMapping> cellImageMappings;
    // Non-fatal diagnostics (skipped entries, unmapped drawings, ...).
    std::vector<std::string> warnings;
};

// Lookup tables built once from the media parts, so image attachment can
// happen per row while the sheet is being read (P2-1) instead of in a
// post-pass over materialized data.
struct ImageAttachment {
    bool enabled = false;
    std::map<std::string, int> imageIndexByName;
    std::map<std::string, int> wpsIdToImage;
    std::map<std::string, std::vector<const ImagePosition*>> anchorByCell;
};

// Streaming sink: batches of rows are handed over while the sheet is read.
// Returning false stops the read (P2-1).
using RowBatchSink = std::function<bool(std::vector<std::vector<CellValue>>&&)>;

// Everything that steers a read. Keeping it in C++ means work the caller does
// not ask for is never performed: only the requested sheet is materialized,
// only the requested columns are read, and the image pipeline can be skipped
// entirely.
struct ReadOptions {
    std::string sheetName;            // empty -> first sheet
    size_t headerRow = 0;             // 0-based; used by column projection
    size_t maxRows = 0;               // 0 = no cap beyond the format maximum
    size_t maxCols = 0;
    bool includeImages = true;        // false -> skip the whole image pipeline
    std::vector<std::string> columns; // empty -> every column
};

class XlsxReader {
public:
    XlsxReader();

    // Reads the requested worksheet plus its images. Failures are reported via
    // lastError_ as "CODE|message".
    ExcelData readExcel(const std::string& filepath, const ReadOptions& options);

    // Same, for an in-memory package (P0-4: no temporary file).
    ExcelData readExcel(const std::vector<uint8_t>& bytes, const ReadOptions& options);

    // Streaming variants (P2-1): rows are pushed to `sink` in batches of
    // `batchSize` and never accumulate, so memory stays flat on huge sheets.
    // `data` receives images and warnings; rowCount is counted by the sink.
    void readExcelStreamed(const std::string& filepath, const ReadOptions& options,
                           const RowBatchSink& sink, size_t batchSize, ExcelData& data);
    void readExcelStreamed(const std::vector<uint8_t>& bytes, const ReadOptions& options,
                           const RowBatchSink& sink, size_t batchSize, ExcelData& data);

    // Last error in "CODE|message" form; empty when no error occurred.
    std::string getLastError() const { return lastError_; }

private:
    bool load(const std::string& filepath);
    bool load(const std::vector<uint8_t>& bytes);
    void runPipeline(const std::string* filepath, const std::vector<uint8_t>* bytes,
                     const ReadOptions& options, const RowBatchSink* sink,
                     size_t batchSize, ExcelData& data);
    bool readRequestedSheet(const ReadOptions& options, const ImageAttachment& attach,
                            const RowBatchSink* sink, size_t batchSize, ExcelData& data);
    // Exactly one of `filepath` / `bytes` is non-null.
    void readImages(const std::string* filepath, const std::vector<uint8_t>* bytes,
                    const ReadOptions& options, ExcelData& data, ImageAttachment& attach);
    // Attaches WPS DISPIMG and anchor images to a single row.
    void attachRowImages(const std::string& sheetName,
                         const std::vector<size_t>& projectedColumns,
                         size_t rowIndex, std::vector<CellValue>& row,
                         const ImageAttachment& attach);
    // When `sink` is null rows are buffered into `data`; otherwise they are
    // pushed to the sink in batches of `batchSize` (P2-1).
    void readSheet(xlnt::worksheet ws, const ReadOptions& options,
                   const ImageAttachment& attach, const RowBatchSink* sink,
                   size_t batchSize, ExcelData& data);
    // Maps options.columns onto 1-based column indices and fills
    // sheet.headers. Returns an empty vector when projection is off.
    std::vector<size_t> resolveColumns(xlnt::worksheet ws, const ReadOptions& options,
                                       SheetData& sheet);
    std::string cellToString(const xlnt::cell& cell);

    xlnt::workbook workbook_;
    std::string lastError_;
    bool loaded_;
    std::vector<std::string> warnings_;
};

} // namespace baja_xlsx

#endif // XLSX_READER_H
