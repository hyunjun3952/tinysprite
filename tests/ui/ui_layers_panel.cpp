/* The layer list driven with synthetic input: double-click a row to rename the layer in place. */
#include <memory>

#include "editor.h"
#include "layers_panel.h"
#include "minitest.h"
#include "ui_harness.h"

using test_ui::Harness;

namespace
{

/* Where the name of the topmost layer row is (found by hovering): the first row of the Layers panel
 * docked at the right edge of a 1000 px wide window with no menu bar. */
constexpr float ROW_X = 780.0f;
constexpr float ROW_Y = 64.0f;

struct Fixture {
    Harness h;
    std::unique_ptr<Editor> editor;
    std::unique_ptr<LayersPanel> layers;

    Fixture()
    {
        editor = Editor::create(h.renderer, 8, 8);
        layers = std::make_unique<LayersPanel>(*editor);
        h.layersBody = [this] { layers->show(h.panels); };
        h.frame(ROW_X, ROW_Y, false);
    }

    void click()
    {
        h.frame(ROW_X, ROW_Y, true);
        h.frame(ROW_X, ROW_Y, false);
    }
    void doubleClick()
    {
        click();
        click();
        h.frame(ROW_X, ROW_Y, false);
    }
    void type(const char *text)
    {
        ImGui::GetIO().AddInputCharactersUTF8(text);
        h.frame(ROW_X, ROW_Y, false);
    }
    const char *name() const
    {
        return editor->sprite().layer(0)->name().c_str();
    }
};

} // namespace

TEST(double_click_edits_the_name_in_place)
{
    Fixture f;
    f.doubleClick();
    ASSERT_TRUE(ImGui::GetIO().WantTextInput);
    f.type("Hero");
    ASSERT_TRUE(std::string(f.name()) == "Hero");
    f.h.key(ImGuiKey_Enter);
    ASSERT_TRUE(std::string(f.name()) == "Hero");
    /* One undo step restores the old name. */
    f.editor->undo();
    ASSERT_TRUE(std::string(f.name()) == "Layer 1");
}

TEST(escape_keeps_the_old_name_and_records_nothing)
{
    Fixture f;
    f.doubleClick();
    f.type("Hero");
    ASSERT_TRUE(std::string(f.name()) == "Hero"); /* shown live while typing */
    f.h.key(ImGuiKey_Escape);
    ASSERT_TRUE(std::string(f.name()) == "Layer 1");
    ASSERT_TRUE(!f.editor->history().canUndo());
}

TEST(a_single_click_does_not_start_renaming)
{
    Fixture f;
    f.click();
    ASSERT_TRUE(!ImGui::GetIO().WantTextInput);
    f.type("x");
    f.h.key(ImGuiKey_Enter);
    ASSERT_TRUE(std::string(f.name()) == "Layer 1");
}

/* Rows are 32 px tall and the first one starts about 54 px down; the list is top layer first. */
static float row_y(int fromTop)
{
    return 70.0f + 32.0f * static_cast<float>(fromTop);
}

static void add_layers(Fixture &f, int total)
{
    while (f.editor->sprite().layerCount() < total) {
        f.editor->addLayer();
    }
    f.h.frame(ROW_X, row_y(0), false);
    f.h.frame(ROW_X, row_y(0), false);
}

static std::string names(Fixture &f)
{
    std::string out;
    for (int i = 0; i < f.editor->sprite().layerCount(); i++) {
        out += f.editor->sprite().layer(i)->name() +
               (i + 1 < f.editor->sprite().layerCount() ? "," : "");
    }
    return out;
}

TEST(dragging_a_row_moves_the_layer)
{
    Fixture f;
    add_layers(f, 3);
    ASSERT_TRUE(names(f) == "Layer 1,Layer 2,Layer 3");
    f.h.drag(ROW_X, row_y(0), ROW_X, row_y(2)); /* the top layer onto the bottom row */
    ASSERT_TRUE(names(f) == "Layer 3,Layer 1,Layer 2");
    ASSERT_EQ_INT(0, f.editor->activeLayer()); /* the moved layer stays active */
    f.editor->undo();
    ASSERT_TRUE(names(f) == "Layer 1,Layer 2,Layer 3");
}

/* Rows of the list whose pixel at x is the off-white that marks the drop position. */
static int ink_rows(Fixture &f, int x)
{
    int count = 0;
    for (int y = 40; y < 260; y++) {
        unsigned c = f.h.pixel(x, y);
        count += c == 0xf7f5f0u;
    }
    return count;
}

TEST(dragging_a_row_shows_a_line_where_it_will_land)
{
    Fixture f;
    add_layers(f, 3);
    /* a column of the list with no icon or text in it: nothing else is that color there */
    int x = 900;
    while (x < 990 && ink_rows(f, x) != 0) {
        x++;
    }
    ASSERT_TRUE(x < 990);
    /* Hold the top row and move over the bottom one: the layer would end at the bottom. */
    f.h.frame(ROW_X, row_y(0), false);
    f.h.frame(ROW_X, row_y(0), true);
    for (int i = 1; i <= 8; i++) {
        f.h.frame(ROW_X, row_y(0) + 8.0f * static_cast<float>(i), true);
    }
    f.h.frame(ROW_X, row_y(2), true);
    f.h.frame(ROW_X, row_y(2), true);
    ASSERT_TRUE(ink_rows(f, x) >= 2); /* a line about 2 px thick */
    ASSERT_TRUE(ink_rows(f, x) <= 4);
    /* Over its own row nothing would change, so no line. */
    f.h.frame(ROW_X, row_y(0), true);
    f.h.frame(ROW_X, row_y(0), true);
    ASSERT_EQ_INT(0, ink_rows(f, x));
    f.h.frame(ROW_X, row_y(0), false);
}

TEST(dropping_a_row_on_itself_changes_nothing)
{
    Fixture f;
    add_layers(f, 3);
    f.h.drag(ROW_X, row_y(1), ROW_X + 4.0f, row_y(1) + 3.0f);
    ASSERT_TRUE(names(f) == "Layer 1,Layer 2,Layer 3");
}

TEST(delete_asks_first_and_enter_confirms)
{
    Fixture f;
    add_layers(f, 2);
    f.layers->askToDelete();
    f.h.frame(ROW_X, row_y(0), false);
    f.h.frame(ROW_X, row_y(0), false);
    ASSERT_TRUE(f.layers->dialogOpen());
    ASSERT_EQ_INT(2, f.editor->sprite().layerCount());
    f.h.key(ImGuiKey_Enter);
    f.h.frame(ROW_X, row_y(0), false);
    ASSERT_EQ_INT(1, f.editor->sprite().layerCount());
    ASSERT_TRUE(!f.layers->dialogOpen());
    f.editor->undo();
    ASSERT_EQ_INT(2, f.editor->sprite().layerCount());
}

TEST(escape_cancels_the_delete)
{
    Fixture f;
    add_layers(f, 2);
    f.layers->askToDelete();
    f.h.frame(ROW_X, row_y(0), false);
    f.h.frame(ROW_X, row_y(0), false);
    ASSERT_TRUE(f.layers->dialogOpen());
    f.h.key(ImGuiKey_Escape);
    f.h.frame(ROW_X, row_y(0), false);
    ASSERT_EQ_INT(2, f.editor->sprite().layerCount());
    ASSERT_TRUE(!f.layers->dialogOpen());
}

TEST(the_only_layer_is_never_deleted)
{
    Fixture f;
    f.layers->askToDelete();
    f.h.frame(ROW_X, row_y(0), false);
    f.h.frame(ROW_X, row_y(0), false);
    ASSERT_TRUE(!f.layers->dialogOpen());
    ASSERT_EQ_INT(1, f.editor->sprite().layerCount());
}

/* A click on the row `fromTop` with a modifier key held (ImGuiMod_Ctrl / ImGuiMod_Shift). */
static void click_row_with(Fixture &f, int fromTop, ImGuiKey mod)
{
    float y = row_y(fromTop);
    /* Idle frames around each change, so the input queue does not merge them. */
    ImGui::GetIO().AddKeyEvent(mod, true);
    f.h.frame(ROW_X, y, false);
    f.h.frame(ROW_X, y, false);
    f.h.frame(ROW_X, y, true);
    f.h.frame(ROW_X, y, true);
    f.h.frame(ROW_X, y, false);
    f.h.frame(ROW_X, y, false);
    ImGui::GetIO().AddKeyEvent(mod, false);
    f.h.frame(ROW_X, y, false);
    f.h.frame(ROW_X, y, false);
}

static std::string selected(Fixture &f)
{
    std::string out;
    for (int i : f.editor->selectedLayers()) {
        out += std::to_string(i);
    }
    return out;
}

TEST(ctrl_click_adds_and_removes_rows_from_the_selection)
{
    Fixture f;
    add_layers(f, 3);
    ASSERT_TRUE(selected(f) == "2");     /* the new layer on top */
    click_row_with(f, 2, ImGuiMod_Ctrl); /* the bottom row */
    ASSERT_TRUE(selected(f) == "02");
    click_row_with(f, 1, ImGuiMod_Ctrl);
    ASSERT_TRUE(selected(f) == "012");
    click_row_with(f, 1, ImGuiMod_Ctrl);
    ASSERT_TRUE(selected(f) == "02");
}

TEST(shift_click_selects_the_rows_in_between)
{
    Fixture f;
    add_layers(f, 4);
    f.h.frame(ROW_X, row_y(3), false);
    f.h.frame(ROW_X, row_y(3), true); /* a plain click on the bottom row */
    f.h.frame(ROW_X, row_y(3), false);
    ASSERT_TRUE(selected(f) == "0");
    click_row_with(f, 1, ImGuiMod_Shift); /* the second row from the top: layer 2 */
    ASSERT_TRUE(selected(f) == "012");
    ASSERT_EQ_INT(2, f.editor->activeLayer());
}

TEST(a_plain_click_collapses_the_selection)
{
    Fixture f;
    add_layers(f, 3);
    click_row_with(f, 2, ImGuiMod_Ctrl);
    ASSERT_TRUE(selected(f) == "02");
    f.h.frame(ROW_X, row_y(1), false);
    f.h.frame(ROW_X, row_y(1), true);
    f.h.frame(ROW_X, row_y(1), false);
    ASSERT_TRUE(selected(f) == "1");
}

TEST(deleting_a_selection_asks_once_and_is_one_undo_step)
{
    Fixture f;
    add_layers(f, 3);
    click_row_with(f, 1, ImGuiMod_Ctrl); /* layers 2 and 1 */
    f.layers->askToDelete();
    f.h.frame(ROW_X, row_y(0), false);
    f.h.frame(ROW_X, row_y(0), false);
    ASSERT_TRUE(f.layers->dialogOpen());
    f.h.key(ImGuiKey_Enter);
    f.h.frame(ROW_X, row_y(0), false);
    ASSERT_EQ_INT(1, f.editor->sprite().layerCount());
    ASSERT_TRUE(std::string(f.name()) == "Layer 1");
    f.editor->undo();
    ASSERT_EQ_INT(3, f.editor->sprite().layerCount());
}

TEST(selecting_every_layer_blocks_the_delete)
{
    Fixture f;
    add_layers(f, 3);
    click_row_with(f, 2, ImGuiMod_Shift);
    ASSERT_TRUE(selected(f) == "012");
    f.layers->askToDelete();
    f.h.frame(ROW_X, row_y(0), false);
    f.h.frame(ROW_X, row_y(0), false);
    ASSERT_TRUE(!f.layers->dialogOpen());
    ASSERT_EQ_INT(3, f.editor->sprite().layerCount());
}

TEST(dragging_a_selected_row_moves_the_whole_selection)
{
    Fixture f;
    add_layers(f, 4);
    click_row_with(f, 3, ImGuiMod_Ctrl); /* layers 3 and 0 (bottom) */
    ASSERT_TRUE(selected(f) == "03");
    /* Drag the top row (layer 3) down onto the second row from the bottom (layer 1): the block
     * goes under it. */
    f.h.drag(ROW_X, row_y(0), ROW_X, row_y(2));
    ASSERT_TRUE(names(f) == "Layer 1,Layer 4,Layer 2,Layer 3"); /* the block keeps its order */
}

/* Hovers (x, y) for a few frames and reports the cursor shape and whether a tooltip is showing. */
struct Hover {
    int cursor;
    bool tooltip;
};

static Hover hover_at(Fixture &f, float x, float y)
{
    for (int i = 0; i < 4; i++) {
        f.h.frame(x, y, false);
    }
    ImGuiWindow *tip = ImGui::FindWindowByName("##Tooltip_00");
    return Hover{ImGui::GetMouseCursor(), tip && tip->Active && !tip->Hidden};
}

TEST(a_disabled_button_shows_a_blocked_cursor_and_no_tooltip)
{
    Fixture f;
    /* Show tooltips at once: the test frames take far less than the usual hover delays. */
    ImGui::GetStyle().HoverDelayShort = 0.0f;
    ImGui::GetStyle().HoverDelayNormal = 0.0f;
    ImGui::GetStyle().HoverStationaryDelay = 0.0f;
    /* A single layer: the buttons row has New and Copy enabled, Up, Down, Delete and Merge
     * disabled. Sweep the panel to find the row and the buttons. */
    bool sawEnabledTip = false;
    bool sawBlockedCursor = false;
    bool tipOnBlocked = false;
    for (int row = 80; row < 160; row += 8) {
        for (int x = 745; x < 1000; x += 8) {
            Hover h = hover_at(f, static_cast<float>(x), static_cast<float>(row));
            bool blocked = h.cursor == ImGuiMouseCursor_NotAllowed;
            sawEnabledTip = sawEnabledTip || (!blocked && h.tooltip);
            sawBlockedCursor = sawBlockedCursor || blocked;
            tipOnBlocked = tipOnBlocked || (blocked && h.tooltip);
        }
    }
    ASSERT_TRUE(sawEnabledTip);    /* an enabled button still explains itself */
    ASSERT_TRUE(sawBlockedCursor); /* a disabled one says "not allowed" ... */
    ASSERT_TRUE(!tipOnBlocked);    /* ... instead of showing a tooltip */
}

TEST(the_list_has_focus_after_a_click_on_it_and_loses_it_elsewhere)
{
    Fixture f;
    add_layers(f, 2);
    f.click();
    f.h.frame(ROW_X, ROW_Y, false);
    ASSERT_TRUE(f.layers->focused());
    f.h.frame(300, 300, true);
    f.h.frame(300, 300, false);
    f.h.frame(300, 300, false);
    ASSERT_TRUE(!f.layers->focused());
}

int main(void)
{
    RUN(double_click_edits_the_name_in_place);
    RUN(escape_keeps_the_old_name_and_records_nothing);
    RUN(a_single_click_does_not_start_renaming);
    RUN(dragging_a_row_moves_the_layer);
    RUN(dragging_a_row_shows_a_line_where_it_will_land);
    RUN(dropping_a_row_on_itself_changes_nothing);
    RUN(delete_asks_first_and_enter_confirms);
    RUN(escape_cancels_the_delete);
    RUN(the_only_layer_is_never_deleted);
    RUN(ctrl_click_adds_and_removes_rows_from_the_selection);
    RUN(shift_click_selects_the_rows_in_between);
    RUN(a_plain_click_collapses_the_selection);
    RUN(deleting_a_selection_asks_once_and_is_one_undo_step);
    RUN(selecting_every_layer_blocks_the_delete);
    RUN(dragging_a_selected_row_moves_the_whole_selection);
    RUN(a_disabled_button_shows_a_blocked_cursor_and_no_tooltip);
    RUN(the_list_has_focus_after_a_click_on_it_and_loses_it_elsewhere);
    return TEST_SUMMARY();
}
