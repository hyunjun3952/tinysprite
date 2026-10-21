#include <cstdio>
#include <fstream>
#include <string>

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

TEST(save_replaces_atomically_and_reports_failure)
{
    const char *path = "test_pxa_atomic.pxa";
    ASSERT_TRUE(pxa_save(sample(), path));
    ASSERT_TRUE(!std::ifstream(std::string(path) + ".tmp").good());
    ASSERT_TRUE(pxa_load(path).has_value());
    /* A target in a missing directory fails and leaves nothing behind. */
    ASSERT_TRUE(!pxa_save(sample(), "no_such_dir/x.pxa"));
    std::remove(path);
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

/* A 3x2 sprite with two layers and three frames, each frame different and with its own duration. */
static Sprite animated()
{
    Sprite s(3, 2);
    s.addLayer("Top");
    s.insertFrame(1, s.makeFrame());
    s.insertFrame(2, s.makeFrame());
    for (int f = 0; f < 3; f++) {
        s.layer(0)->cel(f)->set(f, 0, RED);
        s.layer(1)->cel(f)->set(0, 1, Rgba{static_cast<uint8_t>(10 * f), 1, 2, 255});
        s.setFrameDuration(f, 40 + 30 * f);
    }
    return s;
}

TEST(frames_and_durations_round_trip)
{
    Sprite s = animated();
    std::optional<Sprite> back = pxa_decode(pxa_encode(s));
    ASSERT_TRUE(back.has_value());
    if (!back) {
        return;
    }
    ASSERT_EQ_INT(3, back->frameCount());
    for (int f = 0; f < 3; f++) {
        ASSERT_EQ_INT(40 + 30 * f, back->frameDuration(f));
        for (int l = 0; l < 2; l++) {
            for (int i = 0; i < 6; i++) {
                ASSERT_TRUE(back->layer(l)->cel(f)->pixels()[i] == s.layer(l)->cel(f)->pixels()[i]);
            }
        }
    }
}

TEST(a_plain_sprite_writes_no_frame_chunk)
{
    std::vector<uint8_t> data = pxa_encode(Sprite(2, 2));
    for (size_t i = 0; i + 4 <= data.size(); i++) {
        ASSERT_TRUE(
            !(data[i] == 'F' && data[i + 1] == 'R' && data[i + 2] == 'A' && data[i + 3] == 'M'));
    }
}

TEST(a_single_frame_with_its_own_duration_round_trips)
{
    Sprite s(2, 2);
    s.setFrameDuration(0, 321);
    std::optional<Sprite> back = pxa_decode(pxa_encode(s));
    ASSERT_TRUE(back.has_value());
    if (!back) {
        return;
    }
    ASSERT_EQ_INT(1, back->frameCount());
    ASSERT_EQ_INT(321, back->frameDuration(0));
}

TEST(a_header_that_claims_more_frames_than_the_data_holds_is_rejected)
{
    std::vector<uint8_t> data = pxa_encode(Sprite(64, 64));
    data[8 + 8 + 8] = 200; /* frame count of HEAD */
    ASSERT_TRUE(!pxa_decode(data));
}

TEST(a_frame_chunk_with_the_wrong_length_is_rejected)
{
    std::vector<uint8_t> data = pxa_encode(animated());
    /* HEAD is 20 bytes after the 8 byte file header; FRAM follows, its length at +4. */
    size_t fram = 8 + 20;
    ASSERT_EQ_CHAR('F', data[fram]);
    data[fram + 4] = 4; /* three frames need 6 bytes */
    ASSERT_TRUE(!pxa_decode(data));
}

TEST(a_zero_duration_is_rejected)
{
    std::vector<uint8_t> data = pxa_encode(animated());
    size_t fram = 8 + 20;
    data[fram + 8] = 0;
    data[fram + 9] = 0;
    ASSERT_TRUE(!pxa_decode(data));
}

TEST(tags_round_trip)
{
    Sprite s = animated();
    s.addTag(Sprite::Tag{"walk", 0, 1, PlayMode::PingPong});
    s.addTag(Sprite::Tag{"\xe6\xad\xa9\xe3\x81\x8f", 1, 2, PlayMode::Reverse});
    std::optional<Sprite> back = pxa_decode(pxa_encode(s));
    ASSERT_TRUE(back.has_value());
    if (!back) {
        return;
    }
    ASSERT_EQ_INT(2, static_cast<int>(back->tags().size()));
    ASSERT_TRUE(back->tags()[0].name == "walk");
    ASSERT_EQ_INT(0, back->tags()[0].from);
    ASSERT_EQ_INT(1, back->tags()[0].to);
    ASSERT_TRUE(back->tags()[0].direction == PlayMode::PingPong);
    ASSERT_TRUE(back->tags()[1].name == s.tags()[1].name);
    ASSERT_TRUE(back->tags()[1].direction == PlayMode::Reverse);
}

TEST(a_sprite_without_tags_writes_no_tag_chunk)
{
    std::vector<uint8_t> data = pxa_encode(animated());
    for (size_t i = 0; i + 4 <= data.size(); i++) {
        ASSERT_TRUE(
            !(data[i] == 'T' && data[i + 1] == 'A' && data[i + 2] == 'G' && data[i + 3] == 'S'));
    }
}

TEST(a_tag_outside_the_frames_is_rejected)
{
    Sprite s = animated();
    s.addTag(Sprite::Tag{"x", 0, 2, PlayMode::Forward});
    std::vector<uint8_t> data = pxa_encode(s);
    /* the TAGS chunk is last: count 2, name len 1, "x", from 4, to 4, direction 1 */
    size_t to = data.size() - 1 - 4;
    ASSERT_EQ_INT(2, data[to]);
    data[to] = 3; /* one frame past the end */
    ASSERT_TRUE(!pxa_decode(data));
}

TEST(a_tag_with_an_unknown_direction_is_rejected)
{
    Sprite s = animated();
    s.addTag(Sprite::Tag{"x", 0, 1, PlayMode::Forward});
    std::vector<uint8_t> data = pxa_encode(s);
    data[data.size() - 1] = 9;
    ASSERT_TRUE(!pxa_decode(data));
}

TEST(a_truncated_tag_chunk_is_rejected)
{
    Sprite s = animated();
    s.addTag(Sprite::Tag{"walk", 0, 1, PlayMode::Forward});
    std::vector<uint8_t> data = pxa_encode(s);
    data.resize(data.size() - 3);
    ASSERT_TRUE(!pxa_decode(data));
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
    RUN(save_replaces_atomically_and_reports_failure);
    RUN(frames_and_durations_round_trip);
    RUN(a_plain_sprite_writes_no_frame_chunk);
    RUN(a_single_frame_with_its_own_duration_round_trips);
    RUN(a_header_that_claims_more_frames_than_the_data_holds_is_rejected);
    RUN(a_frame_chunk_with_the_wrong_length_is_rejected);
    RUN(a_zero_duration_is_rejected);
    RUN(tags_round_trip);
    RUN(a_sprite_without_tags_writes_no_tag_chunk);
    RUN(a_tag_outside_the_frames_is_rejected);
    RUN(a_tag_with_an_unknown_direction_is_rejected);
    RUN(a_truncated_tag_chunk_is_rejected);
    return TEST_SUMMARY();
}
