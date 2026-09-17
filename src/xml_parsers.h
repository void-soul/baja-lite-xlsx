#ifndef XML_PARSERS_H
#define XML_PARSERS_H

#include <string>
#include <map>

namespace baja_xlsx { namespace xmlp {

// Robust scanners for the fixed OOXML schemas used by this library.
// Unlike the previous offset-magic string search (AUDIT-20260917-004),
// these helpers tolerate attribute order, single/double quotes and
// whitespace around '='. They are NOT general-purpose XML parsers.

// Find the next "<tag" opening whose following char is a boundary
// (space, '>', '/', tab, CR, LF) so "<drawing" does not match "<drawing1".
size_t findTagOpen(const std::string& xml, const std::string& tag, size_t from);

// Read an attribute value bounded to the opening tag starting at
// `elementStart` (up to the next '>').
bool getAttribute(const std::string& xml, size_t elementStart,
                  const std::string& attr, std::string& out);

// Read an attribute value bounded to [start, end) -- for searching
// attributes anywhere inside a container element (e.g. r:embed in an
// anchor block).
bool getAttributeInRange(const std::string& xml, size_t start, size_t end,
                         const std::string& attr, std::string& out);

// Read inner text of <tag>...</tag>, searching from `from`.
bool getElementText(const std::string& xml, const std::string& tag,
                    size_t from, std::string& out);

// Parse "Id" -> filename part of "Target" pairs from a .rels document.
std::map<std::string, std::string> parseRelationships(const std::string& xmlContent);

// Parse a non-negative integer (trims whitespace, rejects overflow).
bool parseNonNegativeInt(const std::string& s, int& out);

}} // namespace baja_xlsx::xmlp

#endif // XML_PARSERS_H
