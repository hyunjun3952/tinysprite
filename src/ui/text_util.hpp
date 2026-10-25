#ifndef TINYSPRITE_UI_TEXT_UTIL_HPP
#define TINYSPRITE_UI_TEXT_UTIL_HPP

#include <string>

/* Small text helpers the files of src/ui share. */

/* The text without the white space at both ends. */
inline std::string trim(const std::string &text)
{
    const char *space = " \t\r\n";
    size_t first = text.find_first_not_of(space);
    if (first == std::string::npos) {
        return "";
    }
    return text.substr(first, text.find_last_not_of(space) - first + 1);
}

#endif
