#include "file_paths.hpp"

#include <algorithm>
#include <cctype>

namespace
{

bool isSeparator(char c)
{
    return c == '/' || c == '\\';
}

size_t lastSeparator(const std::string &path)
{
    return path.find_last_of("/\\");
}

} // namespace

std::string with_default_extension(const std::string &path, const std::string &extension)
{
    if (path.empty()) {
        return path;
    }
    size_t name = lastSeparator(path);
    name = name == std::string::npos ? 0 : name + 1;
    if (path.find('.', name) != std::string::npos) {
        return path;
    }
    return path + extension;
}

std::string with_required_extension(const std::string &path, const std::string &extension)
{
    bool ends = path.size() >= extension.size() &&
                std::equal(extension.rbegin(), extension.rend(), path.rbegin(), [](char a, char b) {
                    return a == std::tolower(static_cast<unsigned char>(b));
                });
    return path.empty() || ends ? path : path + extension;
}

std::string with_replaced_extension(const std::string &path, const std::string &extension)
{
    if (path.empty()) {
        return path;
    }
    size_t name = lastSeparator(path);
    name = name == std::string::npos ? 0 : name + 1;
    size_t dot = path.find_last_of('.');
    return (dot == std::string::npos || dot < name ? path : path.substr(0, dot)) + extension;
}

size_t root_length(const std::string &path)
{
    if (path.size() >= 2 && isSeparator(path[0]) && isSeparator(path[1])) {
        /* UNC: the root is "\\server\share" plus its separator */
        size_t server = path.find_first_of("/\\", 2);
        if (server == std::string::npos) {
            return path.size();
        }
        size_t share = path.find_first_of("/\\", server + 1);
        return share == std::string::npos ? path.size() : share + 1;
    }
    if (path.size() >= 3 && std::isalpha(static_cast<unsigned char>(path[0])) != 0 &&
        path[1] == ':' && isSeparator(path[2])) {
        return 3;
    }
    return !path.empty() && isSeparator(path[0]) ? 1 : 0;
}

std::string directory_of(const std::string &path)
{
    size_t at = lastSeparator(path);
    if (at == std::string::npos) {
        return "";
    }
    size_t root = root_length(path);
    return at < root ? path.substr(0, root) : path.substr(0, at);
}

std::string file_name_of(const std::string &path)
{
    size_t at = lastSeparator(path);
    return at == std::string::npos ? path : path.substr(at + 1);
}

std::string join_path(const std::string &directory, const std::string &name)
{
    if (directory.empty()) {
        return name;
    }
    return isSeparator(directory.back()) ? directory + name : directory + "/" + name;
}
