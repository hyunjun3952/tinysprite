/* Headless tests for the UI editor state: layer operations: add / remove / move, visibility and
 * lock, multi-layer selection, duplicate and merge. Built only when the UI is built; runs under
 * SDL_VIDEODRIVER=dummy. */

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "editor.hpp"
#include "minitest.hpp"
#include "ui_editor_support.hpp"

/* An editor with layers "Layer 1" .. "Layer n" (index 0 is the bottom one). */
static Editor *editor_with_layers(int n)
{
    Editor *e = make_editor();
    while (e->sprite().layerCount() < n) {
        e->addLayer();
    }
    return e;
}

static std::string layer_names(Editor *e)
{
    std::string out;
    for (int i = 0; i < e->sprite().layerCount(); i++) {
        out += e->sprite().layer(i)->name().substr(6);
    }
    return out; /* "1234": the numbers of the layers, bottom first */
}

static std::string selection(Editor *e)
{
    std::string out;
    for (int i : e->selectedLayers()) {
        out += std::to_string(i);
    }
    return out;
}

/* Layers 1..4 (index 0..3), each with one dot in its own column so a merge shows where it came
 * from. */
static Editor *editor_with_dots()
{
    Editor *e = editor_with_layers(4);
    for (int i = 0; i < 4; i++) {
        e->sprite().layer(i)->cel(0)->set(i, 0, Rgba{255, 0, 0, 255});
    }
    e->markAllDirty();
    return e;
}

static bool has_dot(Editor *e, int layer, int x)
{
    return e->sprite().layer(layer)->cel(0)->get(x, 0).a == 255;
}

TEST(layer_add_remove_move_undo_redo)
{
    Editor *e = make_editor();
    e->addLayer();
    e->addLayer();
    ASSERT_EQ_INT(3, e->sprite().layerCount());
    ASSERT_EQ_INT(2, e->activeLayer());
    e->selectTool(ToolKind::Pencil);
    click(e, 1, 1);
    e->removeLayer(2);
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    ASSERT_EQ_INT(1, e->activeLayer());
    e->undo();
    ASSERT_EQ_INT(3, e->sprite().layerCount());
    e->undo(); /* the pixel on the restored layer */
    e->undo(); /* second add */
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    ASSERT_EQ_INT(1, e->activeLayer());
    e->undo();
    ASSERT_EQ_INT(1, e->sprite().layerCount());
    ASSERT_EQ_INT(0, e->activeLayer());
    e->redo();
    e->redo();
    ASSERT_EQ_INT(3, e->sprite().layerCount());
    delete e;
}

TEST(visibility_toggle_is_undoable)
{
    Editor *e = make_editor();
    e->setLayerVisible(0, false);
    ASSERT_TRUE(!e->sprite().layer(0)->visible());
    e->undo();
    ASSERT_TRUE(e->sprite().layer(0)->visible());
    delete e;
}

TEST(live_name_and_opacity_edits_commit_as_one_step)
{
    Editor *e = make_editor();
    Layer *layer = e->sprite().layer(0);
    layer->setName("B");
    layer->setName("Bo");
    layer->setName("Body");
    e->commitLayerName(0, "Layer 1");
    layer->setOpacity(200);
    layer->setOpacity(90);
    e->commitSelectedLayersOpacity({{0, 255}});
    ASSERT_TRUE(strcmp(layer->name().c_str(), "Body") == 0);
    ASSERT_EQ_INT(90, layer->opacity());
    e->undo();
    ASSERT_EQ_INT(255, layer->opacity());
    e->undo();
    ASSERT_TRUE(strcmp(layer->name().c_str(), "Layer 1") == 0);
    ASSERT_TRUE(!e->history().canUndo());
    e->redo();
    ASSERT_TRUE(strcmp(layer->name().c_str(), "Body") == 0);
    delete e;
}

TEST(unchanged_live_edit_records_nothing)
{
    Editor *e = make_editor();
    e->commitLayerName(0, "Layer 1");
    e->commitSelectedLayersOpacity({{0, 255}});
    ASSERT_TRUE(!e->history().canUndo());
    delete e;
}

TEST(layer_edits_are_held_off_while_dragging)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Pencil);
    e->press(0, 0);
    e->addLayer();
    e->setLayerVisible(0, false);
    ASSERT_EQ_INT(1, e->sprite().layerCount());
    ASSERT_TRUE(e->sprite().layer(0)->visible());
    e->release();
    delete e;
}

TEST(tools_ignore_a_hidden_layer)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Pencil);
    e->setLayerVisible(0, false);
    click(e, 2, 2);
    ASSERT_EQ_INT(0, alpha(e, 2, 2));
    delete e;
}

TEST(lock_toggle_is_undoable)
{
    Editor *e = make_editor();
    e->setLayerLocked(0, true);
    ASSERT_TRUE(e->sprite().layer(0)->locked());
    e->undo();
    ASSERT_TRUE(!e->sprite().layer(0)->locked());
    delete e;
}

TEST(tools_ignore_a_locked_layer)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Pencil);
    e->setLayerLocked(0, true);
    click(e, 2, 2);
    ASSERT_EQ_INT(0, alpha(e, 2, 2));
    e->setLayerLocked(0, false);
    click(e, 2, 2);
    ASSERT_EQ_INT(255, alpha(e, 2, 2));
    delete e;
}

TEST(a_plain_pick_selects_only_that_layer)
{
    Editor *e = editor_with_layers(4);
    e->pickLayer(1, LayerPick::Only);
    ASSERT_TRUE(selection(e) == "1");
    ASSERT_EQ_INT(1, e->activeLayer());
    e->pickLayer(2, LayerPick::Toggle);
    e->pickLayer(3, LayerPick::Only);
    ASSERT_TRUE(selection(e) == "3");
    delete e;
}

TEST(toggle_adds_and_removes_layers_and_active_follows)
{
    Editor *e = editor_with_layers(4);
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(2, LayerPick::Toggle);
    ASSERT_TRUE(selection(e) == "02");
    ASSERT_EQ_INT(2, e->activeLayer());
    e->pickLayer(2, LayerPick::Toggle); /* the active one leaves: another selected one takes over */
    ASSERT_TRUE(selection(e) == "0");
    ASSERT_EQ_INT(0, e->activeLayer());
    e->pickLayer(0, LayerPick::Toggle); /* the last one cannot be toggled off */
    ASSERT_TRUE(selection(e) == "0");
    delete e;
}

TEST(range_selects_everything_between_the_anchor_and_the_click)
{
    Editor *e = editor_with_layers(5);
    e->pickLayer(1, LayerPick::Only);
    e->pickLayer(3, LayerPick::Range);
    ASSERT_TRUE(selection(e) == "123");
    ASSERT_EQ_INT(3, e->activeLayer());
    e->pickLayer(0, LayerPick::Range); /* from the same anchor, the other way */
    ASSERT_TRUE(selection(e) == "01");
    delete e;
}

TEST(selection_follows_layers_when_the_stack_changes)
{
    Editor *e = editor_with_layers(4);
    e->pickLayer(1, LayerPick::Only);
    e->pickLayer(2, LayerPick::Toggle);
    e->moveLayer(3, 0); /* the top layer goes to the bottom: both selected ones move up */
    ASSERT_TRUE(selection(e) == "23");
    e->addLayer(); /* a new layer becomes the only selection */
    ASSERT_TRUE(selection(e) == "4");
    delete e;
}

TEST(removing_the_selected_layers_is_one_undo_step)
{
    Editor *e = editor_with_layers(4);
    e->pickLayer(1, LayerPick::Only);
    e->pickLayer(2, LayerPick::Toggle);
    e->removeSelectedLayers();
    ASSERT_TRUE(layer_names(e) == "14");
    ASSERT_TRUE(selection(e) == "0"); /* the layer under the removed ones */
    e->undo();
    ASSERT_TRUE(layer_names(e) == "1234");
    e->redo();
    ASSERT_TRUE(layer_names(e) == "14");
    delete e;
}

TEST(the_last_layer_cannot_be_removed_with_a_selection)
{
    Editor *e = editor_with_layers(3);
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(2, LayerPick::Range);
    e->removeSelectedLayers();
    ASSERT_EQ_INT(3, e->sprite().layerCount());
    delete e;
}

TEST(duplicating_the_selected_layers_selects_the_copies)
{
    Editor *e = editor_with_layers(3);
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(2, LayerPick::Toggle);
    e->duplicateSelectedLayers();
    ASSERT_EQ_INT(5, e->sprite().layerCount());
    ASSERT_EQ_INT(2, static_cast<int>(e->selectedLayers().size()));
    e->undo();
    ASSERT_EQ_INT(3, e->sprite().layerCount());
    delete e;
}

TEST(a_selected_block_moves_together)
{
    Editor *e = editor_with_layers(4);
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(2, LayerPick::Toggle);
    e->moveSelectedLayers(2, 3); /* up onto the top layer: the block goes above it */
    ASSERT_TRUE(layer_names(e) == "2413");
    ASSERT_TRUE(selection(e) == "23");
    e->undo();
    ASSERT_TRUE(layer_names(e) == "1234");
    delete e;
}

TEST(a_selected_block_moves_down_below_the_target)
{
    Editor *e = editor_with_layers(4);
    e->pickLayer(2, LayerPick::Only);
    e->pickLayer(3, LayerPick::Range);
    e->moveSelectedLayers(3, 0); /* down onto the bottom layer: the block goes under it */
    ASSERT_TRUE(layer_names(e) == "3412");
    delete e;
}

TEST(dropping_a_block_on_itself_or_dragging_an_unselected_layer)
{
    Editor *e = editor_with_layers(4);
    e->pickLayer(1, LayerPick::Only);
    e->pickLayer(2, LayerPick::Toggle);
    e->moveSelectedLayers(2, 1); /* onto a selected layer: nothing */
    ASSERT_TRUE(layer_names(e) == "1234");
    e->moveSelectedLayers(0, 3); /* an unselected layer moves alone */
    ASSERT_TRUE(layer_names(e) == "2341");
    delete e;
}

TEST(visibility_and_lock_apply_to_the_whole_selection_in_one_step)
{
    Editor *e = editor_with_layers(3);
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(1, LayerPick::Toggle);
    e->setSelectedLayersVisible(false);
    ASSERT_TRUE(!e->sprite().layer(0)->visible() && !e->sprite().layer(1)->visible());
    ASSERT_TRUE(e->sprite().layer(2)->visible());
    e->setSelectedLayersLocked(true);
    ASSERT_TRUE(e->sprite().layer(0)->locked() && e->sprite().layer(1)->locked());
    e->undo();
    ASSERT_TRUE(!e->sprite().layer(0)->locked());
    ASSERT_TRUE(!e->sprite().layer(0)->visible()); /* only the lock was undone */
    e->undo();
    ASSERT_TRUE(e->sprite().layer(0)->visible() && e->sprite().layer(1)->visible());
    delete e;
}

TEST(merging_selected_layers_collects_them_into_the_lowest_one)
{
    Editor *e = editor_with_dots();
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(2, LayerPick::Toggle);
    e->pickLayer(3, LayerPick::Toggle);
    ASSERT_TRUE(e->canMergeLayers());
    e->mergeSelectedLayers();
    ASSERT_TRUE(layer_names(e) ==
                "12"); /* "Layer 1" holds 1, 3 and 4; "Layer 2" is the one left out */
    ASSERT_TRUE(has_dot(e, 0, 0) && has_dot(e, 0, 2) && has_dot(e, 0, 3));
    ASSERT_TRUE(!has_dot(e, 0, 1));
    ASSERT_TRUE(has_dot(e, 1, 1)); /* the unselected layer keeps its own pixels, now on top */
    ASSERT_TRUE(selection(e) == "0");
    ASSERT_EQ_INT(0, e->activeLayer());
    delete e;
}

TEST(merging_the_selection_is_one_undo_step)
{
    Editor *e = editor_with_dots();
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(3, LayerPick::Range);
    e->mergeSelectedLayers();
    ASSERT_EQ_INT(1, e->sprite().layerCount());
    ASSERT_TRUE(has_dot(e, 0, 0) && has_dot(e, 0, 1) && has_dot(e, 0, 2) && has_dot(e, 0, 3));
    e->undo();
    ASSERT_TRUE(layer_names(e) == "1234");
    ASSERT_TRUE(has_dot(e, 0, 0) && !has_dot(e, 0, 1) && has_dot(e, 1, 1) && has_dot(e, 2, 2) &&
                has_dot(e, 3, 3));
    e->redo();
    ASSERT_EQ_INT(1, e->sprite().layerCount());
    delete e;
}

TEST(merge_is_refused_for_a_hidden_layer_or_a_locked_lowest_one)
{
    Editor *e = editor_with_dots();
    e->pickLayer(1, LayerPick::Only);
    e->pickLayer(2, LayerPick::Toggle);
    e->setLayerVisible(2, false);
    ASSERT_TRUE(!e->canMergeLayers());
    e->mergeSelectedLayers();
    ASSERT_EQ_INT(4, e->sprite().layerCount());
    e->setLayerVisible(2, true);
    e->setLayerLocked(1, true);
    ASSERT_TRUE(!e->canMergeLayers());
    e->setLayerLocked(1, false);
    ASSERT_TRUE(e->canMergeLayers());
    delete e;
}

TEST(a_single_selected_layer_merges_down)
{
    Editor *e = editor_with_dots();
    e->pickLayer(2, LayerPick::Only);
    ASSERT_TRUE(e->canMergeLayers());
    e->mergeSelectedLayers();
    ASSERT_TRUE(layer_names(e) == "124");
    ASSERT_TRUE(has_dot(e, 1, 1) && has_dot(e, 1, 2));
    e->pickLayer(0, LayerPick::Only);
    ASSERT_TRUE(!e->canMergeLayers()); /* nothing below the bottom layer */
    delete e;
}

TEST(a_new_document_starts_with_a_single_selected_layer)
{
    Editor *e = editor_with_layers(3);
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(2, LayerPick::Toggle);
    ASSERT_TRUE(e->setSprite(Sprite(4, 4)));
    ASSERT_TRUE(selection(e) == "0");
    delete e;
}

TEST(duplicate_layer_selects_the_copy)
{
    Editor *e = make_editor();
    dot(e, 2, 2);
    e->addLayer();
    e->setActiveLayer(0);
    e->duplicateSelectedLayers();
    ASSERT_EQ_INT(3, e->sprite().layerCount());
    ASSERT_EQ_INT(1, e->activeLayer());
    ASSERT_EQ_INT(255, alpha(e, 2, 2));
    ASSERT_TRUE(e->dirty());
    e->undo();
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    ASSERT_EQ_INT(1, e->activeLayer());
    e->redo();
    ASSERT_EQ_INT(3, e->sprite().layerCount());
    delete e;
}

TEST(merge_layer_down_selects_the_lower_layer)
{
    Editor *e = make_editor();
    dot(e, 1, 1);
    e->addLayer();
    dot(e, 3, 3);
    e->mergeLayerDown(1);
    ASSERT_EQ_INT(1, e->sprite().layerCount());
    ASSERT_EQ_INT(0, e->activeLayer());
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    ASSERT_EQ_INT(255, alpha(e, 3, 3));
    e->undo();
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    ASSERT_EQ_INT(0, alpha(e, 3, 3)); /* active layer 0 again holds only its own pixel */
    e->redo();
    ASSERT_EQ_INT(255, alpha(e, 3, 3));
    delete e;
}

TEST(merge_down_follows_a_higher_active_layer)
{
    Editor *e = make_editor();
    e->addLayer();
    e->addLayer();
    ASSERT_EQ_INT(2, e->activeLayer());
    e->mergeLayerDown(1);
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    ASSERT_EQ_INT(1, e->activeLayer());
    delete e;
}

TEST(duplicate_and_merge_are_held_off_while_dragging_or_invalid)
{
    Editor *e = make_editor();
    e->addLayer();
    e->mergeLayerDown(0); /* nothing below */
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    e->selectTool(ToolKind::Pencil);
    e->press(0, 0);
    e->duplicateSelectedLayers();
    e->mergeLayerDown(1);
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    e->release();
    e->setLayerVisible(1, false);
    e->mergeLayerDown(1); /* hidden layers do not merge */
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    delete e;
}

TEST(merge_is_refused_when_a_lower_selected_layer_is_locked)
{
    Editor *e = make_editor();
    e->addLayer();
    e->addLayer();
    e->sprite().layer(1)->setLocked(true);
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(2, LayerPick::Range);
    ASSERT_TRUE(!e->canMergeLayers());
    e->mergeSelectedLayers();
    ASSERT_EQ_INT(3, e->sprite().layerCount());
    delete e;
}

TEST(merge_is_refused_when_a_blend_would_change_the_look)
{
    Editor *e = editor_with_dots();
    e->sprite().layer(2)->setBlendMode(BlendMode::Multiply);
    e->pickLayer(2, LayerPick::Only); /* Multiply onto a Normal layer that has Base under it */
    ASSERT_TRUE(!e->canMergeLayers());
    e->mergeSelectedLayers();
    ASSERT_EQ_INT(4, e->sprite().layerCount());
    e->pickLayer(1, LayerPick::Only); /* a Normal layer onto Normal still merges */
    ASSERT_TRUE(e->canMergeLayers());
    e->pickLayer(1, LayerPick::Only);
    e->pickLayer(2, LayerPick::Toggle); /* the blended layer folded into one with Base under it */
    ASSERT_TRUE(!e->canMergeLayers());
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(2, LayerPick::Toggle); /* ... but nothing is under the lowest one here */
    ASSERT_TRUE(e->canMergeLayers());
    e->sprite().layer(2)->setBlendMode(BlendMode::Normal);
    e->pickLayer(1, LayerPick::Only);
    e->pickLayer(2, LayerPick::Toggle);
    ASSERT_TRUE(e->canMergeLayers());
    delete e;
}

TEST(undo_and_redo_keep_the_same_layer_active)
{
    Editor *e = make_editor();
    e->addLayer();
    e->addLayer();
    Layer *top = e->sprite().layer(2);
    e->removeLayer(0);
    ASSERT_TRUE(e->sprite().layer(e->activeLayer()) == top);
    e->undo();
    ASSERT_TRUE(e->sprite().layer(e->activeLayer()) == top);
    e->redo();
    ASSERT_TRUE(e->sprite().layer(e->activeLayer()) == top);
    delete e;
}

TEST(opacity_edits_apply_to_every_selected_layer_as_one_step)
{
    Editor *e = editor_with_layers(3);
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(2, LayerPick::Toggle);
    std::vector<std::pair<int, uint8_t>> before = e->selectedLayerOpacities();
    ASSERT_EQ_INT(2, static_cast<int>(before.size()));
    e->setSelectedLayersOpacityLive(100);
    e->setSelectedLayersOpacityLive(80); /* dragging the slider: many live values */
    ASSERT_EQ_INT(80, e->sprite().layer(0)->opacity());
    ASSERT_EQ_INT(80, e->sprite().layer(2)->opacity());
    ASSERT_EQ_INT(255, e->sprite().layer(1)->opacity());
    e->commitSelectedLayersOpacity(before);
    e->undo();
    ASSERT_EQ_INT(255, e->sprite().layer(0)->opacity());
    ASSERT_EQ_INT(255, e->sprite().layer(2)->opacity());
    e->redo();
    ASSERT_EQ_INT(80, e->sprite().layer(0)->opacity());
    ASSERT_EQ_INT(80, e->sprite().layer(2)->opacity());
    delete e;
}

TEST(layers_keep_their_own_opacity_before_a_shared_edit)
{
    Editor *e = editor_with_layers(2);
    e->sprite().layer(0)->setOpacity(50);
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(1, LayerPick::Toggle);
    std::vector<std::pair<int, uint8_t>> before = e->selectedLayerOpacities();
    e->setSelectedLayersOpacityLive(200);
    e->commitSelectedLayersOpacity(before);
    e->undo();
    ASSERT_EQ_INT(50, e->sprite().layer(0)->opacity());
    ASSERT_EQ_INT(255, e->sprite().layer(1)->opacity());
    delete e;
}

TEST(an_unchanged_shared_opacity_records_nothing)
{
    Editor *e = editor_with_layers(2);
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(1, LayerPick::Toggle);
    std::vector<std::pair<int, uint8_t>> before = e->selectedLayerOpacities();
    e->commitSelectedLayersOpacity(before);
    e->undo(); /* undoes the last layer that was added, not an opacity step */
    ASSERT_EQ_INT(1, e->sprite().layerCount());
    delete e;
}

TEST(a_shared_opacity_edit_is_held_off_while_stroking)
{
    Editor *e = editor_with_layers(2);
    e->selectTool(ToolKind::Pencil);
    e->press(1, 1);
    e->setSelectedLayersOpacityLive(10);
    ASSERT_EQ_INT(255, e->sprite().layer(1)->opacity());
    e->release();
    delete e;
}

TEST(blend_mode_applies_to_every_selected_layer_as_one_step)
{
    Editor *e = editor_with_layers(3);
    e->sprite().layer(2)->setBlendMode(BlendMode::Add);
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(2, LayerPick::Toggle);
    e->setSelectedLayersBlendMode(BlendMode::Multiply);
    ASSERT_EQ_INT(static_cast<int>(BlendMode::Multiply),
                  static_cast<int>(e->sprite().layer(0)->blendMode()));
    ASSERT_EQ_INT(static_cast<int>(BlendMode::Normal),
                  static_cast<int>(e->sprite().layer(1)->blendMode()));
    ASSERT_EQ_INT(static_cast<int>(BlendMode::Multiply),
                  static_cast<int>(e->sprite().layer(2)->blendMode()));
    e->undo();
    ASSERT_EQ_INT(static_cast<int>(BlendMode::Normal),
                  static_cast<int>(e->sprite().layer(0)->blendMode()));
    ASSERT_EQ_INT(static_cast<int>(BlendMode::Add),
                  static_cast<int>(e->sprite().layer(2)->blendMode()));
    e->redo();
    ASSERT_EQ_INT(static_cast<int>(BlendMode::Multiply),
                  static_cast<int>(e->sprite().layer(2)->blendMode()));
    delete e;
}

TEST(an_unchanged_blend_mode_records_nothing)
{
    Editor *e = editor_with_layers(2);
    e->setSelectedLayersBlendMode(BlendMode::Normal);
    e->undo(); /* undoes the last layer that was added, not a blend step */
    ASSERT_EQ_INT(1, e->sprite().layerCount());
    delete e;
}

TEST(a_blend_mode_edit_is_held_off_while_stroking)
{
    Editor *e = editor_with_layers(2);
    e->selectTool(ToolKind::Pencil);
    e->press(1, 1);
    e->setSelectedLayersBlendMode(BlendMode::Screen);
    ASSERT_EQ_INT(static_cast<int>(BlendMode::Normal),
                  static_cast<int>(e->sprite().layer(1)->blendMode()));
    e->release();
    delete e;
}

TEST(move_up_and_down_shift_the_active_layer_by_one)
{
    Editor *e = editor_with_layers(3);
    e->setActiveLayer(1); /* "2" */
    ASSERT_TRUE(e->canMoveLayersUp());
    e->moveSelectedLayersBy(1);
    ASSERT_TRUE(layer_names(e) == "132");
    ASSERT_EQ_INT(2, e->activeLayer());
    ASSERT_TRUE(!e->canMoveLayersUp());
    e->moveSelectedLayersBy(1); /* already on top: nothing happens */
    ASSERT_TRUE(layer_names(e) == "132");
    e->moveSelectedLayersBy(-1);
    ASSERT_TRUE(layer_names(e) == "123");
    e->undo();
    ASSERT_TRUE(layer_names(e) == "132");
    delete e;
}

TEST(move_down_stops_at_the_bottom)
{
    Editor *e = editor_with_layers(2);
    e->setActiveLayer(0);
    ASSERT_TRUE(!e->canMoveLayersDown());
    e->moveSelectedLayersBy(-1);
    ASSERT_TRUE(layer_names(e) == "12");
    delete e;
}

TEST(moving_a_selection_moves_it_as_a_block_one_step_and_one_undo)
{
    Editor *e = editor_with_layers(4);
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(1, LayerPick::Toggle); /* "1" and "2" */
    ASSERT_TRUE(e->canMoveLayersUp());
    e->moveSelectedLayersBy(1);
    ASSERT_TRUE(layer_names(e) == "3124");
    ASSERT_TRUE(selection(e) == "12");
    e->moveSelectedLayersBy(1);
    ASSERT_TRUE(layer_names(e) == "3412");
    ASSERT_TRUE(!e->canMoveLayersUp());
    e->undo();
    ASSERT_TRUE(layer_names(e) == "3124");
    e->undo();
    ASSERT_TRUE(layer_names(e) == "1234");
    delete e;
}

TEST(a_selection_with_a_gap_moves_by_its_edge_layers)
{
    Editor *e = editor_with_layers(4);
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(2, LayerPick::Toggle); /* "1" and "3" */
    e->moveSelectedLayersBy(1);
    /* each selected layer swaps with the unselected one above it */
    ASSERT_TRUE(layer_names(e) == "2143");
    delete e;
}

TEST(moving_layers_is_held_off_while_stroking)
{
    Editor *e = editor_with_layers(3);
    e->setActiveLayer(0);
    e->selectTool(ToolKind::Pencil);
    e->press(1, 1);
    ASSERT_TRUE(!e->canMoveLayersUp());
    e->moveSelectedLayersBy(1);
    ASSERT_TRUE(layer_names(e) == "123");
    e->release();
    delete e;
}

/* The Layer menu and the Layers panel's Delete button ask the editor, so both agree. */
TEST(removing_layers_is_allowed_only_while_one_stays_and_not_while_stroking)
{
    Editor *e = editor_with_layers(3);
    ASSERT_TRUE(e->canRemoveSelectedLayers());
    e->pickLayer(0, LayerPick::Only);
    e->pickLayer(2, LayerPick::Range);
    ASSERT_EQ_INT(3, static_cast<int>(e->selectedLayers().size()));
    ASSERT_TRUE(!e->canRemoveSelectedLayers()); /* would leave no layer */
    e->pickLayer(1, LayerPick::Only);
    ASSERT_TRUE(e->canRemoveSelectedLayers());
    e->press(0, 0);
    ASSERT_TRUE(e->stroking());
    ASSERT_TRUE(!e->canRemoveSelectedLayers());
    e->release();
    delete e;
}

int main()
{
    if (!ui_init()) {
        return 1;
    }
    RUN(layer_add_remove_move_undo_redo);
    RUN(visibility_toggle_is_undoable);
    RUN(live_name_and_opacity_edits_commit_as_one_step);
    RUN(unchanged_live_edit_records_nothing);
    RUN(layer_edits_are_held_off_while_dragging);
    RUN(tools_ignore_a_hidden_layer);
    RUN(lock_toggle_is_undoable);
    RUN(tools_ignore_a_locked_layer);
    RUN(a_plain_pick_selects_only_that_layer);
    RUN(toggle_adds_and_removes_layers_and_active_follows);
    RUN(range_selects_everything_between_the_anchor_and_the_click);
    RUN(selection_follows_layers_when_the_stack_changes);
    RUN(removing_the_selected_layers_is_one_undo_step);
    RUN(the_last_layer_cannot_be_removed_with_a_selection);
    RUN(duplicating_the_selected_layers_selects_the_copies);
    RUN(a_selected_block_moves_together);
    RUN(a_selected_block_moves_down_below_the_target);
    RUN(dropping_a_block_on_itself_or_dragging_an_unselected_layer);
    RUN(visibility_and_lock_apply_to_the_whole_selection_in_one_step);
    RUN(merging_selected_layers_collects_them_into_the_lowest_one);
    RUN(merging_the_selection_is_one_undo_step);
    RUN(merge_is_refused_for_a_hidden_layer_or_a_locked_lowest_one);
    RUN(a_single_selected_layer_merges_down);
    RUN(a_new_document_starts_with_a_single_selected_layer);
    RUN(duplicate_layer_selects_the_copy);
    RUN(merge_layer_down_selects_the_lower_layer);
    RUN(merge_down_follows_a_higher_active_layer);
    RUN(duplicate_and_merge_are_held_off_while_dragging_or_invalid);
    RUN(merge_is_refused_when_a_lower_selected_layer_is_locked);
    RUN(merge_is_refused_when_a_blend_would_change_the_look);
    RUN(undo_and_redo_keep_the_same_layer_active);
    RUN(opacity_edits_apply_to_every_selected_layer_as_one_step);
    RUN(layers_keep_their_own_opacity_before_a_shared_edit);
    RUN(an_unchanged_shared_opacity_records_nothing);
    RUN(a_shared_opacity_edit_is_held_off_while_stroking);
    RUN(move_up_and_down_shift_the_active_layer_by_one);
    RUN(move_down_stops_at_the_bottom);
    RUN(moving_a_selection_moves_it_as_a_block_one_step_and_one_undo);
    RUN(a_selection_with_a_gap_moves_by_its_edge_layers);
    RUN(moving_layers_is_held_off_while_stroking);
    RUN(blend_mode_applies_to_every_selected_layer_as_one_step);
    RUN(an_unchanged_blend_mode_records_nothing);
    RUN(a_blend_mode_edit_is_held_off_while_stroking);
    RUN(removing_layers_is_allowed_only_while_one_stays_and_not_while_stroking);
    ui_quit();
    return TEST_SUMMARY();
}
