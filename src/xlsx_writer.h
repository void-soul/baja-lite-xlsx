#ifndef XLSX_WRITER_H
#define XLSX_WRITER_H

#include "a1_reference.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "zip_writer.h"

namespace baja_xlsx {

// One output cell. Numbers stay numbers so Excel sees real values and no
// precision is lost; text is escaped only when it is written.
struct WriteCell {
    enum class Kind : uint8_t { Empty, Number, Text, Boolean };

    Kind kind = Kind::Empty;
    double number = 0;
    bool boolean = false;
    std::string text;
    // A time value gets a date number format automatically when its column does
    // not define one, so Excel shows a date instead of a raw serial.
    bool isDate = false;
};

// Output shape of a column. Where a value comes from is the caller's business
// (the N-API layer maps JSON properties/indices), the writer only cares about
// the XML it has to produce.
struct WriteColumn {
    std::string header;        // header text (may be empty)
    std::string numberFormat;  // e.g. "#,##0.00", "yyyy-mm-dd"
    std::string align;         // "", "left", "center", "right"
    double width = 0;          // 0 = no explicit <col>
};

struct WritePlan {
    std::string sheetName = "Sheet1";
    std::vector<WriteColumn> columns;
    bool includeHeader = true;
    bool freezeHeader = false;
};

// Pull-based row source: one implementation reads straight out of the JS array
// (no bulk copy), so the generator stays independent of N-API.
class RowSource {
public:
    virtual ~RowSource() = default;
    virtual size_t rowCount() const = 0;
    // Fills `row` with exactly one cell per column; false when exhausted.
    virtual bool nextRow(std::vector<WriteCell>& row) = 0;
};

// The styles a sheet write needs. `firstNumFmtId` / `xfBaseIndex` let the same
// logic serve a brand new styles.xml (ids from 164, cellXfs from index 1) and a
// template whose styles.xml is appended to.
struct SheetStyles {
    size_t firstNumFmtId = 164;
    size_t xfBaseIndex = 1;

    std::vector<std::string> numFmtCodes; // custom formats, ids firstNumFmtId + i
    std::vector<std::string> xfBodies;    // extra <xf> bodies, index xfBaseIndex + i
    std::vector<size_t> styleIndexByColumn;
    std::vector<bool> explicitFormat;
    // Date formats are resolved from values while rows stream, so they exist up
    // front: a cell that needs one only picks an index that is already there.
    size_t dateStyleIndex = 0;     // "yyyy-mm-dd"
    size_t dateTimeStyleIndex = 0; // "yyyy-mm-dd hh:mm:ss"

    std::map<std::string, size_t> formatIds;      // format code -> numFmtId
    std::map<std::string, size_t> formatStyleIds; // format code -> cellXfs index
    std::map<std::string, size_t> xfIds;          // xf body -> cellXfs index

    // Returns the cellXfs index for a number format (and optional alignment),
    // adding the entries when they are new.
    size_t styleForFormat(const std::string& formatCode,
                          const std::string& align = std::string());
};

// Per-column styles for a plan; columns without a format or alignment keep 0.
// `firstNumFmtId` / `xfBaseIndex` let a template's styles.xml be appended to.
SheetStyles buildSheetStyles(const WritePlan& plan, size_t firstNumFmtId = 164,
                             size_t xfBaseIndex = 1);

// Streams the rows of a sheetData element, one piece per call, so a huge sheet
// never exists in memory as one string. Shared by the new-workbook and the
// template-replacement paths.
class SheetRowStream {
public:
    // `startRow` is the 1-based row number the first emitted row gets; appends
    // pass the row after the sheet's current last row.
    SheetRowStream(const WritePlan& plan, RowSource& source, SheetStyles& styles,
                   size_t startRow = 1);

    // Appends the next piece to `chunk`; false once every row was produced.
    bool next(std::string& chunk);

    size_t totalRows() const { return totalRows_; }

private:
    const WritePlan& plan_;
    RowSource& source_;
    SheetStyles& styles_;
    std::vector<std::string> letters_;
    std::vector<WriteCell> row_;
    size_t startRow_ = 1;
    size_t emitted_ = 0;
    size_t totalRows_ = 0;
};

// Builds a complete, minimal, valid .xlsx package into `out`.
bool writeNewWorkbook(const WritePlan& plan, RowSource& source,
                      zipio::ZipWriter::Compression compression,
                      std::vector<uint8_t>& out, std::string& error);

// Serializes a styles.xml for the given styles (new-workbook case).
std::string buildStylesXml(const SheetStyles& styles);

// Renders `<sheetData>` ... `</sheetData>`? Only the outer element is added by
// the caller; the stream emits <row> elements.
// columnLetters / parseA1Reference live in a1_reference.h.

// Shared helpers (also used by the patching mode).
std::string escapeXmlText(const std::string& text);
std::string escapeXmlAttribute(const std::string& text);
std::string formatNumber(double value);
double toExcelSerial(int year, int month, int day, int hour, int minute, int second,
                     int millisecond);
bool isValidSheetName(const std::string& name, std::string& reason);

// Serializes one cell element (`<c ...>...</c>`); shared with the patcher.
void appendCellXml(std::string& out, const std::string& columnLetters, size_t row,
                   const WriteCell& cell, size_t styleIndex);

} // namespace baja_xlsx

#endif // XLSX_WRITER_H
