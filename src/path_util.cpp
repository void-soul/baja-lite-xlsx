#include "path_util.h"

#if defined(_WIN32)
#include <filesystem>
#endif

namespace baja_xlsx { namespace pathutil {

#if defined(_WIN32)

namespace {

// UTF-8 -> UTF-16 for the wide Win32 APIs.
std::wstring toWide(const std::string& utf8Path) {
    std::error_code ec;
    const std::filesystem::path path = std::filesystem::u8path(utf8Path);
    return path.wstring();
}

} // namespace

std::FILE* openForRead(const std::string& utf8Path) {
    return _wfopen(toWide(utf8Path).c_str(), L"rb");
}

std::FILE* openForWrite(const std::string& utf8Path) {
    return _wfopen(toWide(utf8Path).c_str(), L"wb");
}

zip_t* openZipReadOnly(const std::string& utf8Path, std::string& error) {
    error.clear();
    zip_error_t ze;
    zip_error_init(&ze);
    zip_source_t* source = zip_source_win32w_create(toWide(utf8Path).c_str(), 0, -1, &ze);
    if (!source) {
        error = std::string("Failed to open XLSX file as ZIP: ") + zip_error_strerror(&ze);
        zip_error_fini(&ze);
        return nullptr;
    }
    zip_t* archive = zip_open_from_source(source, ZIP_RDONLY, &ze);
    if (!archive) {
        error = std::string("Failed to open XLSX file as ZIP: ") + zip_error_strerror(&ze);
        zip_source_free(source);
        zip_error_fini(&ze);
        return nullptr;
    }
    zip_error_fini(&ze);
    return archive;
}

zip_t* createZip(const std::string& utf8Path, std::string& error) {
    error.clear();
    zip_error_t ze;
    zip_error_init(&ze);
    const std::wstring wide = toWide(utf8Path);
    zip_source_t* source = zip_source_win32w_create(wide.c_str(), 0, -1, &ze);
    if (!source) {
        // The file does not exist yet; create an empty one and retry with a
        // freshly initialized error.
        zip_error_fini(&ze);
        std::FILE* file = openForWrite(utf8Path);
        if (!file) {
            error = "Failed to create '" + utf8Path + "'";
            return nullptr;
        }
        std::fclose(file);
        zip_error_init(&ze);
        source = zip_source_win32w_create(wide.c_str(), 0, -1, &ze);
    }
    if (!source) {
        error = std::string("Failed to create ZIP: ") + zip_error_strerror(&ze);
        zip_error_fini(&ze);
        return nullptr;
    }
    zip_t* archive = zip_open_from_source(source, ZIP_CREATE | ZIP_TRUNCATE, &ze);
    if (!archive) {
        error = std::string("Failed to create ZIP: ") + zip_error_strerror(&ze);
        zip_source_free(source);
        zip_error_fini(&ze);
        return nullptr;
    }
    zip_error_fini(&ze);
    return archive;
}

#else

std::FILE* openForRead(const std::string& utf8Path) {
    return std::fopen(utf8Path.c_str(), "rb");
}

std::FILE* openForWrite(const std::string& utf8Path) {
    return std::fopen(utf8Path.c_str(), "wb");
}

zip_t* openZipReadOnly(const std::string& utf8Path, std::string& error) {
    error.clear();
    int errorCode = 0;
    zip_t* archive = zip_open(utf8Path.c_str(), ZIP_RDONLY, &errorCode);
    if (!archive) {
        zip_error_t ze;
        zip_error_init_with_code(&ze, errorCode);
        error = std::string("Failed to open XLSX file as ZIP: ") + zip_error_strerror(&ze);
        zip_error_fini(&ze);
    }
    return archive;
}

zip_t* createZip(const std::string& utf8Path, std::string& error) {
    error.clear();
    std::FILE* file = openForWrite(utf8Path);
    if (!file) {
        error = "Failed to create '" + utf8Path + "'";
        return nullptr;
    }
    std::fclose(file);

    int errorCode = 0;
    zip_t* archive = zip_open(utf8Path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &errorCode);
    if (!archive) {
        zip_error_t ze;
        zip_error_init_with_code(&ze, errorCode);
        error = std::string("Failed to create ZIP: ") + zip_error_strerror(&ze);
        zip_error_fini(&ze);
    }
    return archive;
}

#endif

bool removeFile(const std::string& utf8Path) {
#if defined(_WIN32)
    std::error_code ec;
    return std::filesystem::remove(std::filesystem::u8path(utf8Path), ec);
#else
    return std::remove(utf8Path.c_str()) == 0;
#endif
}

}} // namespace baja_xlsx::pathutil
