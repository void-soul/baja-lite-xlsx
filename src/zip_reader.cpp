#include "zip_reader.h"
#include <cstring>

namespace baja_xlsx { namespace zipio {

zip_t* openReadOnly(const std::string& path, std::string& error) {
    error.clear();
    int errorp = 0;
    zip_t* za = zip_open(path.c_str(), ZIP_RDONLY, &errorp);
    if (!za) {
        zip_error_t ziperr;
        zip_error_init_with_code(&ziperr, errorp);
        error = std::string("Failed to open XLSX file as ZIP: ") + zip_error_strerror(&ziperr);
        zip_error_fini(&ziperr);
        return nullptr;
    }
    return za;
}

bool readFile(zip_t* za, const std::string& filename,
              std::vector<uint8_t>& outData, size_t maxBytes,
              std::string& error) {
    error.clear();
    outData.clear();

    zip_int64_t index = zip_name_locate(za, filename.c_str(), 0);
    if (index < 0) {
        return false; // entry not present (not an error for callers that probe)
    }

    struct zip_stat sb;
    zip_stat_init(&sb);
    if (zip_stat_index(za, index, 0, &sb) != 0) {
        error = "Failed to stat zip entry: " + filename;
        return false;
    }
    if (!(sb.valid & ZIP_STAT_SIZE)) {
        error = "Zip entry has no declared size: " + filename;
        return false;
    }
    if (sb.size > static_cast<zip_uint64_t>(kMaxEntryBytes) ||
        sb.size > static_cast<zip_uint64_t>(maxBytes)) {
        error = "Zip entry exceeds size limit: " + filename;
        return false;
    }

    zip_file_t* zf = zip_fopen_index(za, index, 0);
    if (!zf) {
        error = "Failed to open zip entry: " + filename;
        return false;
    }

    outData.resize(static_cast<size_t>(sb.size));
    zip_int64_t bytesRead = sb.size > 0 ? zip_fread(zf, outData.data(), sb.size) : 0;
    zip_fclose(zf);

    if (bytesRead != static_cast<zip_int64_t>(sb.size)) {
        error = "Zip entry truncated while reading: " + filename;
        outData.clear();
        return false;
    }
    return true;
}

}} // namespace baja_xlsx::zipio
