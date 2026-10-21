#ifndef TINYSPRITE_UI_EDITOR_H
#define TINYSPRITE_UI_EDITOR_H

#include <SDL.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "tinysprite/clip.h"
#include "tinysprite/composite.h"
#include "tinysprite/history.h"
#include "tinysprite/move.h"
#include "tinysprite/pencil.h"
#include "tinysprite/recent_colors.h"
#include "tinysprite/shape.h"
#include "tinysprite/sprite.h"
#include "tinysprite/viewport.h"

enum class ToolKind {
    Pencil,
    Eraser,
    Bucket,
    Eyedropper,
    Line,
    Rect,
    Ellipse,
    Select,
    Count,
};

inline constexpr int TOOL_COUNT = static_cast<int>(ToolKind::Count);

class Drag;

/* Everything the UI edits: the document, its undo history, the tool state and the CPU-composited
 * texture the canvas displays. Created once and passed to each panel. The tool and view state is
 * plain public data the panels and shortcuts edit directly; operations that touch the document go
 * through the methods, which hold off while a tool is being dragged where noted. */
class Editor
{
  public:
    /* Returns null on failure. */
    static std::unique_ptr<Editor> create(SDL_Renderer *renderer, int width, int height);
    ~Editor();
    Editor(const Editor &) = delete;
    Editor &operator=(const Editor &) = delete;

    Sprite *sprite() const
    {
        return sprite_.get();
    }
    History *history() const
    {
        return history_.get();
    }
    SDL_Texture *texture() const
    {
        return texture_.get();
    }

    ToolKind tool = ToolKind::Pencil;
    float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};    /* foreground color, straight alpha, 0..1 */
    float bg_color[4] = {1.0f, 1.0f, 1.0f, 1.0f}; /* background, swapped with the foreground */
    RecentColors recent = {};                     /* colors drawn with lately, newest first */
    int palette_selected = -1; /* swatch of the sprite palette last picked or added, -1 for none */
    bool show_grid = true;
    int active_layer = 0;
    int frame = 0;

    /* Tool options: pixel_perfect / brush_* apply to pencil and eraser, shape_filled to the
     * rectangle and ellipse (symmetry also reaches shapes and the bucket). */
    int brush_size = 1; /* 1..PENCIL_BRUSH_MAX */
    bool brush_round = false;
    bool pixel_perfect = false;
    Symmetry symmetry = {};
    bool shape_filled = false;

    Viewport view = {};
    bool needs_fit = true; /* fit the sprite to the canvas on the next canvas frame */
    bool auto_fit = false; /* the view is still the fitted one: refit when the canvas resizes */
    float canvas_w = 0.0f; /* canvas size as of the last canvas frame */
    float canvas_h = 0.0f;

    /* Rectangular selection in cel coordinates, always inside the sprite. While a selection is
     * being moved it stays at its origin and move_dx/dy hold the drag offset (zero otherwise). */
    bool has_selection = false;
    Rect selection = {};
    int move_dx = 0;
    int move_dy = 0;

    int hover_x = 0; /* cel under the mouse, valid when hovering is true */
    int hover_y = 0;
    bool hovering = false;

    Rgba foreground() const;
    void setForeground(Rgba color);

    /* Sprite palette helpers (the palette is not part of undo). Add puts the foreground color in
     * the palette unless it is already there and returns its index, or -1 if the palette is full.
     * Pick makes a swatch the foreground color. Remove deletes a swatch; the selection stays on
     * the same color, or is cleared when that color was removed. Out-of-range indexes are
     * ignored. */
    int paletteAdd();
    void palettePick(int index);
    void paletteRemove(int index);

    /* Exchanges the foreground and background colors. */
    void swapColors();

    /* Switches tools (does nothing while a tool is being dragged) and ends any temporary pick. */
    void selectTool(ToolKind kind);

    /* While `on` is true the eyedropper is the active tool, and the previous tool comes back once
     * it turns false. Meant to be called every frame with the state of the modifier key; does
     * nothing while a tool is being dragged, so a pick that ends mid-drag is undone on a later
     * call. */
    void tempPick(bool on);

    /* Mouse handling for the active tool, in cel coordinates (which may lie outside the sprite).
     * press starts a drag when the tool accepts it, drag is called every frame while the button
     * is held, release commits. Each one marks the cels it changes dirty. */
    void press(int cx, int cy);
    void drag(int cx, int cy);
    void release();

    /* Aborts the current drag, reverting its pixels. Does nothing when no drag is active. */
    void cancelDrag();

    /* True while a tool is being dragged (undo / redo / layer changes are held off meanwhile). */
    bool stroking() const
    {
        return drag_ != nullptr;
    }

    /* Marks cels for re-compositing; `area` must not be empty. */
    void markDirty(Rect area);
    void markAllDirty();
    bool dirty() const
    {
        return dirty_;
    }
    Rect dirtyRect() const
    {
        return dirtyRect_;
    }

    /* Re-composites the dirty region and uploads it to the texture. */
    void updateTexture();

    /* Zooms one step in (direction > 0) or out (< 0) around the canvas center. */
    void zoomStep(int direction);

    /* True when the mouse is over a cel of the sprite. */
    bool hoverInSprite() const;

    /* The cel of the active layer and frame; null if there is none. */
    Cel *activeCel();

    /* The active cel when a tool may change it; null while a tool is being dragged or when the
     * layer is hidden. */
    Cel *editableCel();

    /* Undo / redo; they do nothing while a stroke is being drawn. */
    void undo();
    void redo();

    /* Layer operations (all do nothing while a tool is being dragged). Each is one undo step. */
    void addLayer();
    void removeLayer(int index);
    void moveLayer(int from, int to);
    void setLayerVisible(int index, bool visible);

    /* Duplicate puts a copy above the layer and makes it active; merge draws the layer onto the
     * one below it (both must be visible). If the merged layer was active the one below becomes
     * active, otherwise the active layer keeps pointing at the same layer. */
    void duplicateLayer(int index);
    void mergeLayerDown(int index);

    /* Records a name / opacity edit made live with layer_set_*: the layer currently holds the
     * edited value, `before*` is what it held when the edit began. One undo step; nothing if
     * unchanged. */
    void commitLayerName(int index, const char *beforeName);
    void commitLayerOpacity(int index, uint8_t beforeOpacity);

    /* Selection helpers. Delete clears the selected pixels (one undo step). */
    void selectAll();
    void clearSelection();
    void deleteSelection();

    /* Makes `r`, clipped to the sprite, the selection; drops the selection if nothing is left. */
    void setSelectionClipped(Rect r);

    /* Copy / cut put the selected pixels of the active layer on the clipboard (cut also clears
     * them, one undo step). They return true if something was copied; cut needs a visible
     * layer. */
    bool copySelection();
    bool cutSelection();

    /* Pastes the clipboard as one undo step with its corner on the hovered cel, else on the
     * selection corner, else at (0, 0); the pasted area becomes the selection and the select tool
     * is chosen so it can be dragged into place. Does nothing without a clipboard or a visible
     * layer. */
    void paste();

    /* Flips (horizontally or else vertically) or turns (clockwise or else counter-clockwise) the
     * selected pixels in place, one undo step. A rotated selection keeps its center; whatever
     * lands outside the sprite is lost. */
    void flipSelection(bool horizontal);
    void rotateSelection(bool clockwise);

  private:
    template <auto Destroy> struct Deleter {
        template <class T> void operator()(T *object) const
        {
            Destroy(object);
        }
    };

    Editor() = default;

    Rect spriteBounds() const;
    void afterHistoryStep();

    std::unique_ptr<Sprite, Deleter<sprite_destroy>> sprite_;
    std::unique_ptr<History, Deleter<history_destroy>> history_;
    std::unique_ptr<SDL_Texture, Deleter<SDL_DestroyTexture>> texture_;
    std::unique_ptr<Clip, Deleter<clip_destroy>> clipboard_; /* last copied / cut selection */
    std::vector<Rgba> pixels_;                               /* composited frame */
    bool dirty_ = false;
    Rect dirtyRect_ = {};

    bool tempPick_ = false; /* the eyedropper is active only while a modifier is held */
    ToolKind pickReturn_ = ToolKind::Pencil; /* the tool to go back to when the pick ends */

    /* The live drag between a press and its release, null when idle. Declared after the history
     * it works on, so it is gone first. */
    std::unique_ptr<Drag> drag_;
};

#endif
