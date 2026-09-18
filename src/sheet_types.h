#ifndef SHEET_TYPES_H
#define SHEET_TYPES_H

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

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

// What a cell holds, next to its display text. Populated by the direct reader
// while scanning; the JS bridge consults it only when the caller asked for
// `values: 'typed'`.
enum CellKind : uint8_t {
    CellKindString = 0,
    CellKindNumber = 1,
    CellKindBoolean = 2,
    CellKindDate = 3
};

// A single cell: plain text plus optional attached image indices.
// Image attachment (business logic) is resolved here in C++ so that the
// N-API boundary layer only converts values (AUDIT-20260917-034).
struct CellValue {
    std::string text;
    std::vector<int> imageIndices; // indices into ExcelData::images
    uint8_t kind = CellKindString;
    // Raw value for numeric / date cells (dates are Excel serials); used by
    // the `values: 'typed'` bridge, where the formatted text would lose data.
    double number = 0;
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

// How the worksheet is read.
enum class ReadEngine {
    Xlnt, // default: xlnt builds the workbook model, then one sheet is read
    Xml   // read the requested sheet straight out of the package (P3)
};

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
    ReadEngine engine = ReadEngine::Xlnt;
    // values: 'typed' - the bridge turns numbers, booleans and dates into real
    // JS values instead of the formatted strings. Only the direct reader
    // populates the cell kinds, so the xlnt engine rejects the combination.
    bool typedValues = false;
};

} // namespace baja_xlsx

#endif // SHEET_TYPES_H
