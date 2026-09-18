#include "write_snapshot.h"

namespace baja_xlsx {

void WriteSnapshot::reserve(size_t rows, size_t cells) {
    rowOffsets_.reserve(rows + 1);
    cells_.reserve(cells);
}

uint32_t WriteSnapshot::intern(const std::string& text) {
    auto it = stringIndex_.find(text);
    if (it != stringIndex_.end()) {
        return it->second;
    }
    const uint32_t id = static_cast<uint32_t>(strings_.size());
    strings_.push_back(text);
    stringIndex_.emplace(strings_.back(), id);
    return id;
}

void WriteSnapshot::addRow(const std::vector<WriteCell>& row) {
    if (rowOffsets_.empty()) {
        rowOffsets_.push_back(0);
    }
    for (const WriteCell& cell : row) {
        Slot slot;
        slot.kind = cell.kind;
        slot.number = cell.number;
        slot.boolean = cell.boolean;
        slot.isDate = cell.isDate;
        if (cell.kind == WriteCell::Kind::Text) {
            slot.textIndex = intern(cell.text);
        }
        cells_.push_back(slot);
    }
    rowOffsets_.push_back(static_cast<uint32_t>(cells_.size()));
}

size_t WriteSnapshot::rowCount() const {
    return rowOffsets_.empty() ? 0 : rowOffsets_.size() - 1;
}

bool WriteSnapshot::nextRow(std::vector<WriteCell>& row) {
    row.clear();
    if (cursor_ >= rowCount()) {
        return false;
    }

    const uint32_t begin = rowOffsets_[cursor_];
    const uint32_t end = rowOffsets_[cursor_ + 1];
    ++cursor_;

    row.reserve(end - begin);
    for (uint32_t i = begin; i < end; ++i) {
        const Slot& slot = cells_[i];
        WriteCell cell;
        cell.kind = slot.kind;
        cell.number = slot.number;
        cell.boolean = slot.boolean;
        cell.isDate = slot.isDate;
        if (slot.kind == WriteCell::Kind::Text) {
            cell.text = strings_[slot.textIndex];
        }
        row.push_back(std::move(cell));
    }
    return true;
}

size_t WriteSnapshot::memoryBytes() const {
    size_t total = cells_.size() * sizeof(Slot) +
                   rowOffsets_.size() * sizeof(uint32_t) +
                   strings_.size() * sizeof(std::string);
    for (const std::string& text : strings_) {
        total += text.size();
    }
    return total;
}

} // namespace baja_xlsx
