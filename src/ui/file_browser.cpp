#include "file_browser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <sstream>

#include "file_paths.hpp"

namespace fs = std::filesystem;

namespace
{

std::string lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool extensionListed(const std::string &name, const std::string &extensions)
{
    if (extensions.empty()) {
        return true;
    }
    std::string ext = lower(fs::path(name).extension().string());
    std::istringstream listed(extensions);
    for (std::string item; listed >> item;) {
        if (item == ext) {
            return true;
        }
    }
    return false;
}

/* `path` without trailing separators (a lone "/" stays). */
std::string trimmed(std::string path)
{
    while (path.size() > 1 && (path.back() == '/' || path.back() == '\\')) {
        path.pop_back();
    }
    return path;
}

bool isDirectory(const std::string &path)
{
    std::error_code error;
    return !path.empty() && fs::is_directory(path, error);
}

} // namespace

std::vector<DirEntry> list_directory(const std::string &directory, const std::string &extensions)
{
    std::vector<DirEntry> folders;
    std::vector<DirEntry> files;
    std::error_code error;
    for (fs::directory_iterator it(directory.empty() ? "." : directory, error), end;
         !error && it != end; it.increment(error)) {
        std::string name = it->path().filename().string();
        if (name.empty() || name[0] == '.') {
            continue;
        }
        std::error_code kind;
        if (it->is_directory(kind)) {
            folders.push_back({name, true});
        } else if (!kind && extensionListed(name, extensions)) {
            files.push_back({name, false});
        }
    }
    auto byName = [](const DirEntry &a, const DirEntry &b) {
        return lower(a.name) < lower(b.name);
    };
    std::sort(folders.begin(), folders.end(), byName);
    std::sort(files.begin(), files.end(), byName);
    folders.insert(folders.end(), files.begin(), files.end());
    return folders;
}

std::string parent_directory(const std::string &directory)
{
    if (directory.empty()) {
        return "..";
    }
    return directory_of(trimmed(directory));
}

std::string browse_directory(const std::string &typed, const std::string &fallback)
{
    if (isDirectory(typed)) {
        return trimmed(typed);
    }
    std::string folder = directory_of(typed);
    if (isDirectory(folder)) {
        return folder;
    }
    return fallback.empty() ? "." : fallback;
}

std::string home_directory()
{
    const char *home = std::getenv("HOME");
    if (home == nullptr) {
        home = std::getenv("USERPROFILE"); /* Windows */
    }
    return home == nullptr ? "" : home;
}

std::vector<DirEntry> filter_entries(const std::vector<DirEntry> &entries, const std::string &query)
{
    const std::string needle = lower(query);
    std::vector<DirEntry> kept;
    std::copy_if(
        entries.begin(), entries.end(), std::back_inserter(kept),
        [&](const DirEntry &entry) { return lower(entry.name).find(needle) != std::string::npos; });
    return kept;
}

std::string expand_home(const std::string &path)
{
    const bool tilde =
        !path.empty() && path[0] == '~' && (path.size() == 1 || path[1] == '/' || path[1] == '\\');
    const std::string home = home_directory();
    return tilde && !home.empty() ? home + path.substr(1) : path;
}

namespace
{

struct Search {
    const std::string &extensions;
    std::string needle; /* lowercase */
    size_t maxResults;
    std::vector<DirEntry> found;
};

// NOLINTNEXTLINE(misc-no-recursion): a depth-first walk of folders, bounded by maxDepth
void searchIn(Search &search, const std::string &root, const std::string &prefix, int depth)
{
    for (const DirEntry &entry : list_directory(root, search.extensions)) {
        if (search.found.size() >= search.maxResults) {
            return;
        }
        const std::string relative = prefix + entry.name;
        if (entry.directory && depth > 1) {
            searchIn(search, root + "/" + entry.name, relative + "/", depth - 1);
        }
        if (lower(entry.name).find(search.needle) != std::string::npos &&
            search.found.size() < search.maxResults) {
            search.found.push_back({relative, entry.directory});
        }
    }
}

} // namespace

std::vector<DirEntry> search_directory(const std::string &root, const std::string &extensions,
                                       const std::string &query, int maxDepth, size_t maxResults)
{
    if (query.empty()) {
        return {};
    }
    Search search{extensions, lower(query), maxResults, {}};
    searchIn(search, root, "", maxDepth);
    return search.found;
}
