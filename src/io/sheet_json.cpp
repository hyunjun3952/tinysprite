#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>

#include "frame_range.hpp"
#include "tinysprite/io.hpp"

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

/* What a packed frame adds: that it was trimmed, which part of the whole frame it is, and how big
 * the whole frame is (both as drawn, so the scale is in them). */
std::string trimJson(const Sprite &sprite, const SpriteSheet &sheet, size_t i, int scale)
{
    return R"(, "trimmed": true, "spriteSourceSize": )" + rectJson(sheet.trims[i]) +
           R"(, "sourceSize": {"w": )" + std::to_string(sprite.width() * scale) +
           ", \"h\": " + std::to_string(sprite.height() * scale) + "}";
}

std::string framesJson(const Sprite &sprite, int first, const SpriteSheet &sheet, int scale)
{
    std::string out;
    for (size_t i = 0; i < sheet.frames.size(); i++) {
        out += i == 0 ? "\n" : ",\n";
        out += "    {\"frame\": " + rectJson(sheet.frames[i]) + ", \"duration\": " +
               std::to_string(sprite.frameDuration(first + static_cast<int>(i)));
        if (sheet.trimmed && i < sheet.trims.size()) {
            out += trimJson(sprite, sheet, i, scale);
        }
        out += "}";
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

/* The slices in the exported frames' pixel space: every frame has the same size, so each slice is
 * given once, scaled like the frames. */
std::string slicesJson(const Sprite &sprite, int scale)
{
    std::string out;
    for (const Sprite::Slice &slice : sprite.slices()) {
        Rect r = {slice.rect.x * scale, slice.rect.y * scale, slice.rect.w * scale,
                  slice.rect.h * scale};
        out += out.empty() ? "\n" : ",\n";
        out += "    {\"name\": " + quoted(slice.name) + ", \"bounds\": " + rectJson(r);
        if (slice.hasPivot) {
            out += R"(, "pivot": {"x": )" + std::to_string(slice.pivotX * scale) + R"(, "y": )" +
                   std::to_string(slice.pivotY * scale) + "}";
        }
        out += "}";
    }
    return out;
}

} // namespace

std::string spritesheet_metadata_json(const Sprite &sprite, const SheetOptions &options,
                                      const SpriteSheet &sheet, const std::string &image)
{
    int last = frame_range_end(sprite, options.first, options.last);
    std::string frames = framesJson(sprite, options.first, sheet, options.scale);
    std::string tags = tagsJson(sprite, options.first, last);
    std::string out =
        "{\n  \"meta\": {\n    \"app\": \"tinysprite\",\n    \"image\": " + quoted(image) +
        ",\n    \"size\": {\"w\": " + std::to_string(sheet.width) +
        ", \"h\": " + std::to_string(sheet.height) +
        "},\n    \"frameSize\": {\"w\": " + std::to_string(sprite.width() * options.scale) +
        ", \"h\": " + std::to_string(sprite.height() * options.scale) +
        "},\n    \"scale\": " + std::to_string(options.scale) + "\n  },\n";
    out += "  \"frames\": [" + frames + (frames.empty() ? "" : "\n  ") + "],\n";
    out += "  \"tags\": [" + tags + (tags.empty() ? "" : "\n  ") + "],\n";
    std::string slices = slicesJson(sprite, options.scale);
    out += "  \"slices\": [" + slices + (slices.empty() ? "" : "\n  ") + "]\n}\n";
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
