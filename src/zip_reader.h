#ifndef ZIP_READER_H
#define ZIP_READER_H

#include <string>
#include <vector>
#include <zip.h>

namespace baja_xlsx { namespace zipio {

// Opens an archive read-only. Returns nullptr on failure and fills `error`.
zip_t* openReadOnly(const std::string& path, std::string& error);

// Reads one entry by exact name. Validates the declared entry size
// (AUDIT-20260917-006: no unbounded resize on attacker-controlled sizes)
// and enforces `maxBytes`. Returns false and fills `error` on failure;
// returns false without error when the entry does not exist.
bool readFile(zip_t* za, const std::string& filename,
              std::vector<uint8_t>& outData, size_t maxBytes,
              std::string& error);

// Maximum bytes for a single archive entry (256 MB default safety cap).
const size_t kMaxEntryBytes = 256u * 1024u * 1024u;

inline void close(zip_t* za) { if (za) zip_close(za); }

}} // namespace baja_xlsx::zipio

#endif // ZIP_READER_H
