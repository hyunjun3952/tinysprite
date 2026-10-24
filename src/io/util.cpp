#include "util.hpp"

#include <cstdio>

#include "tinysprite/io.hpp"

namespace
{

/* How long a sequence that starts with `lead` is: 1 for ASCII, 0 for a byte that starts none. */
size_t utf8Length(unsigned char lead)
{
    if (lead < 0xC2 || lead > 0xF4) {
        return lead < 0x80 ? 1 : 0;
    }
    if (lead >= 0xF0) {
        return 4;
    }
    return lead >= 0xE0 ? 3 : 2;
}

/* Whether the `length - 1` bytes after the lead byte at `at` are all continuation bytes. */
bool hasContinuations(const std::string &text, size_t at, size_t length)
{
    for (size_t i = 1; i < length; i++) {
        if ((static_cast<unsigned char>(text[at + i]) & 0xC0) != 0x80) {
            return false;
        }
    }
    return true;
}

/* The length of the well-formed UTF-8 sequence that starts at `at` (overlong forms, surrogates and
 * code points past U+10FFFF included as malformed), or 0 if there is none. */
size_t utf8Sequence(const std::string &text, size_t at)
{
    const auto byte = [&](size_t i) { return static_cast<unsigned char>(text[i]); };
    const unsigned char lead = byte(at);
    const size_t length = utf8Length(lead);
    if (length <= 1) {
        return length;
    }
    if (at + length > text.size()) {
        return 0; /* cut short */
    }
    if (!hasContinuations(text, at, length)) {
        return 0;
    }
    const unsigned char second = byte(at + 1);
    const bool overlong = (lead == 0xE0 && second < 0xA0) || (lead == 0xF0 && second < 0x90);
    const bool beyond = (lead == 0xF4 && second > 0x8F) || (lead == 0xED && second > 0x9F);
    return overlong || beyond ? 0 : length;
}

/* One character below U+0020, a quote or a backslash as its JSON escape; empty for the rest. */
std::string escapeAscii(char c)
{
    switch (c) {
    case '"':
        return "\\\"";
    case '\\':
        return "\\\\";
    case '\n':
        return "\\n";
    case '\t':
        return "\\t";
    default:
        break;
    }
    if (static_cast<unsigned char>(c) >= 0x20) {
        return "";
    }
    char buf[8];
    std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(c));
    return buf;
}

/* `text` as a JSON string literal (quotes included). A byte that is not valid UTF-8 (a name read
 * from a damaged or foreign .pxa) becomes U+FFFD, so the file stays valid UTF-8. */
std::string quotedText(const std::string &text)
{
    std::string out = "\"";
    for (size_t i = 0; i < text.size();) {
        const size_t length = utf8Sequence(text, i);
        if (length == 0) {
            out += "\\ufffd";
            i++;
            continue;
        }
        const std::string escaped = length == 1 ? escapeAscii(text[i]) : "";
        out += escaped.empty() ? text.substr(i, length) : escaped;
        i += length;
    }
    return out + "\"";
}

} // namespace

std::string json_quoted(const std::string &text)
{
    return quotedText(text);
}

std::filesystem::path path_from_utf8(const std::string &path)
{
    return std::filesystem::u8path(path);
}

std::string path_to_utf8(const std::filesystem::path &path)
{
    return path.u8string();
}
