/* Headless tests for the UI editor state: the document: replacing it, new documents, overlays,
 * revision counter and preferences. Built only when the UI is built; runs under
 * SDL_VIDEODRIVER=dummy. */

#include <utility>

#include "editor.hpp"
#include "minitest.hpp"
#include "ui_editor_support.hpp"

TEST(overlay_settings_survive_opening_a_document)
{
    Editor *e = make_editor();
    ASSERT_TRUE(e->overlays().status && e->overlays().cursor && e->overlays().mirror_axes);
    e->overlays().status = false;
    ASSERT_TRUE(e->setSprite(Sprite(8, 8)));
    ASSERT_TRUE(!e->overlays().status);
    delete e;
}

TEST(revision_counts_every_change_of_the_pixels)
{
    Editor *e = make_editor();
    uint64_t before = e->revision();
    click(e, 1, 1);
    ASSERT_TRUE(e->revision() > before);
    uint64_t afterStroke = e->revision();
    e->undo();
    ASSERT_TRUE(e->revision() > afterStroke);
    delete e;
}

TEST(set_sprite_replaces_the_document_and_starts_over)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Pencil);
    click(e, 1, 1);
    e->selectAll();
    ASSERT_TRUE(e->history().canUndo());

    Sprite loaded(20, 10);
    loaded.addLayer("Second");
    ASSERT_TRUE(e->setSprite(std::move(loaded)));
    ASSERT_EQ_INT(20, e->sprite().width());
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    ASSERT_EQ_INT(0, e->activeLayer());
    ASSERT_TRUE(!e->history().canUndo());
    ASSERT_TRUE(!e->hasSelection());
    ASSERT_TRUE(e->view().needs_fit);
    e->updateTexture(); /* the texture matches the new size */
    click(e, 19, 9);
    ASSERT_EQ_INT(255, alpha(e, 19, 9));
    delete e;
}

TEST(set_sprite_is_refused_while_dragging)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Pencil);
    e->press(0, 0);
    ASSERT_TRUE(!e->setSprite(Sprite(4, 4)));
    ASSERT_EQ_INT(8, e->sprite().width());
    e->release();
    delete e;
}

TEST(new_document_is_blank_with_the_default_palette)
{
    Editor *e = make_editor();
    click(e, 1, 1);
    ASSERT_TRUE(e->newDocument(20, 10));
    ASSERT_EQ_INT(20, e->sprite().width());
    ASSERT_EQ_INT(10, e->sprite().height());
    ASSERT_EQ_INT(16, e->sprite().palette().count());
    ASSERT_EQ_INT(0, alpha(e, 1, 1));
    ASSERT_TRUE(!e->history().canUndo());
    ASSERT_TRUE(!e->newDocument(0, 5));
    ASSERT_TRUE(!e->newDocument(5, 5000));
    ASSERT_EQ_INT(20, e->sprite().width());
    delete e;
}

TEST(preferences_round_trip_through_the_settings)
{
    Editor *a = make_editor();
    a->setShowGrid(false);
    a->overlays().status = false;
    a->overlays().timeline = false;
    a->overlays().timeline_height = 180.0f;
    a->overlays().options_bar = false;
    a->overlays().status_items = 0x15u; /* a few of the status items */
    a->options().brush_size = 5;
    a->options().brush_round = true;
    a->options().pixel_perfect = true;
    a->options().shape_filled = true;
    a->options().symmetry.mirror_x = true;
    a->setForeground(Rgba{10, 20, 30, 255});
    a->selectTool(ToolKind::Star);
    SettingsMap s;
    a->store(s);
    delete a;

    Editor *b = make_editor();
    b->load(s);
    ASSERT_TRUE(!b->showGrid());
    ASSERT_TRUE(!b->overlays().status);
    ASSERT_TRUE(!b->overlays().timeline);
    ASSERT_TRUE(b->overlays().timeline_height == 180.0f);
    ASSERT_TRUE(!b->overlays().options_bar);
    ASSERT_TRUE(b->overlays().status_items == 0x15u);
    ASSERT_TRUE(b->overlays().cursor);
    ASSERT_EQ_INT(5, b->options().brush_size);
    ASSERT_TRUE(b->options().brush_round && b->options().pixel_perfect &&
                b->options().shape_filled);
    ASSERT_TRUE(!b->options().symmetry.mirror_x); /* symmetry always starts off */
    ASSERT_TRUE(b->foreground() == (Rgba{10, 20, 30, 255}));
    ASSERT_TRUE(b->tool() == ToolKind::Star);
    delete b;
}

TEST(loading_bad_preferences_keeps_the_defaults)
{
    Editor *e = make_editor();
    SettingsMap s = {
        {"brush.size", "9999"}, {"color.fg", "zzzzzzzz"}, {"tool", "99"}, {"view.grid", "perhaps"}};
    e->load(s);
    ASSERT_EQ_INT(1, e->options().brush_size);
    ASSERT_TRUE(e->showGrid());
    ASSERT_TRUE(e->tool() == ToolKind::Pencil);
    ASSERT_EQ_INT(255, e->foreground().a);
    delete e;
}

/* Each document tab has its own Editor; the preferences and the clipboard belong to the user, so
 * they follow from one tab to the next. */
TEST(adopting_preferences_copies_the_tool_colors_brush_overlays_and_clipboard)
{
    Editor *a = make_editor();
    Editor *b = make_editor();
    a->selectTool(ToolKind::Bucket);
    a->options().brush_size = 4;
    a->options().brush_round = true;
    a->foregroundFloats()[0] = 0.0f;
    a->foregroundFloats()[2] = 1.0f; /* blue */
    a->overlays().status = false;
    a->selectTool(ToolKind::Pencil);
    click(a, 1, 1);
    a->selectAll();
    ASSERT_TRUE(a->copySelection());
    b->adoptPreferences(*a);
    ASSERT_EQ_INT(static_cast<int>(ToolKind::Pencil), static_cast<int>(b->tool()));
    ASSERT_EQ_INT(4, b->options().brush_size);
    ASSERT_TRUE(b->options().brush_round);
    ASSERT_EQ_INT(255, b->foreground().b);
    ASSERT_TRUE(!b->overlays().status);
    /* the clipboard came along: pasting in the other document works */
    b->selectTool(ToolKind::Select);
    b->paste();
    ASSERT_EQ_INT(255, alpha(b, 1, 1));
    ASSERT_TRUE(b->history().canUndo());
    delete a;
    delete b;
}

TEST(adopting_preferences_takes_the_symmetry_along)
{
    Editor *a = make_editor();
    Editor *b = make_editor();
    a->options().symmetry.mirror_x = true;
    a->options().symmetry.mirror_y = true;
    b->adoptPreferences(*a);
    ASSERT_TRUE(b->options().symmetry.mirror_x);
    ASSERT_TRUE(b->options().symmetry.mirror_y);
    delete a;
    delete b;
}

TEST(adopting_preferences_leaves_the_document_alone)
{
    Editor *a = make_editor();
    Editor *b = make_editor();
    click(b, 2, 2);
    b->adoptPreferences(*a);
    ASSERT_EQ_INT(255, alpha(b, 2, 2));
    ASSERT_TRUE(b->history().canUndo());
    delete a;
    delete b;
}

TEST(the_slices_overlay_is_on_by_default_and_is_a_saved_preference)
{
    Editor *a = make_editor();
    ASSERT_TRUE(a->overlays().slices);
    a->overlays().slices = false;
    SettingsMap settings;
    a->store(settings);
    Editor *b = make_editor();
    ASSERT_TRUE(b->overlays().slices);
    b->load(settings);
    ASSERT_TRUE(!b->overlays().slices);
    delete a;
    delete b;
}

int main()
{
    if (!ui_init()) {
        return 1;
    }
    RUN(overlay_settings_survive_opening_a_document);
    RUN(revision_counts_every_change_of_the_pixels);
    RUN(set_sprite_replaces_the_document_and_starts_over);
    RUN(set_sprite_is_refused_while_dragging);
    RUN(new_document_is_blank_with_the_default_palette);
    RUN(preferences_round_trip_through_the_settings);
    RUN(loading_bad_preferences_keeps_the_defaults);
    RUN(adopting_preferences_copies_the_tool_colors_brush_overlays_and_clipboard);
    RUN(adopting_preferences_takes_the_symmetry_along);
    RUN(adopting_preferences_leaves_the_document_alone);
    RUN(the_slices_overlay_is_on_by_default_and_is_a_saved_preference);
    ui_quit();
    return TEST_SUMMARY();
}
