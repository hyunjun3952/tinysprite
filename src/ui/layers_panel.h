#ifndef TINYSPRITE_UI_LAYERS_PANEL_H
#define TINYSPRITE_UI_LAYERS_PANEL_H

#include <cstdint>
#include <string>

#include "editor.h"
#include "layer_thumbnails.h"
#include "panels.h"

/* Layer list (top layer first): each row shows what is painted on the layer, with visibility and
 * lock toggles (double-click a row to rename it in place), plus the opacity of the active layer and
 * add / duplicate / delete / merge buttons. Click selects a layer, Ctrl+click adds or removes one,
 * Shift+click selects a range; duplicate, delete, drag and the toggles then act on all the selected
 * layers. Draws into the current ImGui window. */
class LayersPanel
{
  public:
    explicit LayersPanel(Editor &editor);

    void show(const Panels &panels);

    /* True while the Layers window has the keyboard focus, so a Delete key press means "delete the
     * layer" rather than "clear the selection". Only meaningful in the frame it was drawn. */
    bool focused() const;

    /* Opens the confirmation dialog for deleting the active layer; does nothing if it is the only
     * one. The dialog is drawn by show(). While it is open dialogOpen() is true. */
    void askToDelete();
    bool dialogOpen() const
    {
        return dialogOpen_;
    }

  private:
    /* What every row of the list shares, and the drop (if any) recorded while drawing it. */
    struct RowContext {
        float frameHeight;
        float rowHeight;
        bool multiple; /* more than one layer is selected */
    };
    struct Drop {
        int from = -1;
        int to = -1;
    };

    void rows();
    void row(int index, const RowContext &ctx, Drop &drop);
    void rowSelectable(int index, const RowContext &ctx);
    void rowDragDrop(int index, const Layer &layer, const RowContext &ctx, Drop &drop);
    void rowHintAndRename(int index, const Layer &layer);
    void nameCell(int index, Layer &layer, const RowContext &ctx);
    void visibilityToggle(int index, const Layer &layer, const RowContext &ctx);
    void lockToggle(int index, const Layer &layer, const RowContext &ctx);
    void renameField(int index, Layer &layer);
    void thumbnail(const Layer &layer, float rowHeight);
    void deleteDialog();
    void activeLayerDetails();
    void buttons();

    Editor &editor_;
    LayerThumbnails thumbs_;

    /* The row whose name is being edited in place (-1 for none), what the name was when the edit
     * began, and whether the field still has to grab the keyboard focus. */
    int renaming_ = -1;
    std::string renameBefore_;
    bool renameFocus_ = false;

    int focusFrame_ = -1;     /* the ImGui frame in which the window had the focus */
    bool openDelete_ = false; /* the dialog still has to be opened */
    bool dialogOpen_ = false;
    int deleteCount_ = 0; /* how many layers the dialog is about, and the active one's name */
    std::string deleteName_;

    /* The opacity is edited live and one undo step is recorded on release; this holds the value
     * from before that edit began. */
    uint8_t opacityBefore_ = 0;
};

#endif
