#ifndef NUMBER_FORMAT_H
#define NUMBER_FORMAT_H

#include <cctype>
#include <string>

namespace baja_xlsx {

// What a number format code says about the value it renders.
struct FormatTokens {
    bool date = false; // y or d: the value is a date
    bool time = false; // h or s: the value carries a time of day
};

// Scans a format code outside of quoted literals. Shared by the xlnt reader and
// the direct XML reader, so both classify dates and times identically (P3).
inline FormatTokens scanFormatTokens(const std::string& code) {
    FormatTokens tokens;
    bool quoted = false;
    for (size_t i = 0; i < code.size(); ++i) {
        const char c = code[i];
        if (c == '"') {
            quoted = !quoted;
            continue;
        }
        if (c == '\\' || c == '_' || c == '*') { // escaped literal character
            ++i;
            continue;
        }
        if (quoted) continue;

        const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower == 'y' || lower == 'd') tokens.date = true;
        if (lower == 'h' || lower == 's') tokens.time = true;
    }
    return tokens;
}

} // namespace baja_xlsx

#endif // NUMBER_FORMAT_H
