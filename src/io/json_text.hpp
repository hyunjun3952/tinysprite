#ifndef TINYSPRITE_IO_JSON_TEXT_HPP
#define TINYSPRITE_IO_JSON_TEXT_HPP

#include <string>

/* `text` as a JSON string literal (quotes included). A byte that is not valid UTF-8 (a name read
 * from a damaged or foreign .pxa) becomes U+FFFD, so the file stays valid UTF-8. */
std::string json_quoted(const std::string &text);

#endif
