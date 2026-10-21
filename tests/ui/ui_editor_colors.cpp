/* Headless tests for the UI editor state: colors and the palette: foreground / background, recent
 * colors, palette edits. Built only when the UI is built; runs under SDL_VIDEODRIVER=dummy. */

#include "editor.h"
#include "minitest.h"
#include "ui_editor_support.h"

TEST(swap_colors_exchanges_foreground_and_background)
{
    Editor *e = make_editor();
    ASSERT_EQ_INT(255, e->foreground().r);
    ASSERT_EQ_INT(0, e->foreground().g);
    e->swapColors();
    ASSERT_EQ_INT(255, e->foreground().g);
    ASSERT_EQ_INT(255, e->backgroundFloats()[0] > 0.5f ? 255 : 0);
    ASSERT_EQ_INT(0, e->backgroundFloats()[1] > 0.5f ? 255 : 0);
    e->swapColors();
    ASSERT_EQ_INT(0, e->foreground().g);
    delete e;
}

TEST(drawing_records_recent_colors)
{
    Editor *e = make_editor();
    ASSERT_EQ_INT(0, e->recent().size());
    dot(e, 0, 0);
    ASSERT_EQ_INT(1, e->recent().size());
    ASSERT_EQ_INT(255, e->recent()[0].r);
    dot(e, 1, 0); /* the same color again */
    ASSERT_EQ_INT(1, e->recent().size());

    e->setForeground(Rgba{0, 0, 255, 255});
    e->selectTool(ToolKind::Bucket);
    click(e, 5, 5);
    ASSERT_EQ_INT(2, e->recent().size());
    ASSERT_EQ_INT(255, e->recent()[0].b);

    e->setForeground(Rgba{0, 255, 0, 255});
    e->selectTool(ToolKind::Rect);
    e->press(2, 2);
    e->drag(3, 3);
    e->release();
    ASSERT_EQ_INT(3, e->recent().size());
    delete e;
}

TEST(eraser_and_picker_do_not_record_colors)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Eraser);
    click(e, 0, 0);
    e->selectTool(ToolKind::Eyedropper);
    click(e, 0, 0);
    ASSERT_EQ_INT(0, e->recent().size());
    delete e;
}

TEST(set_color_changes_the_foreground)
{
    Editor *e = make_editor();
    e->setForeground(Rgba{0, 128, 255, 51});
    Rgba c = e->foreground();
    ASSERT_EQ_INT(0, c.r);
    ASSERT_EQ_INT(128, c.g);
    ASSERT_EQ_INT(255, c.b);
    ASSERT_EQ_INT(51, c.a);
    delete e;
}

TEST(palette_starts_with_default_colors)
{
    Editor *e = make_editor();
    ASSERT_TRUE(e->sprite().palette().count() > 0);
    delete e;
}

TEST(palette_add_uses_the_current_color_once)
{
    Editor *e = make_editor();
    Palette &p = e->sprite().palette();
    int before = p.count();
    e->setForeground(Rgba{12, 34, 56, 255});
    int index = e->paletteAdd();
    ASSERT_EQ_INT(before, index);
    ASSERT_EQ_INT(before + 1, p.count());
    ASSERT_EQ_INT(index, e->paletteSelected());
    ASSERT_EQ_INT(56, p.get(index).b);
    ASSERT_EQ_INT(index, e->paletteAdd()); /* already there: no duplicate */
    ASSERT_EQ_INT(before + 1, p.count());
    delete e;
}

TEST(palette_remove_keeps_the_selection_on_the_same_color)
{
    Editor *e = make_editor();
    Palette &p = e->sprite().palette();
    int count = p.count();
    e->palettePick(count - 1);
    Rgba last = p.get(count - 1);
    e->paletteRemove(0);
    ASSERT_EQ_INT(count - 1, p.count());
    ASSERT_EQ_INT(count - 2, e->paletteSelected());
    ASSERT_EQ_INT(last.r, p.get(e->paletteSelected()).r);
    e->paletteRemove(e->paletteSelected()); /* removing the selected swatch deselects */
    ASSERT_TRUE(e->paletteSelected() < p.count());
    e->paletteRemove(999);
    ASSERT_EQ_INT(count - 2, p.count());
    delete e;
}

TEST(pick_palette_color_sets_the_foreground)
{
    Editor *e = make_editor();
    Palette &p = e->sprite().palette();
    Rgba want = p.get(2);
    e->palettePick(2);
    ASSERT_EQ_INT(2, e->paletteSelected());
    ASSERT_EQ_INT(want.g, e->foreground().g);
    ASSERT_EQ_INT(want.b, e->foreground().b);
    e->palettePick(999);
    ASSERT_EQ_INT(2, e->paletteSelected());
    delete e;
}

TEST(palette_edits_are_unsaved_changes_until_saved_or_undone)
{
    Editor *e = make_editor();
    ASSERT_TRUE(e->history().atSavePoint());
    e->setForeground(Rgba{1, 2, 3, 255});
    e->paletteAdd();
    ASSERT_TRUE(!e->history().atSavePoint());
    e->markSaved();
    ASSERT_TRUE(e->history().atSavePoint());
    e->paletteRemove(0);
    ASSERT_TRUE(!e->history().atSavePoint());
    e->undo();
    ASSERT_TRUE(e->history().atSavePoint());
    ASSERT_TRUE(!e->modifiedOutsideHistory());
    delete e;
}

TEST(palette_add_remove_and_reset_undo_and_redo)
{
    Editor *e = make_editor();
    Palette original = e->sprite().palette();
    e->setForeground(Rgba{9, 8, 7, 255});
    e->paletteAdd();
    Palette added = e->sprite().palette();
    e->paletteRemove(0);
    Palette removed = e->sprite().palette();
    e->paletteReset();
    e->undo();
    ASSERT_TRUE(e->sprite().palette() == removed);
    e->undo();
    ASSERT_TRUE(e->sprite().palette() == added);
    e->undo();
    ASSERT_TRUE(e->sprite().palette() == original);
    e->redo();
    e->redo();
    ASSERT_TRUE(e->sprite().palette() == removed);
    delete e;
}

TEST(undoing_a_palette_edit_drops_a_swatch_selection_that_no_longer_exists)
{
    Editor *e = make_editor();
    e->setForeground(Rgba{9, 8, 7, 255});
    int index = e->paletteAdd();
    ASSERT_EQ_INT(index, e->paletteSelected());
    e->undo();
    ASSERT_EQ_INT(-1, e->paletteSelected());
    delete e;
}

TEST(a_palette_edit_during_a_stroke_is_refused)
{
    Editor *e = make_editor();
    int count = e->sprite().palette().count();
    e->selectTool(ToolKind::Pencil);
    e->press(1, 1);
    e->setForeground(Rgba{9, 8, 7, 255});
    ASSERT_EQ_INT(-1, e->paletteAdd());
    e->paletteRemove(0);
    ASSERT_EQ_INT(count, e->sprite().palette().count());
    e->release();
    delete e;
}

TEST(palette_reset_restores_the_default_colors)
{
    Editor *e = make_editor();
    int defaults = e->sprite().palette().count();
    e->setForeground(Rgba{1, 2, 3, 255});
    e->paletteAdd();
    e->paletteRemove(0);
    e->markSaved();
    e->paletteReset();
    ASSERT_EQ_INT(defaults, e->sprite().palette().count());
    ASSERT_EQ_INT(-1, e->paletteSelected());
    ASSERT_TRUE(!e->history().atSavePoint());
    ASSERT_TRUE(e->sprite().palette().get(0) == (Rgba{0, 0, 0, 255}));
    delete e;
}

TEST(set_palette_replaces_it_as_an_undoable_unsaved_change)
{
    Editor *e = make_editor();
    Palette p;
    p.add(Rgba{1, 2, 3, 255});
    e->markSaved();
    e->setPalette(p);
    ASSERT_EQ_INT(1, e->sprite().palette().count());
    ASSERT_EQ_INT(-1, e->paletteSelected());
    ASSERT_TRUE(!e->history().atSavePoint());
    e->undo();
    ASSERT_TRUE(e->sprite().palette().count() > 1);
    e->redo();
    e->markSaved();
    e->markModified();
    ASSERT_TRUE(e->modifiedOutsideHistory());
    delete e;
}

int main(void)
{
    if (!ui_init()) {
        return 1;
    }
    RUN(swap_colors_exchanges_foreground_and_background);
    RUN(drawing_records_recent_colors);
    RUN(eraser_and_picker_do_not_record_colors);
    RUN(set_color_changes_the_foreground);
    RUN(palette_starts_with_default_colors);
    RUN(palette_add_uses_the_current_color_once);
    RUN(palette_remove_keeps_the_selection_on_the_same_color);
    RUN(pick_palette_color_sets_the_foreground);
    RUN(palette_edits_are_unsaved_changes_until_saved_or_undone);
    RUN(palette_add_remove_and_reset_undo_and_redo);
    RUN(undoing_a_palette_edit_drops_a_swatch_selection_that_no_longer_exists);
    RUN(a_palette_edit_during_a_stroke_is_refused);
    RUN(palette_reset_restores_the_default_colors);
    RUN(set_palette_replaces_it_as_an_undoable_unsaved_change);
    ui_quit();
    return TEST_SUMMARY();
}
