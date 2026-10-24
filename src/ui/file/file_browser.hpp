#ifndef TINYSPRITE_UI_FILE_BROWSER_HPP
#define TINYSPRITE_UI_FILE_BROWSER_HPP

#include <string>
#include <vector>

/* What the file dialog lists: the folders and files of one directory. Plain functions of the disk;
 * the dialog itself is in app/files.cpp. */

struct DirEntry {
    std::string name; /* the last component only */
    bool directory;
};

/* The folders of `directory` (an empty string means the current one), then its files whose
 * extension is in `extensions` (lowercase, separated by spaces, e.g. ".png .pxa"; empty lists every
 * file). Each group is sorted by name ignoring case, and names starting with "." are left out.
 * Returns nothing for a folder that cannot be read. */
std::vector<DirEntry> list_directory(const std::string &directory, const std::string &extensions);

/* The folder above `directory`: "/a/b" gives "/a", a root stays a root, a bare name gives "" (the
 * current folder) and "" gives "..". */
std::string parent_directory(const std::string &directory);

/* The folder the dialog shows for what is typed in its path field: the typed path when it is a
 * folder, else the folder it is in when that exists, else `fallback`, else "." . */
std::string browse_directory(const std::string &typed, const std::string &fallback);

/* The entries whose name contains `query`, ignoring case; an empty query keeps them all. */
std::vector<DirEntry> filter_entries(const std::vector<DirEntry> &entries,
                                     const std::string &query);

/* The folders and files below `root` (to `maxDepth` levels, 1 = only `root` itself) whose own name
 * contains `query`, ignoring case, folders being searched in list_directory's order; a file must
 * also have one of `extensions`. Names are the path relative to `root`, with "/" separators. Stops
 * after `maxResults`. An empty query finds nothing. */
std::vector<DirEntry> search_directory(const std::string &root, const std::string &extensions,
                                       const std::string &query, int maxDepth, size_t maxResults);

/* `path` with a leading "~" (alone or before a separator) replaced by the home folder. */
std::string expand_home(const std::string &path);

/* The user's home folder from the environment, or "". */
std::string home_directory();

#endif
