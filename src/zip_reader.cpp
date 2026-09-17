#include "zip_reader.h"
#include "xml_parsers.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>

namespace baja_xlsx { namespace zipio {

namespace {

const char* kContentTypesPath = "[Content_Types].xml";

bool isStandardRelationshipType(const std::string& type) {
    if (type.empty()) return false;
    return type.rfind("http://schemas.openxmlformats.org/", 0) == 0 ||
           type.rfind("https://schemas.openxmlformats.org/", 0) == 0 ||
           type.rfind("http://schemas.microsoft.com/", 0) == 0 ||
           type.rfind("https://schemas.microsoft.com/", 0) == 0;
}

// Standard OOXML package part content types; vendor ones (WPS, etc.) make
// xlnt's manifest/content-type lookups fail with "key not found in container".
bool isStandardContentType(const std::string& type) {
    if (type.empty()) return true; // no ContentType attribute -> leave alone
    return type.rfind("application/vnd.openxmlformats-officedocument.", 0) == 0 ||
           type.rfind("application/vnd.openxmlformats-package.", 0) == 0 ||
           type.rfind("application/vnd.ms-excel.", 0) == 0 ||
           type.rfind("application/xml", 0) == 0 ||
           type.rfind("text/xml", 0) == 0;
}

// Drops elements whose Type/ContentType attribute is vendor-specific.
// `element` is the local tag name, `attribute` the attribute to inspect.
size_t stripNonStandardElements(const std::string& xml, const char* element,
                                const char* attribute, std::string& out,
                                bool (*isStandard)(const std::string&)) {
    out.clear();
    out.reserve(xml.size());
    size_t removed = 0;
    size_t pos = 0;
    const std::string closing = std::string("</") + element + ">";
    while (true) {
        const size_t elemPos = xmlp::findTagOpen(xml, element, pos);
        if (elemPos == std::string::npos) break;

        const size_t gt = xml.find('>', elemPos);
        if (gt == std::string::npos) break;
        const size_t selfClose = xml.find("/>", elemPos);
        size_t elemEnd = 0;
        if (selfClose != std::string::npos && selfClose <= gt) {
            elemEnd = selfClose + 2;
        } else {
            const size_t closeTag = xml.find(closing, gt);
            elemEnd = (closeTag == std::string::npos) ? gt + 1 : closeTag + closing.size();
        }

        std::string type;
        xmlp::getAttribute(xml, elemPos, attribute, type);
        if (isStandard(type)) {
            out.append(xml, pos, elemEnd - pos);
        } else {
            ++removed;
        }
        pos = elemEnd;
    }
    out.append(xml, pos, std::string::npos);
    return removed;
}

bool isRelationshipDoc(const std::string& name) {
    return name.size() > 5 && name.compare(name.size() - 5, 5, ".rels") == 0;
}

std::string makeTempWorkbookPath() {
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::temp_directory_path(ec);
    if (ec || dir.empty()) {
        dir = std::filesystem::current_path(ec);
    }
    std::random_device rd;
    const std::string name = "baja_xlsx_sanitized_" +
                             std::to_string(rd()) + "_" + std::to_string(rd()) + ".xlsx";
    return (dir / name).string();
}

} // namespace

bool createSanitizedCopy(const std::string& source, std::string& outTempPath,
                         std::string& error) {
    error.clear();
    outTempPath.clear();

    std::string openError;
    zip_t* in = openReadOnly(source, openError);
    if (!in) {
        error = openError;
        return false;
    }

    const std::string dest = makeTempWorkbookPath();
    int createError = 0;
    zip_t* out = zip_open(dest.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &createError);
    if (!out) {
        zip_error_t ze;
        zip_error_init_with_code(&ze, createError);
        error = std::string("Failed to create sanitized copy: ") + zip_error_strerror(&ze);
        zip_error_fini(&ze);
        close(in);
        return false;
    }

    bool ok = true;
    size_t stripped = 0;
    size_t renamed = 0;
    const zip_int64_t numEntries = zip_get_num_entries(in, 0);
    for (zip_int64_t i = 0; ok && i < numEntries; ++i) {
        const char* entryName = zip_get_name(in, i, 0);
        if (!entryName) continue;
        const std::string filename(entryName);   // raw name: used for reading
        std::string outName = filename;          // canonical name: written out
        if (outName.find('\\') != std::string::npos) {
            std::replace(outName.begin(), outName.end(), '\\', '/');
            ++renamed;
        }

        if (!outName.empty() && outName.back() == '/') {
            if (zip_dir_add(out, outName.c_str(), ZIP_FL_ENC_UTF_8) < 0) {
                // Directory entries are optional; ignore failures.
            }
            continue;
        }

        std::vector<uint8_t> data;
        std::string readError;
        if (!readFile(in, filename, data, kMaxEntryBytes, readError)) {
            // Probing misses are fine; real read failures abort the copy.
            if (!readError.empty()) {
                error = "Failed to copy entry '" + filename + "': " + readError;
                ok = false;
            }
            continue;
        }

        std::string payload(reinterpret_cast<const char*>(data.data()), data.size());
        if (isRelationshipDoc(outName)) {
            // Every .rels document: vendor-specific relationship types are
            // unknown to xlnt and abort the load.
            std::string filtered;
            stripped += stripNonStandardElements(payload, "Relationship", "Type",
                                                 filtered, isStandardRelationshipType);
            payload.swap(filtered);
        } else if (outName == kContentTypesPath) {
            // [Content_Types].xml overrides for vendor parts (e.g. WPS
            // cellimages) also break xlnt's content-type lookups.
            std::string filtered;
            stripped += stripNonStandardElements(payload, "Override", "ContentType",
                                                 filtered, isStandardContentType);
            payload.swap(filtered);
        }

        // libzip takes ownership (freep=1) and frees with free(), so hand it
        // a malloc'd copy rather than std::string storage.
        const size_t payloadSize = payload.size();
        void* payloadCopy = std::malloc(payloadSize > 0 ? payloadSize : 1);
        if (!payloadCopy) {
            error = "Out of memory staging entry '" + outName + "'";
            ok = false;
            break;
        }
        if (payloadSize > 0) {
            std::memcpy(payloadCopy, payload.data(), payloadSize);
        }

        zip_source_t* src = zip_source_buffer(out, payloadCopy, payloadSize, 1);
        if (!src) {
            std::free(payloadCopy);
            error = "Failed to stage entry '" + outName + "'";
            ok = false;
            break;
        }
        if (zip_file_add(out, outName.c_str(), src, ZIP_FL_ENC_UTF_8) < 0) {
            zip_source_free(src);
            error = "Failed to write entry '" + outName + "'";
            ok = false;
            break;
        }
    }

    if (zip_close(out) != 0) {
        error = "Failed to finalize sanitized copy";
        ok = false;
    }
    close(in);

    if (!ok || (stripped == 0 && renamed == 0)) {
        if (stripped == 0 && renamed == 0 && ok) {
            error = "No vendor-specific relationship types or entry names found";
        }
        std::remove(dest.c_str());
        return false;
    }

    outTempPath = dest;
    return true;
}

bool packageHasMedia(const std::string& path) {
    std::string error;
    zip_t* za = openReadOnly(path, error);
    if (!za) {
        // Cannot tell -> assume there is nothing to extract; sheet data is
        // still returned in full.
        return false;
    }

    bool found = false;
    const zip_int64_t numEntries = zip_get_num_entries(za, 0);
    for (zip_int64_t i = 0; i < numEntries && !found; ++i) {
        const char* entryName = zip_get_name(za, i, 0);
        if (!entryName) continue;
        const std::string entry(entryName);
        found = entry.compare(0, 9, "xl/media/") == 0 ||
                entry.compare(0, 12, "xl/drawings/") == 0 ||
                entry.compare(0, 14, "xl/cellimages/") == 0;
    }

    close(za);
    return found;
}

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
