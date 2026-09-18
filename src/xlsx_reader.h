#ifndef XLSX_READER_H
#define XLSX_READER_H

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <xlnt/xlnt.hpp>

#include "sheet_types.h"

namespace baja_xlsx {

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
    // P3: reads the requested sheet straight from the package, without building
    // the workbook model. Returns false when the package uses something that path
    // does not model, in which case the caller falls back to xlnt -- `data` is
    // left untouched in that case.
    bool readDirect(const std::string* filepath, const std::vector<uint8_t>* bytes,
                    const ReadOptions& options, const RowBatchSink* sink, size_t batchSize,
                    ExcelData& data);
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
    // Sheet names read from the package, used to validate drawing anchors when
    // the workbook model was never loaded (the direct path).
    std::vector<std::string> directSheetNames_;
};

} // namespace baja_xlsx

#endif // XLSX_READER_H
