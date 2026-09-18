#include "zip_writer.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <zlib.h>

namespace baja_xlsx { namespace zipio {

namespace {

const uint32_t kLocalHeaderSignature = 0x04034b50u;
const uint32_t kCentralHeaderSignature = 0x02014b50u;
const uint32_t kEndOfCentralDirectorySignature = 0x06054b50u;
const uint16_t kVersionNeeded = 20;      // 2.0: deflate + UTF-8 names
const uint16_t kFlagUtf8Names = 0x0800;  // bit 11: file name is UTF-8
const uint16_t kMethodStore = 0;
const uint16_t kMethodDeflate = 8;
const uint64_t kMaxZip32 = 0xFFFFFFFFull;

// zlib's deflate with a raw window (-MAX_WBITS) is exactly what ZIP expects.
int compressionLevel(ZipWriter::Compression compression) {
    return static_cast<int>(compression);
}

} // namespace

void ZipWriter::putUint16(uint16_t value) {
    out_.push_back(static_cast<uint8_t>(value & 0xFF));
    out_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void ZipWriter::putUint32(uint32_t value) {
    out_.push_back(static_cast<uint8_t>(value & 0xFF));
    out_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void ZipWriter::putBytes(const void* data, size_t size) {
    if (size == 0) return;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    out_.insert(out_.end(), bytes, bytes + size);
}

void ZipWriter::dosTimestamp(uint16_t& time, uint16_t& date) {
    std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    const int year = local.tm_year + 1900;
    time = static_cast<uint16_t>((local.tm_hour << 11) | (local.tm_min << 5) |
                                 (local.tm_sec / 2));
    date = static_cast<uint16_t>(((year < 1980 ? 0 : year - 1980) << 9) |
                                 ((local.tm_mon + 1) << 5) | local.tm_mday);
}

bool ZipWriter::addEntry(const std::string& name, const std::string& data,
                         Compression compression, std::string& error) {
    const size_t size = data.size();
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(data.data());
    const uint32_t crc = static_cast<uint32_t>(
        crc32(0L, Z_NULL, 0));
    const uint32_t checksum = size == 0
        ? crc
        : static_cast<uint32_t>(crc32(crc, reinterpret_cast<const Bytef*>(raw),
                                      static_cast<uInt>(size)));

    if (compression == Compression::Store || size == 0) {
        return addRaw(name, raw, size, checksum, size, kMethodStore, error);
    }

    uLongf bound = compressBound(static_cast<uLong>(size));
    std::vector<uint8_t> compressed(bound);
    const int rc = compress2(compressed.data(), &bound,
                             reinterpret_cast<const Bytef*>(raw),
                             static_cast<uLong>(size),
                             compressionLevel(compression));
    if (rc != Z_OK) {
        error = "Failed to deflate '" + name + "'";
        return false;
    }
    compressed.resize(bound);
    if (compressed.size() >= size) {
        return addRaw(name, raw, size, checksum, size, kMethodStore, error);
    }
    return addRaw(name, compressed.data(), compressed.size(), checksum, size,
                  kMethodDeflate, error);
}

bool ZipWriter::addStreamedEntry(const std::string& name,
                                 const std::function<bool(std::string& chunk)>& producer,
                                 Compression compression, std::string& error) {
    std::vector<uint8_t> compressed;
    compressed.reserve(64 * 1024);

    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (deflateInit2(&zs, compressionLevel(compression), Z_DEFLATED, -MAX_WBITS, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK) {
        error = "Failed to initialize deflate";
        return false;
    }

    char outBuffer[64 * 1024];
    uint32_t checksum = static_cast<uint32_t>(crc32(0L, Z_NULL, 0));
    uint64_t uncompressedSize = 0;
    bool ok = true;
    bool finished = false;
    std::string chunk;

    while (ok && !finished) {
        chunk.clear();
        const bool more = producer(chunk);

        if (!chunk.empty()) {
            uncompressedSize += chunk.size();
            checksum = static_cast<uint32_t>(crc32(
                checksum, reinterpret_cast<const Bytef*>(chunk.data()),
                static_cast<uInt>(chunk.size())));

            zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(chunk.data()));
            zs.avail_in = static_cast<uInt>(chunk.size());
            while (ok && zs.avail_in > 0) {
                zs.next_out = reinterpret_cast<Bytef*>(outBuffer);
                zs.avail_out = static_cast<uInt>(sizeof(outBuffer));
                if (deflate(&zs, Z_NO_FLUSH) != Z_OK) {
                    ok = false;
                    break;
                }
                const size_t produced = sizeof(outBuffer) - zs.avail_out;
                compressed.insert(compressed.end(), outBuffer, outBuffer + produced);
            }
        }

        if (!more) {
            finished = true;
        }
    }

    if (ok) {
        int rc = Z_OK;
        while (rc != Z_STREAM_END) {
            zs.next_out = reinterpret_cast<Bytef*>(outBuffer);
            zs.avail_out = static_cast<uInt>(sizeof(outBuffer));
            rc = deflate(&zs, Z_FINISH);
            if (rc != Z_OK && rc != Z_STREAM_END) {
                ok = false;
                break;
            }
            const size_t produced = sizeof(outBuffer) - zs.avail_out;
            compressed.insert(compressed.end(), outBuffer, outBuffer + produced);
        }
    }
    deflateEnd(&zs);

    if (!ok) {
        error = "Failed to compress '" + name + "'";
        return false;
    }

    return addRaw(name, compressed.data(), compressed.size(), checksum,
                  uncompressedSize, kMethodDeflate, error);
}

bool ZipWriter::copyEntryFrom(zip_t* source, zip_uint64_t index, const std::string& name,
                              std::string& error) {
    if (!source) return false;

    zip_stat_t stat;
    zip_stat_init(&stat);
    if (zip_stat_index(source, index, 0, &stat) != 0) {
        return false; // not copyable; the caller regenerates it instead
    }
    const zip_uint64_t required = ZIP_STAT_SIZE | ZIP_STAT_COMP_SIZE |
                                 ZIP_STAT_CRC | ZIP_STAT_COMP_METHOD;
    if ((stat.valid & required) != required) {
        return false;
    }
    if (stat.size > kMaxZip32 || stat.comp_size > kMaxZip32) {
        error = "Entry '" + name + "' is too large for a ZIP32 package";
        return false;
    }

    zip_file_t* file = zip_fopen_index(source, index, ZIP_FL_COMPRESSED);
    if (!file) {
        return false;
    }

    std::vector<uint8_t> payload(static_cast<size_t>(stat.comp_size));
    zip_int64_t read = 0;
    if (stat.comp_size > 0) {
        read = zip_fread(file, payload.data(), stat.comp_size);
    }
    zip_fclose(file);
    if (read < 0 || static_cast<zip_uint64_t>(read) != stat.comp_size) {
        error = "Failed to read compressed data of '" + name + "'";
        return false;
    }

    return addRaw(name, payload.data(), payload.size(),
                  static_cast<uint32_t>(stat.crc), stat.size,
                  static_cast<uint16_t>(stat.comp_method), error);
}

bool ZipWriter::addRaw(const std::string& name, const uint8_t* payload, size_t payloadSize,
                       uint32_t crc, uint64_t uncompressedSize, uint16_t method,
                       std::string& error) {
    if (entries_.size() >= 0xFFFF) {
        error = "Too many entries for a ZIP32 package";
        return false;
    }
    if (payloadSize > kMaxZip32 || uncompressedSize > kMaxZip32) {
        error = "Entry '" + name + "' is too large for a ZIP32 package";
        return false;
    }

    Entry entry;
    entry.name = name;
    entry.crc = crc;
    entry.compressedSize = payloadSize;
    entry.uncompressedSize = uncompressedSize;
    entry.method = method;
    entry.headerOffset = out_.size();
    dosTimestamp(entry.dosTime, entry.dosDate);

    writeLocalHeader(entry);
    putBytes(payload, payloadSize);
    entries_.push_back(std::move(entry));
    return true;
}

void ZipWriter::writeLocalHeader(const Entry& entry) {
    putUint32(kLocalHeaderSignature);
    putUint16(kVersionNeeded);
    putUint16(kFlagUtf8Names);
    putUint16(entry.method);
    putUint16(entry.dosTime);
    putUint16(entry.dosDate);
    putUint32(entry.crc);
    putUint32(static_cast<uint32_t>(entry.compressedSize));
    putUint32(static_cast<uint32_t>(entry.uncompressedSize));
    putUint16(static_cast<uint16_t>(entry.name.size()));
    putUint16(0); // no extra field
    putBytes(entry.name.data(), entry.name.size());
}

void ZipWriter::writeCentralHeader(const Entry& entry) {
    putUint32(kCentralHeaderSignature);
    putUint16(kVersionNeeded); // version made by
    putUint16(kVersionNeeded);
    putUint16(kFlagUtf8Names);
    putUint16(entry.method);
    putUint16(entry.dosTime);
    putUint16(entry.dosDate);
    putUint32(entry.crc);
    putUint32(static_cast<uint32_t>(entry.compressedSize));
    putUint32(static_cast<uint32_t>(entry.uncompressedSize));
    putUint16(static_cast<uint16_t>(entry.name.size()));
    putUint16(0); // extra
    putUint16(0); // comment
    putUint16(0); // disk number
    putUint16(0); // internal attributes
    putUint32(0); // external attributes
    putUint32(static_cast<uint32_t>(entry.headerOffset));
    putBytes(entry.name.data(), entry.name.size());
}

bool ZipWriter::finish(std::string& error) {
    if (entries_.size() > 0xFFFF) {
        error = "Too many entries for a ZIP32 package";
        return false;
    }
    const uint64_t centralOffset = out_.size();
    for (const Entry& entry : entries_) {
        writeCentralHeader(entry);
    }
    const uint64_t centralSize = out_.size() - centralOffset;
    if (centralOffset > kMaxZip32 || centralSize > kMaxZip32) {
        error = "Package is too large for a ZIP32 archive";
        return false;
    }

    putUint32(kEndOfCentralDirectorySignature);
    putUint16(0); // this disk
    putUint16(0); // disk with central directory
    putUint16(static_cast<uint16_t>(entries_.size()));
    putUint16(static_cast<uint16_t>(entries_.size()));
    putUint32(static_cast<uint32_t>(centralSize));
    putUint32(static_cast<uint32_t>(centralOffset));
    putUint16(0); // no archive comment
    return true;
}

}} // namespace baja_xlsx::zipio
