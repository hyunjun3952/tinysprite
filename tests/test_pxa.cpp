#include <cstdio>

#include "minitest.h"
#include "tinysprite/io.h"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 128};

/* A 5x3 sprite with a palette and two differently configured layers. */
static Sprite sample()
{
    Sprite s(5, 3);
    s.palette().add(RED);
    s.palette().add(BLUE);
    s.layer(0)->cel(0)->set(1, 2, RED);
    Layer &top = s.addLayer("Top");
    top.setVisible(false);
    top.setOpacity(77);
    top.cel(0)->set(4, 0, BLUE);
    return s;
}

static bool same(const Sprite &a, const Sprite &b)
{
    if (a.width() != b.width() || a.height() != b.height() || a.layerCount() != b.layerCount() ||
        a.palette().count() != b.palette().count()) {
        return false;
    }
    for (int i = 0; i < a.palette().count(); i++) {
        if (a.palette().get(i) != b.palette().get(i)) {
            return false;
        }
    }
    for (int i = 0; i < a.layerCount(); i++) {
        const Layer &la = *a.layer(i);
        const Layer &lb = *b.layer(i);
        if (la.name() != lb.name() || la.visible() != lb.visible() ||
            la.opacity() != lb.opacity() || la.locked() != lb.locked()) {
            return false;
        }
        for (int y = 0; y < a.height(); y++) {
            for (int x = 0; x < a.width(); x++) {
                if (la.cel(0)->get(x, y) != lb.cel(0)->get(x, y)) {
                    return false;
                }
            }
        }
    }
    return true;
}

TEST(round_trip_keeps_locked_and_hidden)
{
    Sprite s(2, 2);
    s.layer(0)->setLocked(true);
    s.addLayer("b").setVisible(false);
    std::optional<Sprite> back = pxa_decode(pxa_encode(s));
    ASSERT_TRUE(back.has_value());
    if (back) {
        ASSERT_TRUE(back->layer(0)->locked() && back->layer(0)->visible());
        ASSERT_TRUE(!back->layer(1)->locked() && !back->layer(1)->visible());
    }
}

TEST(round_trip_keeps_everything)
{
    Sprite s = sample();
    std::optional<Sprite> back = pxa_decode(pxa_encode(s));
    if (!back) {
        MT_FAIL("no sprite");
        return;
    }
    ASSERT_TRUE(same(s, *back));
}

TEST(file_round_trip)
{
    Sprite s = sample();
    std::string path = "test_pxa_roundtrip.pxa";
    ASSERT_TRUE(pxa_save(s, path));
    std::optional<Sprite> back = pxa_load(path);
    std::remove(path.c_str());
    if (!back) {
        MT_FAIL("no sprite");
        return;
    }
    ASSERT_TRUE(same(s, *back));
}

TEST(missing_file_gives_nothing)
{
    ASSERT_TRUE(!pxa_load("does/not/exist.pxa"));
    ASSERT_TRUE(!pxa_save(sample(), "does/not/exist/out.pxa"));
}

TEST(bad_magic_and_version_are_rejected)
{
    std::vector<uint8_t> data = pxa_encode(sample());
    std::vector<uint8_t> badMagic = data;
    badMagic[0] ^= 0xff;
    ASSERT_TRUE(!pxa_decode(badMagic));
    std::vector<uint8_t> newer = data;
    newer[4] = static_cast<uint8_t>(PXA_VERSION + 1);
    ASSERT_TRUE(!pxa_decode(newer));
    ASSERT_TRUE(!pxa_decode({}));
}

TEST(truncated_files_are_rejected_or_cut_at_a_chunk_boundary)
{
    std::vector<uint8_t> data = pxa_encode(sample());
    /* file header 8 + HEAD 20 + PALT 18 + the first LAYR (8 + name 1 + 7 + flags 2 + 60 pixels) */
    size_t firstLayerEnd = 8 + 20 + 18 + (8 + 1 + 7 + 2 + 60);
    for (size_t n = 0; n < data.size(); n++) {
        std::optional<Sprite> cut =
            pxa_decode(std::vector<uint8_t>(data.begin(), data.begin() + static_cast<long>(n)));
        if (n < firstLayerEnd) {
            ASSERT_TRUE(!cut); /* never a crash, always a clean refusal */
        }
    }
}

TEST(unknown_chunks_are_skipped)
{
    std::vector<uint8_t> data = pxa_encode(sample());
    const uint8_t extra[] = {'X', 'T', 'R', 'A', 3, 0, 0, 0, 1, 2, 3};
    data.insert(data.end(), extra, extra + sizeof(extra));
    std::optional<Sprite> back = pxa_decode(data);
    if (!back) {
        MT_FAIL("no sprite");
        return;
    }
    ASSERT_TRUE(same(sample(), *back));
}

TEST(layer_before_header_is_rejected)
{
    /* magic + version, then a LAYR chunk first */
    std::vector<uint8_t> data = pxa_encode(sample());
    std::vector<uint8_t> swapped(data.begin(), data.begin() + 8);
    size_t headLen = 8 + 12; /* tag + length + three u32 */
    size_t layers = 8 + headLen;
    size_t paltLen = 8 + 2 + 2 * 4;
    swapped.insert(swapped.end(), data.begin() + static_cast<long>(layers + paltLen), data.end());
    swapped.insert(swapped.end(), data.begin() + 8, data.begin() + static_cast<long>(layers));
    ASSERT_TRUE(!pxa_decode(swapped));
}

TEST(sprite_without_layers_or_with_bad_size_is_rejected)
{
    std::vector<uint8_t> data = pxa_encode(Sprite(2, 2));
    /* drop the only LAYR chunk: 8 header bytes + name len 1 + "Layer 1" 7 + 2 flags + 16 pixels */
    data.resize(data.size() - (8 + 1 + 7 + 2 + 16));
    ASSERT_TRUE(!pxa_decode(data));
    std::vector<uint8_t> zero = pxa_encode(Sprite(2, 2));
    zero[16] = 0; /* width of HEAD: 8 file header + 8 chunk header */
    ASSERT_TRUE(!pxa_decode(zero));
}

int main(void)
{
    RUN(round_trip_keeps_everything);
    RUN(round_trip_keeps_locked_and_hidden);
    RUN(file_round_trip);
    RUN(missing_file_gives_nothing);
    RUN(bad_magic_and_version_are_rejected);
    RUN(truncated_files_are_rejected_or_cut_at_a_chunk_boundary);
    RUN(unknown_chunks_are_skipped);
    RUN(layer_before_header_is_rejected);
    RUN(sprite_without_layers_or_with_bad_size_is_rejected);
    return TEST_SUMMARY();
}
