#ifndef TINYSPRITE_INFLATE_HPP
#define TINYSPRITE_INFLATE_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

/* Decompresses a zlib stream (RFC 1950 around RFC 1951 deflate: stored, fixed and dynamic Huffman
 * blocks), as used by Aseprite files. Returns nothing for data that is not a valid stream (bad
 * header, a preset dictionary, damaged or truncated blocks) or whose output would exceed
 * `maxBytes`, so a small file cannot make the program allocate without bound. The Adler-32
 * checksum at the end is not verified. */
std::optional<std::vector<uint8_t>> zlib_inflate(const uint8_t *data, size_t size, size_t maxBytes);

#endif
