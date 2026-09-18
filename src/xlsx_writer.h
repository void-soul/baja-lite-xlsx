#ifndef XLSX_WRITER_H
#define XLSX_WRITER_H

#include <cstdint>
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
    // Numbers that carry a time value get a date number format automatically
    // when their column does not define one, so Excel shows a date instead of
    // a raw serial.
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

// Builds a complete, minimal, valid .xlsx package into `out`.
bool writeNewWorkbook(const WritePlan& plan, RowSource& source,
                      zipio::ZipWriter::Compression compression,
                      std::vector<uint8_t>& out, std::string& error);

// Shared helpers (also used by the patching and template modes).
std::string escapeXmlText(const std::string& text);
std::string escapeXmlAttribute(const std::string& text);
std::string columnLetters(size_t index);
std::string formatNumber(double value);
double toExcelSerial(int year, int month, int day, int hour, int minute, int second,
                     int millisecond);
bool isValidSheetName(const std::string& name, std::string& reason);

} // namespace baja_xlsx

#endif // XLSX_WRITER_H
