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
    RUN(dropping_a_row_on_itself_changes_nothing);
    RUN(delete_asks_first_and_enter_confirms);
    RUN(escape_cancels_the_delete);
    RUN(the_only_layer_is_never_deleted);
    RUN(the_list_has_focus_after_a_click_on_it_and_loses_it_elsewhere);
    return TEST_SUMMARY();
}
