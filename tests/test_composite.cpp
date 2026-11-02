#include <memory>
#include <vector>

#include "minitest.hpp"
#include "tinysprite/composite.hpp"
#include "tinysprite/sprite.hpp"
#include "tinysprite/tilemap.hpp"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};
static const Rgba CLEAR = {0, 0, 0, 0};

static bool same(Rgba a, Rgba b)
{
    return a == b;
}

static std::vector<Rgba> make_out(const Sprite &s, Rgba fill)
{
    return std::vector<Rgba>(static_cast<size_t>(s.width()) * static_cast<size_t>(s.height()),
                             fill);
}

static Rgba at(const Sprite &s, const std::vector<Rgba> &out, int x, int y)
{
    return out[y * s.width() + x];
}

TEST(blank_sprite_composites_to_transparent)
{
    Sprite s(4, 3);
    std::vector<Rgba> out = make_out(s, RED);
    composite_sprite(s, 0, Rect{0, 0, 4, 3}, out.data());
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 4; x++) {
            ASSERT_TRUE(same(at(s, out, x, y), CLEAR));
        }
    }
}

TEST(opaque_pixel_is_copied)
{
    Sprite s(4, 3);
    s.layer(0)->cel(0)->set(2, 1, RED);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_sprite(s, 0, Rect{0, 0, 4, 3}, out.data());
    ASSERT_TRUE(same(at(s, out, 2, 1), RED));
    ASSERT_TRUE(same(at(s, out, 1, 1), CLEAR));
}

TEST(upper_layer_covers_lower_layer)
{
    Sprite s(2, 2);
    Layer *top = &s.addLayer("top");
    s.layer(0)->cel(0)->set(0, 0, BLUE);
    top->cel(0)->set(0, 0, RED);
    s.layer(0)->cel(0)->set(1, 0, BLUE);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_sprite(s, 0, Rect{0, 0, 2, 2}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), RED));
    ASSERT_TRUE(same(at(s, out, 1, 0), BLUE));
}

TEST(hidden_layer_is_skipped)
{
    Sprite s(2, 2);
    Layer *top = &s.addLayer("top");
    s.layer(0)->cel(0)->set(0, 0, BLUE);
    top->cel(0)->set(0, 0, RED);
    top->setVisible(false);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_sprite(s, 0, Rect{0, 0, 2, 2}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), BLUE));
}

TEST(layer_opacity_scales_alpha)
{
    Sprite s(2, 2);
    s.layer(0)->cel(0)->set(0, 0, RED);
    s.layer(0)->setOpacity(128);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_sprite(s, 0, Rect{0, 0, 2, 2}, out.data());
    Rgba p = at(s, out, 0, 0);
    ASSERT_EQ_INT(255, p.r);
    ASSERT_EQ_INT(0, p.g);
    ASSERT_EQ_INT(0, p.b);
    ASSERT_EQ_INT(128, p.a);
}

TEST(zero_opacity_layer_contributes_nothing)
{
    Sprite s(2, 2);
    s.layer(0)->cel(0)->set(0, 0, RED);
    s.layer(0)->setOpacity(0);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_sprite(s, 0, Rect{0, 0, 2, 2}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), CLEAR));
}

TEST(translucent_over_opaque_blends)
{
    Sprite s(2, 2);
    Layer *top = &s.addLayer("top");
    s.layer(0)->cel(0)->set(0, 0, BLUE);
    top->cel(0)->set(0, 0, Rgba{255, 0, 0, 128});
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_sprite(s, 0, Rect{0, 0, 2, 2}, out.data());
    Rgba p = at(s, out, 0, 0);
    ASSERT_EQ_INT(255, p.a);
    ASSERT_TRUE(p.r >= 127 && p.r <= 129);
    ASSERT_EQ_INT(0, p.g);
    ASSERT_TRUE(p.b >= 126 && p.b <= 128);
}

TEST(region_only_touches_requested_rect)
{
    Sprite s(4, 4);
    for (int i = 0; i < 4; i++) {
        s.layer(0)->cel(0)->set(i, i, RED);
    }
    std::vector<Rgba> out = make_out(s, BLUE);
    composite_sprite(s, 0, Rect{1, 1, 2, 2}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), BLUE));
    ASSERT_TRUE(same(at(s, out, 1, 1), RED));
    ASSERT_TRUE(same(at(s, out, 2, 2), RED));
    ASSERT_TRUE(same(at(s, out, 2, 1), CLEAR));
    ASSERT_TRUE(same(at(s, out, 3, 3), BLUE));
}

TEST(region_is_clipped_to_the_sprite)
{
    Sprite s(3, 3);
    s.layer(0)->cel(0)->set(2, 2, RED);
    std::vector<Rgba> out = make_out(s, BLUE);
    composite_sprite(s, 0, Rect{-5, -5, 100, 100}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), CLEAR));
    ASSERT_TRUE(same(at(s, out, 2, 2), RED));
    composite_sprite(s, 0, Rect{10, 10, 4, 4}, out.data());
    composite_sprite(s, 0, Rect{0, 0, 0, 0}, out.data());
}

TEST(invalid_frame_leaves_output_untouched)
{
    Sprite s(2, 2);
    std::vector<Rgba> out = make_out(s, BLUE);
    composite_sprite(s, 5, Rect{0, 0, 2, 2}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), BLUE));
}

TEST(composite_pixel_matches_full_composite)
{
    Sprite s(3, 3);
    Layer *top = &s.addLayer("top");
    s.layer(0)->cel(0)->set(1, 1, BLUE);
    top->cel(0)->set(1, 1, RED);
    top->setOpacity(128);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_sprite(s, 0, Rect{0, 0, 3, 3}, out.data());
    ASSERT_TRUE(same(composite_pixel(s, 0, 1, 1), at(s, out, 1, 1)));
    ASSERT_TRUE(same(composite_pixel(s, 0, 0, 0), CLEAR));
    ASSERT_TRUE(same(composite_pixel(s, 0, -1, 0), CLEAR));
    ASSERT_TRUE(same(composite_pixel(s, 0, 3, 0), CLEAR));
    ASSERT_TRUE(same(composite_pixel(s, 4, 1, 1), CLEAR));
}

TEST(composite_over_scales_by_opacity)
{
    Rgba black = {0, 0, 0, 255};
    Rgba white = {255, 255, 255, 255};
    Rgba half = composite_over(black, white, 128);
    ASSERT_EQ_INT(255, half.a);
    ASSERT_EQ_INT(128, half.r);
    ASSERT_TRUE(same(composite_over(black, white, 255), white));
    ASSERT_TRUE(same(composite_over(black, white, 0), black));
    ASSERT_TRUE(same(composite_over(CLEAR, white, 255), white));
    ASSERT_EQ_INT(0, composite_over(CLEAR, white, 0).a);
}

/* A 4 x 4 sprite with one 2 x 2 tile (red top-left pixel) and a 2 x 2 map whose first cell uses it.
 */
static Sprite sprite_with_map(bool visible)
{
    Sprite s(4, 4);
    Cel cel(2, 2);
    cel.set(0, 0, RED);
    Tileset tiles;
    tiles.tileWidth = 2;
    tiles.tileHeight = 2;
    tiles.atlas = std::make_shared<Cel>(cel);
    TileMap map = tilemap_blank("map", 0, 2, 2);
    map.cells[0] = 1;
    map.visible = visible;
    s.setTileData({tiles}, {map});
    return s;
}

TEST(a_visible_tilemap_is_drawn_over_the_layers)
{
    Sprite s = sprite_with_map(true);
    s.layer(0)->cel(0)->set(0, 0, BLUE);
    s.layer(0)->cel(0)->set(3, 3, BLUE);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_sprite(s, 0, Rect{0, 0, 4, 4}, out.data());
    composite_tilemaps(s, Rect{0, 0, 4, 4}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), RED));
    ASSERT_TRUE(same(at(s, out, 1, 0), CLEAR));
    ASSERT_TRUE(same(at(s, out, 3, 3), BLUE)); /* an empty cell shows the layers */
}

TEST(a_hidden_tilemap_is_not_drawn)
{
    Sprite s = sprite_with_map(false);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_tilemaps(s, Rect{0, 0, 4, 4}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), CLEAR));
}

TEST(a_tilemap_is_clipped_to_the_region_and_the_sprite)
{
    Sprite s = sprite_with_map(true);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_tilemaps(s, Rect{1, 0, 3, 4}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), CLEAR)); /* outside the region */
    composite_tilemaps(s, Rect{-5, -5, 20, 20}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), RED));
    composite_tilemaps(s, Rect{0, 0, 4, 4}, nullptr); /* nothing to write to */
}

/* The map of sprite_with_map moved to (x, y), its first cell holding `cell`. */
static void place_map(Sprite &s, int x, int y, uint32_t cell)
{
    TileMap map = s.tilemaps()[0];
    map.x = x;
    map.y = y;
    map.cells[0] = cell;
    s.setTileData(s.tilesets(), {map});
}

TEST(a_tilemap_is_drawn_at_its_offset)
{
    Sprite s = sprite_with_map(true);
    place_map(s, 1, 1, 1);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_tilemaps(s, Rect{0, 0, 4, 4}, out.data());
    ASSERT_TRUE(same(at(s, out, 1, 1), RED));
    ASSERT_TRUE(same(at(s, out, 0, 0), CLEAR));
}

TEST(a_tilemap_past_the_edges_is_cut_off)
{
    Sprite s = sprite_with_map(true);
    place_map(s, -1, -1,
              1 | TILE_FLIP_X | TILE_FLIP_Y); /* the red pixel is at (1, 1) of the tile */
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_tilemaps(s, Rect{0, 0, 4, 4}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), RED));
    place_map(s, 3, 3, 1);
    out = make_out(s, CLEAR);
    composite_tilemaps(s, Rect{0, 0, 4, 4}, out.data());
    ASSERT_TRUE(same(at(s, out, 3, 3), RED));
}

TEST(a_turned_tile_is_drawn_turned)
{
    Sprite s = sprite_with_map(true);
    place_map(s, 0, 0, 1 | TILE_FLIP_X);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_tilemaps(s, Rect{0, 0, 4, 4}, out.data());
    ASSERT_TRUE(same(at(s, out, 1, 0), RED));
    ASSERT_TRUE(same(at(s, out, 0, 0), CLEAR));
}

/* Five by four, three frames, with colors that differ per cel, frame and layer: an opaque layer, a
 * half transparent one in Multiply and a hidden one. */
static Sprite varied()
{
    Sprite s(5, 4);
    s.addLayer("multiplied");
    s.addLayer("hidden");
    s.insertFrame(1, s.makeFrame());
    s.insertFrame(2, s.makeFrame());
    s.layer(1)->setBlendMode(BlendMode::Multiply);
    s.layer(1)->setOpacity(180);
    s.layer(2)->setVisible(false);
    for (int l = 0; l < 3; l++) {
        for (int f = 0; f < 3; f++) {
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 5; x++) {
                    const int mix = 20 * l + 30 * f + 7 * x + 11 * y;
                    s.layer(l)->cel(f)->set(x, y,
                                            Rgba{static_cast<uint8_t>(mix),
                                                 static_cast<uint8_t>(255 - mix),
                                                 static_cast<uint8_t>(x * 40),
                                                 static_cast<uint8_t>(l == 0 ? 255 : 90 + y * 40)});
                }
            }
        }
    }
    return s;
}

TEST(a_region_holds_the_same_pixels_as_composite_pixel)
{
    const Sprite s = varied();
    const Rect region{1, 1, 3, 2};
    const Rgba untouched{1, 2, 3, 4};
    std::vector<Rgba> out = make_out(s, untouched);
    composite_sprite(s, 1, region, out.data());
    for (int y = 0; y < s.height(); y++) {
        for (int x = 0; x < s.width(); x++) {
            const Rgba expected =
                rect_contains(region, x, y) ? composite_pixel(s, 1, x, y) : untouched;
            ASSERT_TRUE(same(at(s, out, x, y), expected));
        }
    }
}

int main()
{
    RUN(a_tilemap_is_drawn_at_its_offset);
    RUN(a_tilemap_past_the_edges_is_cut_off);
    RUN(a_turned_tile_is_drawn_turned);
    RUN(a_visible_tilemap_is_drawn_over_the_layers);
    RUN(a_hidden_tilemap_is_not_drawn);
    RUN(a_tilemap_is_clipped_to_the_region_and_the_sprite);
    RUN(blank_sprite_composites_to_transparent);
    RUN(opaque_pixel_is_copied);
    RUN(upper_layer_covers_lower_layer);
    RUN(hidden_layer_is_skipped);
    RUN(layer_opacity_scales_alpha);
    RUN(zero_opacity_layer_contributes_nothing);
    RUN(translucent_over_opaque_blends);
    RUN(region_only_touches_requested_rect);
    RUN(region_is_clipped_to_the_sprite);
    RUN(invalid_frame_leaves_output_untouched);
    RUN(composite_pixel_matches_full_composite);
    RUN(composite_over_scales_by_opacity);
    RUN(a_region_holds_the_same_pixels_as_composite_pixel);
    return TEST_SUMMARY();
}
