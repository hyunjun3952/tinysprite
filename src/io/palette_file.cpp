#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#include "path_util.h"
#include "tinysprite/io.h"

bool file_write_atomic(const std::string &path, const std::string &data)
{
    /* Write beside the target and rename, so a failed write never destroys an existing file. */
    std::string temp = path + ".tmp";
    {
        std::ofstream file(temp, std::ios::binary);
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
        file.flush();
        file.close();
        if (file.fail()) {
            std::error_code ignored;
            std::filesystem::remove(temp, ignored);
            return false;
        }
    }
    std::error_code error;
    std::filesystem::rename(temp, path, error);
    if (error) {
        std::filesystem::remove(temp, error);
        return false;
    }
    return true;
}

namespace
{

std::string trim(const std::string &text)
{
    size_t first = 0;
    size_t last = text.size();
    while (first < last && std::isspace(static_cast<unsigned char>(text[first]))) {
        first++;
    }
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) {
        last--;
    }
    return text.substr(first, last - first);
}

std::vector<std::string> lines(const std::string &text)
{
    std::vector<std::string> out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        out.push_back(trim(line));
    }
    return out;
}

bool parseHexColor(const std::string &line, Rgba &out)
{
    std::string digits = !line.empty() && line[0] == '#' ? line.substr(1) : line;
    if ((digits.size() != 6 && digits.size() != 8) ||
        !std::all_of(digits.begin(), digits.end(),
                     [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; })) {
        return false;
    }
    auto byte = [&](size_t at) {
        return static_cast<uint8_t>(std::strtoul(digits.substr(at, 2).c_str(), nullptr, 16));
    };
    out = Rgba{byte(0), byte(2), byte(4), digits.size() == 8 ? byte(6) : uint8_t{255}};
    return true;
}

/* "R G B optional name": three numbers 0..255, then anything. */
bool parseGplColor(const std::string &line, Rgba &out)
{
    std::istringstream in(line);
    int r;
    int g;
    int b;
    if (!(in >> r >> g >> b) || std::min({r, g, b}) < 0 || std::max({r, g, b}) > 255) {
        return false;
    }
    out = Rgba{static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), 255};
    return true;
}

} // namespace

std::string palette_encode_hex(const Palette &palette)
{
    std::string out;
    for (int i = 0; i < palette.count(); i++) {
        Rgba c = palette.get(i);
        char line[16];
        if (c.a == 255) {
            std::snprintf(line, sizeof(line), "%02x%02x%02x\n", c.r, c.g, c.b);
        } else {
            std::snprintf(line, sizeof(line), "%02x%02x%02x%02x\n", c.r, c.g, c.b, c.a);
        }
        out += line;
    }
    return out;
}

std::string palette_encode_gpl(const Palette &palette, const std::string &name)
{
    std::string out = "GIMP Palette\nName: " + name + "\nColumns: 8\n#\n";
    for (int i = 0; i < palette.count(); i++) {
        Rgba c = palette.get(i);
        char line[48];
        std::snprintf(line, sizeof(line), "%3d %3d %3d\tColor %d\n", c.r, c.g, c.b, i + 1);
        out += line;
    }
    return out;
}

std::optional<Palette> palette_decode(const std::string &text)
{
    std::vector<std::string> all = lines(text);
    size_t first = 0;
    while (first < all.size() && all[first].empty()) {
        first++;
    }
    if (first == all.size()) {
        return std::nullopt;
    }
    bool gpl = all[first] == "GIMP Palette";
    Palette palette;
    for (size_t i = gpl ? first + 1 : first; i < all.size(); i++) {
        const std::string &line = all[i];
        if (line.empty()) {
            continue;
        }
        if (gpl &&
            (line[0] == '#' || line.rfind("Name:", 0) == 0 || line.rfind("Columns:", 0) == 0)) {
            continue;
        }
        Rgba color;
        if (!(gpl ? parseGplColor(line, color) : parseHexColor(line, color))) {
            return std::nullopt;
        }
        palette.add(color); /* a full palette ignores the rest */
    }
    if (palette.count() == 0) {
        return std::nullopt;
    }
    return palette;
}

namespace
{

/* The palette formats: the extension that selects each and how it is written. Loading looks at the
 * content, so it only needs to know the extension is one of these. */
struct PaletteFormat {
    const char *extension;
    std::string (*encode)(const Palette &palette, const std::string &name);
};

std::string encodeHex(const Palette &palette, const std::string &)
{
    return palette_encode_hex(palette);
}

std::string encodeGpl(const Palette &palette, const std::string &name)
{
    return palette_encode_gpl(palette, name.empty() ? "palette" : name);
}

const PaletteFormat PALETTE_FORMATS[] = {{".gpl", encodeGpl}, {".hex", encodeHex}};

const PaletteFormat *paletteFormat(const std::string &path)
{
    for (const PaletteFormat &format : PALETTE_FORMATS) {
        if (path_has_extension(path, format.extension)) {
            return &format;
        }
    }
    return nullptr;
}

} // namespace

bool palette_save(const Palette &palette, const std::string &path)
{
    const PaletteFormat *format = paletteFormat(path);
    if (!format) {
        return false;
    }
    return file_write_atomic(path,
                             format->encode(palette, std::filesystem::path(path).stem().string()));
}

std::optional<Palette> palette_load(const std::string &path)
{
    if (!paletteFormat(path)) {
        return std::nullopt;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    return palette_decode(
        std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()));
}

std::string palette_formats()
{
    return std::string(PALETTE_FORMATS[0].extension) + " or " + PALETTE_FORMATS[1].extension;
}
