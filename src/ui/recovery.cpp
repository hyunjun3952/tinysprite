#include "recovery.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>

#include "tinysprite/io.hpp"

namespace
{

namespace fs = std::filesystem;

constexpr int LEGACY_SLOT = -1; /* the single copy of earlier versions */

std::string stem(int slot)
{
    return slot == LEGACY_SLOT ? "recovery" : "recovery_" + std::to_string(slot);
}

std::string pxaFile(const std::string &dir, int slot)
{
    return path_to_utf8(path_from_utf8(dir) / (stem(slot) + ".pxa"));
}

std::string pathFile(const std::string &dir, int slot)
{
    return path_to_utf8(path_from_utf8(dir) / (stem(slot) + ".path"));
}

/* The slot a file name stands for ("recovery_12.pxa" is 12, "recovery.pxa" the old copy), or
 * nothing for any other name. */
bool slotOf(const std::string &name, int &slot)
{
    if (name == "recovery.pxa") {
        slot = LEGACY_SLOT;
        return true;
    }
    const std::string prefix = "recovery_";
    const std::string suffix = ".pxa";
    if (name.size() <= prefix.size() + suffix.size() ||
        name.compare(0, prefix.size(), prefix) != 0 ||
        name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return false;
    }
    const std::string digits =
        name.substr(prefix.size(), name.size() - prefix.size() - suffix.size());
    if (!std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; }) ||
        digits.size() > 9) {
        return false;
    }
    slot = std::atoi(digits.c_str());
    return true;
}

std::vector<int> slotsIn(const std::string &dir)
{
    std::vector<int> slots;
    std::error_code error;
    for (fs::directory_iterator it(path_from_utf8(dir), error), end; !error && it != end;
         it.increment(error)) {
        int slot = 0;
        if (slotOf(path_to_utf8(it->path().filename()), slot)) {
            slots.push_back(slot);
        }
    }
    std::sort(slots.begin(), slots.end());
    return slots;
}

} // namespace

bool recovery_save(const std::string &dir, int slot, const Sprite &sprite, const std::string &path)
{
    if (dir.empty()) {
        return false;
    }
    std::error_code error;
    fs::create_directories(path_from_utf8(dir), error);
    return !error && file_write_atomic(pathFile(dir, slot), path) &&
           pxa_save(sprite, pxaFile(dir, slot));
}

std::vector<Recovered> recovery_load(const std::string &dir)
{
    std::vector<Recovered> found;
    if (dir.empty()) {
        return found;
    }
    for (int slot : slotsIn(dir)) {
        std::optional<Sprite> sprite = pxa_load(pxaFile(dir, slot));
        if (!sprite) {
            continue;
        }
        std::string path;
        std::ifstream file(path_from_utf8(pathFile(dir, slot)), std::ios::binary);
        if (file) {
            path.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }
        found.push_back({std::move(*sprite), path, slot});
    }
    return found;
}

bool recovery_move_slot(const std::string &dir, int from, int to)
{
    if (dir.empty()) {
        return false;
    }
    std::error_code error;
    fs::rename(path_from_utf8(pxaFile(dir, from)), path_from_utf8(pxaFile(dir, to)), error);
    if (error) {
        return false;
    }
    fs::rename(path_from_utf8(pathFile(dir, from)), path_from_utf8(pathFile(dir, to)), error);
    return true; /* a copy without a .path file is an untitled one */
}

void recovery_clear_slot(const std::string &dir, int slot)
{
    if (dir.empty()) {
        return;
    }
    std::error_code ignored;
    fs::remove(path_from_utf8(pxaFile(dir, slot)), ignored);
    fs::remove(path_from_utf8(pathFile(dir, slot)), ignored);
}

void recovery_clear(const std::string &dir)
{
    if (dir.empty()) {
        return;
    }
    for (int slot : slotsIn(dir)) {
        recovery_clear_slot(dir, slot);
    }
}
