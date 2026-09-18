#ifndef ZIP_WRITER_H
#define ZIP_WRITER_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <zip.h>

namespace baja_xlsx { namespace zipio {

// A purpose-built ZIP writer for assembling .xlsx packages. It exists because
// the general-purpose update path (open a package, replace parts, close) has to
// recompress parts that did not change and keeps diff bookkeeping for the whole
// archive; for a write-mostly workload that dominates the runtime.
//
// Two properties matter here:
//   * entries are deflated while they are produced, so a huge worksheet never
//     has to exist in memory as uncompressed XML;
//   * an entry of an existing archive can be copied as raw compressed bytes
//     (reusing its CRC, sizes and method), so "change one part" costs O(part)
//     instead of O(package).
class ZipWriter {
public:
    enum class Compression {
        Store = 0,
        Fast = 1,
        Default = 6
    };

    explicit ZipWriter(std::vector<uint8_t>& out) : out_(out) {}

    // Pulls chunks from `producer` until it returns false (an empty chunk is
    // allowed and just means "keep going") and appends the deflated entry.
    bool addStreamedEntry(const std::string& name,
                          const std::function<bool(std::string& chunk)>& producer,
                          Compression compression,
                          std::string& error);

    // Adds an already materialized part (manifests, workbook, styles, ...).
    bool addEntry(const std::string& name, const std::string& data,
                  Compression compression, std::string& error);

    // Copies entry `index` of `source` verbatim: same compressed payload, CRC,
    // sizes and method. Returns false (without touching `error`) when the entry
    // is not copyable, so callers can fall back to regenerating it.
    bool copyEntryFrom(zip_t* source, zip_uint64_t index, const std::string& name,
                       std::string& error);

    // Writes the central directory. Must be called exactly once, last.
    bool finish(std::string& error);

    size_t entryCount() const { return entries_.size(); }

private:
    struct Entry {
        std::string name;
        uint32_t crc = 0;
        uint64_t compressedSize = 0;
        uint64_t uncompressedSize = 0;
        uint64_t headerOffset = 0;
        uint16_t method = 8;
        uint16_t dosTime = 0;
        uint16_t dosDate = 0;
    };

    bool addRaw(const std::string& name, const uint8_t* payload, size_t payloadSize,
                uint32_t crc, uint64_t uncompressedSize, uint16_t method,
                std::string& error);
    void writeLocalHeader(const Entry& entry);
    void writeCentralHeader(const Entry& entry);
    void putUint16(uint16_t value);
    void putUint32(uint32_t value);
    void putBytes(const void* data, size_t size);
    static void dosTimestamp(uint16_t& time, uint16_t& date);

    std::vector<uint8_t>& out_;
    std::vector<Entry> entries_;
};

}} // namespace baja_xlsx::zipio

#endif // ZIP_WRITER_H
