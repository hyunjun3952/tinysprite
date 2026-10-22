#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>

#include "frame_range.h"
#include "tinysprite/io.h"

namespace
{

/* `text` as a JSON string literal (quotes included). */
std::string quoted(const std::string &text)
{
    std::string out = "\"";
    for (char c : text) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(c));
                out += buf;
            } else {
                out += c;
            }
        }
    }
    return out + "\"";
}

const char *directionName(PlayMode mode)
{
    switch (mode) {
    case PlayMode::Forward:
        return "forward";
    case PlayMode::Reverse:
        return "reverse";
    case PlayMode::PingPong:
        break;
    }
    return "pingpong";
}

std::string rectJson(const Rect &r)
{
    return "{\"x\": " + std::to_string(r.x) + ", \"y\": " + std::to_string(r.y) +
           ", \"w\": " + std::to_string(r.w) + ", \"h\": " + std::to_string(r.h) + "}";
}

std::string framesJson(const Sprite &sprite, int first, const SpriteSheet &sheet)
{
    std::string out;
    for (size_t i = 0; i < sheet.frames.size(); i++) {
        out += i == 0 ? "\n" : ",\n";
        out += "    {\"frame\": " + rectJson(sheet.frames[i]) + ", \"duration\": " +
               std::to_string(sprite.frameDuration(first + static_cast<int>(i))) + "}";
    }
    return out;
}

std::string tagsJson(const Sprite &sprite, int first, int last)
{
    std::string out;
    for (const Sprite::Tag &tag : sprite.tags()) {
        int from = std::max(tag.from, first);
        int to = std::min(tag.to, last);
        if (from > to) {
            continue;
        }
        out += out.empty() ? "\n" : ",\n";
        out += "    {\"name\": " + quoted(tag.name) +
               ", \"from\": " + std::to_string(from - first) +
               ", \"to\": " + std::to_string(to - first) + R"(, "direction": ")" +
               directionName(tag.direction) + R"("})";
    }
    return out;
}

} // namespace

std::string spritesheet_metadata_json(const Sprite &sprite, const SheetOptions &options,
                                      const SpriteSheet &sheet, const std::string &image)
{
    int last = frame_range_end(sprite, options.first, options.last);
    std::string frames = framesJson(sprite, options.first, sheet);
    std::string tags = tagsJson(sprite, options.first, last);
    std::string out =
        "{\n  \"meta\": {\n    \"app\": \"tinysprite\",\n    \"image\": " + quoted(image) +
        ",\n    \"size\": {\"w\": " + std::to_string(sheet.width) +
        ", \"h\": " + std::to_string(sheet.height) +
        "},\n    \"frameSize\": {\"w\": " + std::to_string(sprite.width() * options.scale) +
        ", \"h\": " + std::to_string(sprite.height() * options.scale) +
        "},\n    \"scale\": " + std::to_string(options.scale) + "\n  },\n";
    out += "  \"frames\": [" + frames + (frames.empty() ? "" : "\n  ") + "],\n";
    out += "  \"tags\": [" + tags + (tags.empty() ? "" : "\n  ") + "]\n}\n";
    return out;
}

bool spritesheet_export_with_json(const Sprite &sprite, const std::string &path,
                                  const std::string &jsonPath, const SheetOptions &options)
{
    std::optional<SpriteSheet> sheet = spritesheet_render(sprite, options);
    if (!sheet || !image_write_rgba(path, sheet->width, sheet->height, sheet->pixels)) {
        return false;
    }
    return file_write_atomic(
        jsonPath, spritesheet_metadata_json(sprite, options, *sheet,
                                            std::filesystem::path(path).filename().string()));
}
