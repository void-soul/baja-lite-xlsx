#include "a1_reference.h"

#include <cctype>

namespace baja_xlsx {

bool parseColumnLetters(const std::string& text, size_t& out) {
    if (text.empty() || text.size() > 3) return false;
    size_t value = 0;
    for (char c : text) {
        const unsigned char uc = static_cast<unsigned char>(c);
        const char upper = static_cast<char>(std::toupper(uc));
        if (upper < 'A' || upper > 'Z') return false;
        value = value * 26 + static_cast<size_t>(upper - 'A' + 1);
    }
    out = value;
    return value > 0;
}

bool parseColumnReference(const std::string& text, size_t& first, size_t& last) {
    const size_t colon = text.find(':');
    if (colon == std::string::npos) {
        if (!parseColumnLetters(text, first)) return false;
        last = first;
        return true;
    }
    return parseColumnLetters(text.substr(0, colon), first) &&
           parseColumnLetters(text.substr(colon + 1), last) && first <= last;
}

bool parseA1Reference(const std::string& reference, size_t& column, size_t& row,
                      std::string& letters) {
    size_t i = 0;
    letters.clear();
    while (i < reference.size() && std::isalpha(static_cast<unsigned char>(reference[i]))) {
        letters.push_back(
            static_cast<char>(std::toupper(static_cast<unsigned char>(reference[i]))));
        ++i;
    }

    size_t value = 0;
    while (i < reference.size() && std::isdigit(static_cast<unsigned char>(reference[i]))) {
        value = value * 10 + static_cast<size_t>(reference[i] - '0');
        ++i;
    }
    if (letters.empty() || value == 0 || i != reference.size()) return false;

    size_t parsed = 0;
    if (!parseColumnLetters(letters, parsed)) return false;

    column = parsed;
    row = value;
    return true;
}

std::string columnLetters(size_t index) {
    std::string name;
    while (index > 0) {
        const size_t rem = (index - 1) % 26;
        name.insert(name.begin(), static_cast<char>('A' + static_cast<int>(rem)));
        index = (index - 1) / 26;
    }
    return name;
}

} // namespace baja_xlsx
