#include <string>
#include <vector>

#include "minitest.hpp"
#include "tinysprite/inflate.hpp"

/* Streams made with Python's zlib: a fixed-Huffman block, a dynamic-Huffman block and a stored
 * block. */
static const std::vector<uint8_t> FIXED = {120, 218, 203, 72, 205, 201, 201, 87,
                                           200, 64,  39,  1,  104, 3,   8,   177};
static const std::vector<uint8_t> DYNAMIC = {
    120, 218, 77,  80,  139, 21,  196, 48,  8,   154, 21,  216, 127, 135, 42,  16,  239, 250, 26,
    141, 31,  16,  3,   0,   156, 111, 236, 248, 49,  240, 209, 134, 107, 185, 55,  255, 194, 214,
    100, 63,  174, 253, 123, 153, 140, 24,  48,  23,  58,  71,  91,  161, 25,  228, 200, 76,  155,
    218, 164, 152, 25,  238, 205, 192, 115, 58,  65,  136, 181, 150, 84,  61,  93,  209, 85,  208,
    33,  133, 8,   142, 10,  171, 183, 66,  116, 179, 160, 179, 69,  219, 31,  205, 195, 21,  169,
    75,  253, 233, 226, 243, 171, 0,   252, 133, 121, 59,  43,  96,  183, 175, 222, 35,  44,  231,
    189, 78,  55,  19,  175, 91,  221, 83,  248, 0,   122, 110, 152, 115};
static const std::vector<uint8_t> STORED = {120, 1,   1,   7,   0,  248, 255, 115, 116,
                                            111, 114, 101, 100, 33, 11,  239, 2,   179};
static const std::vector<uint8_t> LONG_RUN = {
    120, 218, 99,  96,  100, 98,  102, 97,  101, 99,  231, 224, 228, 226, 230, 225, 229, 227, 23,
    16,  20,  18,  22,  17,  21,  19,  151, 144, 148, 146, 150, 145, 149, 147, 87,  80,  84,  82,
    86,  81,  85,  83,  215, 208, 212, 210, 214, 209, 213, 211, 55,  48,  52,  50,  54,  49,  53,
    51,  183, 176, 180, 178, 182, 177, 181, 179, 119, 112, 116, 114, 118, 113, 117, 115, 247, 240,
    244, 242, 246, 241, 245, 243, 15,  8,   12,  10,  14,  9,   13,  11,  143, 136, 140, 138, 142,
    137, 141, 139, 79,  72,  76,  74,  78,  73,  77,  75,  207, 200, 204, 202, 206, 201, 205, 203,
    47,  40,  44,  42,  46,  41,  45,  43,  175, 168, 172, 170, 174, 169, 173, 171, 111, 104, 108,
    106, 110, 105, 109, 107, 239, 232, 236, 234, 238, 233, 237, 235, 159, 48,  113, 210, 228, 41,
    83,  167, 77,  159, 49,  115, 214, 236, 57,  115, 231, 205, 95,  176, 112, 209, 226, 37,  75,
    151, 45,  95,  177, 114, 213, 234, 53,  107, 215, 173, 223, 176, 113, 211, 230, 45,  91,  183,
    109, 223, 177, 115, 215, 238, 61,  123, 247, 237, 63,  112, 240, 208, 225, 35,  71,  143, 29,
    63,  113, 242, 212, 233, 51,  103, 207, 157, 191, 112, 241, 210, 229, 43,  87,  175, 93,  191,
    113, 243, 214, 237, 59,  119, 239, 221, 127, 240, 240, 209, 227, 39,  79,  159, 61,  127, 241,
    242, 213, 235, 55,  111, 223, 189, 255, 240, 241, 211, 231, 47,  95,  191, 125, 255, 241, 243,
    215, 239, 63,  127, 255, 253, 103, 24,  245, 255, 168, 255, 71,  253, 63,  234, 255, 81,  255,
    143, 250, 127, 212, 255, 163, 254, 31,  245, 255, 168, 255, 71,  253, 63,  234, 255, 81,  255,
    143, 250, 127, 212, 255, 163, 254, 31,  245, 255, 168, 255, 71,  253, 63,  234, 255, 81,  255,
    143, 250, 127, 212, 255, 163, 254, 31,  245, 255, 168, 255, 71,  253, 63,  234, 255, 81,  255,
    143, 250, 127, 212, 255, 163, 254, 31,  245, 255, 168, 255, 71,  253, 63,  234, 255, 81,  255,
    143, 250, 127, 8,   250, 31,  0,   244, 117, 237, 30};

static std::string text(const std::vector<uint8_t> &bytes)
{
    return std::string(bytes.begin(), bytes.end());
}

static std::string dynamic_text()
{
    /* the 400 bytes the dynamic stream was made from: a seeded draw of "aaaaabbbc" */
    return "aaaabbbbaababbabaaabaaacababacabbcabaabaabcaaaabcbcaaabcbababbabcbabcaacbbbabaabaacaaac"
           "cabcbbbacabcaccababbcacbbbbbaccbbaaacaacaaaaabaaaaaabaaaaacaaabbbbaaabbbaaaacabaaabaaab"
           "cbcacbacabbbaaaaaaaaaaabaaacaabacababaabababacbabbaaaababbaaabcbcbcaaaaaaacaabcabbbaaab"
           "acabababaababacaaabacabaaaaaaabaaaabaabaaaabbcacabbaabaaaabbbbbaabbaaacbbaacaaaabaaabab"
           "abaababaabacaaaaabaacaaaaabbbaacbacaaaabaacaaaacabca";
}

TEST(a_fixed_huffman_stream_is_decoded)
{
    ASSERT_TRUE(text(must_have(zlib_inflate(FIXED.data(), FIXED.size(), 1000))) ==
                "hello hello hello hello");
}

TEST(a_dynamic_huffman_stream_is_decoded)
{
    ASSERT_TRUE(text(must_have(zlib_inflate(DYNAMIC.data(), DYNAMIC.size(), 1000))) ==
                dynamic_text());
}

TEST(a_stored_block_is_decoded)
{
    ASSERT_TRUE(text(must_have(zlib_inflate(STORED.data(), STORED.size(), 1000))) == "stored!");
}

TEST(long_matches_reach_back_across_the_output)
{
    std::vector<uint8_t> out = must_have(zlib_inflate(LONG_RUN.data(), LONG_RUN.size(), 20000));
    ASSERT_EQ_INT(10240, out.size());
    ASSERT_EQ_INT(7, out[7]);
    ASSERT_EQ_INT(255, out[10239]);
}

TEST(output_beyond_the_limit_is_refused)
{
    ASSERT_TRUE(!zlib_inflate(FIXED.data(), FIXED.size(), 22).has_value());
    ASSERT_TRUE(zlib_inflate(FIXED.data(), FIXED.size(), 23).has_value());
    ASSERT_TRUE(!zlib_inflate(LONG_RUN.data(), LONG_RUN.size(), 100).has_value());
}

TEST(a_truncated_stream_is_refused_at_every_length)
{
    /* the last 4 bytes are the checksum, which is not read: the blocks are whole without them */
    for (size_t n = 0; n + 4 < DYNAMIC.size(); n++) {
        ASSERT_TRUE(!zlib_inflate(DYNAMIC.data(), n, 1000).has_value());
    }
}

TEST(a_bad_header_is_refused)
{
    std::vector<uint8_t> bad = FIXED;
    bad[0] = 0x79; /* not deflate */
    ASSERT_TRUE(!zlib_inflate(bad.data(), bad.size(), 1000).has_value());
    bad = FIXED;
    bad[1] ^= 1; /* the check bits do not fit */
    ASSERT_TRUE(!zlib_inflate(bad.data(), bad.size(), 1000).has_value());
    bad = FIXED;
    bad[1] |= 0x20; /* a preset dictionary */
    ASSERT_TRUE(!zlib_inflate(bad.data(), bad.size(), 1000).has_value());
}

TEST(damaged_streams_never_crash)
{
    for (size_t i = 2; i < DYNAMIC.size(); i++) {
        std::vector<uint8_t> damaged = DYNAMIC;
        damaged[i] ^= 0x55;
        (void)zlib_inflate(damaged.data(), damaged.size(), 1000); /* any answer, no crash */
    }
}

int main()
{
    RUN(a_fixed_huffman_stream_is_decoded);
    RUN(a_dynamic_huffman_stream_is_decoded);
    RUN(a_stored_block_is_decoded);
    RUN(long_matches_reach_back_across_the_output);
    RUN(output_beyond_the_limit_is_refused);
    RUN(a_truncated_stream_is_refused_at_every_length);
    RUN(a_bad_header_is_refused);
    RUN(damaged_streams_never_crash);
    return TEST_SUMMARY();
}
