#ifndef A1_REFERENCE_H
#define A1_REFERENCE_H

#include <string>

namespace baja_xlsx {

// Column and A1 reference handling, shared by the writer, both readers, the
// patcher and the template renderer (they each had their own copy).

// "A" -> 1, "AB" -> 28, ... Returns false for anything that is not 1-3 letters.
bool parseColumnLetters(const std::string& text, size_t& out);

// Accepts "A", "AB" and "C:E" style references.
bool parseColumnReference(const std::string& text, size_t& first, size_t& last);

// Splits "AB12" into column 28 / row 12 / letters "AB".
bool parseA1Reference(const std::string& reference, size_t& column, size_t& row,
                      std::string& letters);

// 1 -> "A", 27 -> "AA".
std::string columnLetters(size_t index);

} // namespace baja_xlsx

#endif // A1_REFERENCE_H
