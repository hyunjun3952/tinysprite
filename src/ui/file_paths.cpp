#include "file_paths.h"

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

std::string directory_of(const std::string &path)
{
    size_t at = lastSeparator(path);
    if (at == std::string::npos) {
        return "";
    }
    return at == 0 ? path.substr(0, 1) : path.substr(0, at);
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
