#ifndef XLSX_PATCH_H
#define XLSX_PATCH_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "xlsx_writer.h"
#include "zip_writer.h"

namespace baja_xlsx {

// Where the workbook to modify comes from. Exactly one of the two is set.
struct TemplateSource {
    const std::string* path = nullptr;
    const std::vector<uint8_t>* bytes = nullptr;

    bool valid() const { return path != nullptr || bytes != nullptr; }
};

// One cell rewrite for updateCells().
struct CellUpdate {
    std::string sheet;        // empty -> first sheet
    std::string cell;         // "B7"
    WriteCell value;
    std::string numberFormat; // optional; appended to the template's styles
};

// Mode 1b: replace the target sheet's data. Only the sheet (and styles.xml, when
// new number formats are needed) is regenerated; every other part of the
// template is copied as compressed bytes.
bool replaceSheetData(const TemplateSource& source, const WritePlan& plan,
                      RowSource& table, zipio::ZipWriter::Compression compression,
                      std::vector<uint8_t>& out, std::string& error);

// Mode 1c: append rows to a worksheet, keeping its existing rows. When the
// sheet does not exist it is created (workbook entry, relationship, content
// type), which is how callers build multi-sheet workbooks by chaining appends.
// `headerMode`: "yes"/"no" are explicit; "auto" writes the header only when the
// sheet was created or had no rows.
bool appendRows(const TemplateSource& source, const WritePlan& plan, RowSource& table,
                const std::string& headerMode, zipio::ZipWriter::Compression compression,
                std::vector<uint8_t>& out, std::string& error);

// Mode 2: rewrite the listed cells only. The sheet is patched in place, the
// rest of the package is copied verbatim.
bool updateCells(const TemplateSource& source, const std::vector<CellUpdate>& updates,
                 zipio::ZipWriter::Compression compression,
                 std::vector<uint8_t>& out, std::string& error);

// ---------------------------------------------------------------------------
// Package plumbing shared by every write mode
// ---------------------------------------------------------------------------

namespace pkg {

// Entry names are normalized to forward slashes so packages written by WPS
// (backslashes) can be read and patched as well.
std::string normalizeName(const std::string& name);

// Reads one part; tolerates backslash entry names. Returns false and fills
// `error` when the part is missing.
bool readEntry(zip_t* archive, const std::string& wanted, std::string& out, std::string& error);

// Maps a sheet name (empty = first sheet) to its part inside the package,
// trying the relationship target as written plus a few safe alternatives.
bool findSheetPart(zip_t* archive, const std::string& sheetName, std::string& part,
                   std::string& error);

// Every worksheet part, in workbook order.
bool listWorksheetParts(zip_t* archive, std::vector<std::string>& parts, std::string& error);

// One entry to substitute while copying an archive: either in-memory content or
// a stream producer (which keeps a large part out of memory).
struct Replacement {
    std::string name;
    const std::string* content = nullptr;
    const std::function<bool(std::string& chunk)>* stream = nullptr;
};

// Copies every part of `archive`, substituting the listed ones. Untouched parts
// travel as compressed bytes: no recompression, no re-serialization.
bool assemblePackage(zip_t* archive, const std::vector<Replacement>& replacements,
                     zipio::ZipWriter::Compression compression, std::vector<uint8_t>& out,
                     std::string& error);

// Opens the workbook to modify. On success the caller owns `archive`.
bool openTemplate(const TemplateSource& source, zip_t*& archive, std::string& error);

class ArchiveCloser {
public:
    explicit ArchiveCloser(zip_t* archive) : archive_(archive) {}
    ~ArchiveCloser() {
        if (archive_) zip_close(archive_);
    }

private:
    zip_t* archive_;
};

} // namespace pkg

} // namespace baja_xlsx

#endif // XLSX_PATCH_H
