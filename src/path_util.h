#ifndef PATH_UTIL_H
#define PATH_UTIL_H

#include <cstdio>
#include <string>
#include <zip.h>

namespace baja_xlsx { namespace pathutil {

// Paths arrive from JavaScript as UTF-8. Narrow file APIs on Windows interpret
// those bytes as ANSI (GBK on a Chinese system), so any non-ASCII file or
// directory name fails to open -- silently for reads, and as a bogus
// "cannot create file" for writes. Everything that touches the filesystem goes
// through these helpers, which use the wide API on Windows.
std::FILE* openForRead(const std::string& utf8Path);
std::FILE* openForWrite(const std::string& utf8Path);

// Opens a ZIP archive read-only, using the wide source on Windows.
zip_t* openZipReadOnly(const std::string& utf8Path, std::string& error);

// Creates (truncating) a ZIP archive at `utf8Path`.
zip_t* createZip(const std::string& utf8Path, std::string& error);

// Deletes a file, tolerating a missing one.
bool removeFile(const std::string& utf8Path);

}} // namespace baja_xlsx::pathutil

#endif // PATH_UTIL_H
