#ifndef TINYSPRITE_UI_FILE_PATHS_HPP
#define TINYSPRITE_UI_FILE_PATHS_HPP

#include <cstddef>
#include <string>

/* Small string helpers for the path dialog; they only look at the text, never at the disk. Both
 * '/' and '\\' separate directories, since the Windows build accepts either. */

/* `path` with `extension` (".png") appended when its file name has none; "" stays "". */
std::string with_default_extension(const std::string &path, const std::string &extension);

/* `path` with `extension` appended unless it already ends with it (ignoring case); "" stays "". */
std::string with_required_extension(const std::string &path, const std::string &extension);

/* `path` with the extension of its file name (the last ".", if any) replaced by `extension`
 * (".json"); a name without one gets it added; "" stays "". */
std::string with_replaced_extension(const std::string &path, const std::string &extension);

/* How many leading characters of `path` are its root: "/" (1), "C:\\" (3) or "\\\\server\\share\\"
 * (the trailing separator counted when there is one); 0 for a relative path. */
size_t root_length(const std::string &path);

/* Everything before the last separator ("" for a bare name; the root itself, "/" or "C:\\", for
 * a file directly in it, and for the root). */
std::string directory_of(const std::string &path);

/* The last component. */
std::string file_name_of(const std::string &path);

/* `name` inside `directory`, with one separator between them; "" as the directory gives `name`. */
std::string join_path(const std::string &directory, const std::string &name);

#endif
