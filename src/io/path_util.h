#ifndef TINYSPRITE_IO_PATH_UTIL_H
#define TINYSPRITE_IO_PATH_UTIL_H

#include <algorithm>
#include <cctype>
#include <string>

/* True if `path` ends with `extension` (including the dot), ignoring case. */
inline bool path_has_extension(const std::string &path, const std::string &extension)
{
    return path.size() >= extension.size() &&
           std::equal(extension.rbegin(), extension.rend(), path.rbegin(), [](char a, char b) {
               return a == std::tolower(static_cast<unsigned char>(b));
           });
}

#endif
