#include "tinysprite/inflate.hpp"

#include <array>

namespace
{

constexpr int MAX_BITS = 15;
constexpr int LITERAL_CODES = 288;
constexpr int DISTANCE_CODES = 30;
constexpr int END_OF_BLOCK = 256;

constexpr std::array<uint16_t, 29> LENGTH_BASE = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                                  15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                                  67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<uint8_t, 29> LENGTH_EXTRA = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                  2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<uint16_t, 30> DISTANCE_BASE = {
    1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<uint8_t, 30> DISTANCE_EXTRA = {0, 0, 0,  0,  1,  1,  2,  2,  3,  3,
                                                    4, 4, 5,  5,  6,  6,  7,  7,  8,  8,
                                                    9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
/* The order the code lengths of a dynamic block's code-length code are stored in. */
constexpr std::array<uint8_t, 19> CODE_LENGTH_ORDER = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                                       11, 4,  12, 3, 13, 2, 14, 1, 15};

/* A canonical Huffman code: how many codes there are of each length and the symbols in code
 * order. */
struct Huffman {
    std::array<uint16_t, MAX_BITS + 1> count{};
    std::array<uint16_t, LITERAL_CODES> symbol{};
};

/* Reads the stream bit by bit, least significant first; running dry sets `failed`. */
struct Input {
    const uint8_t *data;
    size_t size;
    size_t pos = 0;
    uint32_t bitBuffer = 0;
    int bitCount = 0;
    bool failed = false;

    uint32_t bits(int need)
    {
        while (bitCount < need) {
            if (pos >= size) {
                failed = true;
                return 0;
            }
            bitBuffer |= static_cast<uint32_t>(data[pos++]) << bitCount;
            bitCount += 8;
        }
        uint32_t value = bitBuffer & ((1U << need) - 1U);
        bitBuffer >>= need;
        bitCount -= need;
        return value;
    }
};

struct Output {
    std::vector<uint8_t> bytes;
    size_t limit;
};

/* Fills `h` from the code length of every symbol; false if the lengths over-subscribe the code.
 * An incomplete code is allowed (a block may use a single distance code). */
bool buildHuffman(Huffman &h, const uint8_t *lengths, int n)
{
    h = Huffman{};
    for (int i = 0; i < n; i++) {
        h.count[lengths[i]]++;
    }
    int left = 1;
    for (int len = 1; len <= MAX_BITS; len++) {
        left = (left << 1) - h.count[len];
        if (left < 0) {
            return false;
        }
    }
    std::array<uint16_t, MAX_BITS + 2> offset{};
    for (int len = 1; len <= MAX_BITS; len++) {
        offset[len + 1] = static_cast<uint16_t>(offset[len] + h.count[len]);
    }
    for (int i = 0; i < n; i++) {
        if (lengths[i] != 0) {
            h.symbol[offset[lengths[i]]++] = static_cast<uint16_t>(i);
        }
    }
    return true;
}

/* The next symbol of `h`, or -1 when the input ends or the bits match no code. */
int decodeSymbol(Input &in, const Huffman &h)
{
    int code = 0;
    int first = 0;
    int index = 0;
    for (int len = 1; len <= MAX_BITS; len++) {
        code |= static_cast<int>(in.bits(1));
        int count = h.count[len];
        if (code - count < first) {
            return in.failed ? -1 : h.symbol[index + (code - first)];
        }
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    return -1;
}

bool storedBlock(Input &in, Output &out)
{
    in.bitBuffer = 0; /* the block continues at the next byte */
    in.bitCount = 0;
    if (in.pos + 4 > in.size) {
        return false;
    }
    size_t len = in.data[in.pos] | (in.data[in.pos + 1] << 8);
    size_t inverse = in.data[in.pos + 2] | (in.data[in.pos + 3] << 8);
    in.pos += 4;
    if (len != (~inverse & 0xFFFF) || in.pos + len > in.size ||
        out.bytes.size() + len > out.limit) {
        return false;
    }
    out.bytes.insert(out.bytes.end(), in.data + in.pos, in.data + in.pos + len);
    in.pos += len;
    return true;
}

/* Copies a match of `length` bytes from `distance` back; false if there is not that much output
 * yet or it would pass the limit. */
bool copyMatch(Output &out, size_t length, size_t distance)
{
    if (distance > out.bytes.size() || out.bytes.size() + length > out.limit) {
        return false;
    }
    size_t from = out.bytes.size() - distance;
    for (size_t i = 0; i < length; i++) {
        out.bytes.push_back(out.bytes[from + i]);
    }
    return true;
}

/* The length and distance that follow length symbol `symbol` (257..285). */
bool matchAfter(Input &in, Output &out, int symbol, const Huffman &distances)
{
    symbol -= END_OF_BLOCK + 1;
    if (symbol >= static_cast<int>(LENGTH_BASE.size())) {
        return false;
    }
    size_t length = LENGTH_BASE[symbol] + in.bits(LENGTH_EXTRA[symbol]);
    int distSymbol = decodeSymbol(in, distances);
    if (distSymbol < 0 || distSymbol >= DISTANCE_CODES) {
        return false;
    }
    size_t distance = DISTANCE_BASE[distSymbol] + in.bits(DISTANCE_EXTRA[distSymbol]);
    return !in.failed && copyMatch(out, length, distance);
}

/* The literals and matches of one compressed block, up to its end-of-block symbol. */
bool codedBlock(Input &in, Output &out, const Huffman &literals, const Huffman &distances)
{
    for (;;) {
        int symbol = decodeSymbol(in, literals);
        if (symbol < 0) {
            return false;
        }
        if (symbol == END_OF_BLOCK) {
            return true;
        }
        if (symbol < END_OF_BLOCK) {
            if (out.bytes.size() >= out.limit) {
                return false;
            }
            out.bytes.push_back(static_cast<uint8_t>(symbol));
        } else if (!matchAfter(in, out, symbol, distances)) {
            return false;
        }
    }
}

bool fixedBlock(Input &in, Output &out)
{
    uint8_t lengths[LITERAL_CODES];
    for (int i = 0; i < LITERAL_CODES; i++) {
        lengths[i] = static_cast<uint8_t>(i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8);
    }
    uint8_t distanceLengths[DISTANCE_CODES];
    std::fill(distanceLengths, distanceLengths + DISTANCE_CODES, uint8_t{5});
    Huffman literals;
    Huffman distances;
    return buildHuffman(literals, lengths, LITERAL_CODES) &&
           buildHuffman(distances, distanceLengths, DISTANCE_CODES) &&
           codedBlock(in, out, literals, distances);
}

/* Reads `count` code lengths coded with `codes` (the repeat symbols 16..18 included). */
bool readLengths(Input &in, const Huffman &codes, uint8_t *lengths, int count)
{
    for (int i = 0; i < count;) {
        int symbol = decodeSymbol(in, codes);
        if (symbol < 0) {
            return false;
        }
        if (symbol < 16) {
            lengths[i++] = static_cast<uint8_t>(symbol);
            continue;
        }
        uint8_t value = 0;
        int repeat = 0;
        if (symbol == 16) {
            if (i == 0) {
                return false;
            }
            value = lengths[i - 1];
            repeat = 3 + static_cast<int>(in.bits(2));
        } else {
            repeat =
                symbol == 17 ? 3 + static_cast<int>(in.bits(3)) : 11 + static_cast<int>(in.bits(7));
        }
        if (in.failed || i + repeat > count) {
            return false;
        }
        std::fill(lengths + i, lengths + i + repeat, value);
        i += repeat;
    }
    return !in.failed;
}

bool dynamicBlock(Input &in, Output &out)
{
    int literalCount = static_cast<int>(in.bits(5)) + 257;
    int distanceCount = static_cast<int>(in.bits(5)) + 1;
    int codeLengthCount = static_cast<int>(in.bits(4)) + 4;
    if (in.failed || literalCount > 286 || distanceCount > DISTANCE_CODES) {
        return false;
    }
    uint8_t lengths[LITERAL_CODES + DISTANCE_CODES] = {};
    for (int i = 0; i < codeLengthCount; i++) {
        lengths[CODE_LENGTH_ORDER[static_cast<size_t>(i)]] = static_cast<uint8_t>(in.bits(3));
    }
    Huffman codes;
    if (in.failed || !buildHuffman(codes, lengths, 19)) {
        return false;
    }
    uint8_t all[LITERAL_CODES + DISTANCE_CODES] = {};
    if (!readLengths(in, codes, all, literalCount + distanceCount)) {
        return false;
    }
    Huffman literals;
    Huffman distances;
    return buildHuffman(literals, all, literalCount) &&
           buildHuffman(distances, all + literalCount, distanceCount) &&
           codedBlock(in, out, literals, distances);
}

/* Runs one block of the type its two header bits name. */
bool block(Input &in, Output &out, uint32_t type)
{
    switch (type) {
    case 0:
        return storedBlock(in, out);
    case 1:
        return fixedBlock(in, out);
    case 2:
        return dynamicBlock(in, out);
    default:
        return false;
    }
}

} // namespace

std::optional<std::vector<uint8_t>> zlib_inflate(const uint8_t *data, size_t size, size_t maxBytes)
{
    if (size < 2 || (data[0] & 0x0F) != 8 || ((data[0] << 8) | data[1]) % 31 != 0 ||
        (data[1] & 0x20) != 0) {
        return std::nullopt;
    }
    Input in{data, size};
    in.pos = 2;
    Output out{{}, maxBytes};
    bool last = false;
    while (!last) {
        last = in.bits(1) == 1;
        uint32_t type = in.bits(2);
        if (in.failed || !block(in, out, type)) {
            return std::nullopt;
        }
    }
    return std::move(out.bytes);
}
