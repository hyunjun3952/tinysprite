#include "recovery.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>

#include "tinysprite/io.hpp"

namespace
{

namespace fs = std::filesystem;

std::string pxaFile(const std::string &dir)
{
    return (fs::path(dir) / "recovery.pxa").string();
}

std::string pathFile(const std::string &dir)
{
    return (fs::path(dir) / "recovery.path").string();
}

} // namespace

bool recovery_save(const std::string &dir, const Sprite &sprite, const std::string &path)
{
    if (dir.empty()) {
        return false;
    }
    std::error_code error;
    fs::create_directories(dir, error);
    return !error && file_write_atomic(pathFile(dir), path) && pxa_save(sprite, pxaFile(dir));
}

std::optional<Recovered> recovery_load(const std::string &dir)
{
    if (dir.empty()) {
        return std::nullopt;
    }
    std::optional<Sprite> sprite = pxa_load(pxaFile(dir));
    if (!sprite) {
        return std::nullopt;
    }
    std::string path;
    std::ifstream file(pathFile(dir), std::ios::binary);
    if (file) {
        path.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
    return Recovered{std::move(*sprite), path};
}

void recovery_clear(const std::string &dir)
{
    if (dir.empty()) {
        return;
    }
    std::error_code ignored;
    fs::remove(pxaFile(dir), ignored);
    fs::remove(pathFile(dir), ignored);
}
