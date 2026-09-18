#ifndef CELL_FORMAT_H
#define CELL_FORMAT_H

#include <string>

namespace baja_xlsx {

// Cell value formatting shared by the xlnt reader and the direct XML reader, so
// both produce the same text for the same workbook (P3).

// Shortest round-trip representation of a double, with a fast path for integral
// values. Deliberately not std::to_chars: libc++ only offers the floating point
// overload from macOS 13.3 on, and the deployment target is 10.15.
std::string formatDouble(double value);

// Excel date serial (days since 1899-12-30) as "YYYY-MM-DD[ HH:MM:SS]".
// Deliberately locale independent, unlike xlnt's cell.to_string().
std::string formatDateSerial(double serial);

// "HH:MM:SS" for the time-of-day part of an Excel serial.
std::string formatTimeOfDay(double serial);

} // namespace baja_xlsx

#endif // CELL_FORMAT_H
