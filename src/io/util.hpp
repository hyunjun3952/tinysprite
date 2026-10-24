#ifndef TINYSPRITE_IO_UTIL_HPP
#define TINYSPRITE_IO_UTIL_HPP

#include <algorithm>
#include <cctype>
#include <string>

#include "tinysprite/sprite.hpp"

/* Small helpers the files of src/io share. */

/* Forgets the reason of the last failed file write on this thread. Exporters call it first, so a
 * refusal that never reaches the disk (bad extension, bad scale) does not report an older error. */
void file_write_error_clear();

/* `text` as a JSON string literal (quotes included). A byte that is not valid UTF-8 (a name read
 * from a damaged or foreign .pxa) becomes U+FFFD, so the file stays valid UTF-8. */
std::string json_quoted(const std::string &text);

/* True if `path` ends with `extension` (including the dot), ignoring case. */
inline bool path_has_extension(const std::string &path, const std::string &extension)
{
    return path.size() >= extension.size() &&
           std::equal(extension.rbegin(), extension.rend(), path.rbegin(), [](char a, char b) {
               return a == std::tolower(static_cast<unsigned char>(b));
           });
}

/* The last frame of an export that runs from `first` to `last` (negative: the sprite's last frame),
 * or -1 when the range does not fit the sprite. */
inline int frame_range_end(const Sprite &sprite, int first, int last)
{
    int end = last < 0 ? sprite.frameCount() - 1 : last;
    return first < 0 || end >= sprite.frameCount() || first > end ? -1 : end;
}

#endif
