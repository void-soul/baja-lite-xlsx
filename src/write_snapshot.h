#ifndef WRITE_SNAPSHOT_H
#define WRITE_SNAPSHOT_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "xlsx_writer.h"

namespace baja_xlsx {

// Compact, N-API-free copy of a table: identical strings are stored once and
// cells reference them by index, so a cell costs 16 bytes plus its share of the
// unique strings. That is what lets a worker thread generate the workbook
// without touching any JS value (the async write entry points), and it is far
// smaller than the JS array it was built from.
class WriteSnapshot : public RowSource {
public:
    // Called on the main thread, one call per source row.
    void addRow(const std::vector<WriteCell>& row);
    void reserve(size_t rows, size_t cells);

    size_t rowCount() const override;
    bool nextRow(std::vector<WriteCell>& row) override;

    // Rewinds the pull cursor, so the same snapshot can be written once and
    // (after a failure, say) retried.
    void reset() { cursor_ = 0; }

    size_t cellCount() const { return cells_.size(); }
    size_t uniqueStrings() const { return strings_.size(); }
    size_t memoryBytes() const;

private:
    struct Slot {
        WriteCell::Kind kind = WriteCell::Kind::Empty;
        bool isDate = false;
        bool boolean = false;
        double number = 0;
        uint32_t textIndex = 0;
    };

    uint32_t intern(const std::string& text);

    std::vector<std::string> strings_;
    std::unordered_map<std::string, uint32_t> stringIndex_;
    std::vector<Slot> cells_;
    std::vector<uint32_t> rowOffsets_; // one entry per row start, plus a final end
    size_t cursor_ = 0;                // consumed rows, for the pull interface
};

} // namespace baja_xlsx

#endif // WRITE_SNAPSHOT_H
