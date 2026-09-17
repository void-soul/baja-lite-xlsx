#ifndef ZIP_READER_H
#define ZIP_READER_H

#include <string>
#include <vector>
#include <zip.h>

namespace baja_xlsx { namespace zipio {

// Opens an archive read-only. Returns nullptr on failure and fills `error`.
zip_t* openReadOnly(const std::string& path, std::string& error);

// True when the archive contains any drawing / media / WPS cell-image part.
// Only the central directory is scanned -- nothing is decompressed -- so this
// is cheap enough to gate the whole image pipeline on (P0-3).
bool packageHasMedia(const std::string& path);

// Reads one entry by exact name. Validates the declared entry size
// (AUDIT-20260917-006: no unbounded resize on attacker-controlled sizes)
// and enforces `maxBytes`. Returns false and fills `error` on failure;
// returns false without error when the entry does not exist.
bool readFile(zip_t* za, const std::string& filename,
              std::vector<uint8_t>& outData, size_t maxBytes,
              std::string& error);

// Maximum bytes for a single archive entry (256 MB default safety cap).
const size_t kMaxEntryBytes = 256u * 1024u * 1024u;

// Creates a temporary copy of `source` in which
// xl/_rels/workbook.xml.rels keeps only standard OOXML / Microsoft
// relationship types. Vendors (notably WPS) add proprietary types such as
// "http://www.wps.cn/officeDocument/2020/cellImage" which make xlnt abort
// with "key not found in container", so the workbook becomes unreadable
// even though the file itself is valid.
//
// On success `outTempPath` receives the copy path (ASCII temp location);
// callers own the file and should std::remove() it. Media/other parts are
// copied verbatim, so image extraction can still use the ORIGINAL file.
bool createSanitizedCopy(const std::string& source, std::string& outTempPath,
                         std::string& error);

inline void close(zip_t* za) { if (za) zip_close(za); }

}} // namespace baja_xlsx::zipio

#endif // ZIP_READER_H
