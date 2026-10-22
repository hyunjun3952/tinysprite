#include "tinysprite/io.hpp"

std::filesystem::path path_from_utf8(const std::string &path)
{
    return std::filesystem::u8path(path);
}

std::string path_to_utf8(const std::filesystem::path &path)
{
    return path.u8string();
}
