#ifndef TINYSPRITE_UI_EDITOR_H
#define TINYSPRITE_UI_EDITOR_H

#include <SDL.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "tinysprite/composite.h"
#include "tinysprite/history.h"
#include "tinysprite/selection.h"
#include "tinysprite/sprite.h"
#include "tinysprite/tools.h"
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
    /* Returns null if the size is invalid or the texture cannot be created. */
    static std::unique_ptr<Editor> create(SDL_Renderer *renderer, int width, int height);
    ~Editor();

    /* Makes `sprite` the document: the undo history, selection and palette selection start over
     * and the view is fitted. Returns false (nothing changes) while a tool is being dragged or if
     * the texture for the new size cannot be created. */
    bool setSprite(Sprite sprite);
    Editor(const Editor &) = delete;
    Editor &operator=(const Editor &) = delete;

    Sprite &sprite()
    {
        return sprite_;
    }
    const Sprite &sprite() const
    {
        return sprite_;
    }
    History &history()
    {
        return history_;
    }
    const History &history() const
    {
        return history_;
    }
    SDL_Texture *texture() const
    {
        return texture_.get();
    }

    /* Tool options: pixel_perfect / brush_* apply to pencil and eraser, shape_filled to the
     * rectangle and ellipse (symmetry also reaches shapes and the bucket). Edited directly by the
     * tools panel and the shortcuts. */
    struct ToolOptions {
        int brush_size = 1; /* 1..PENCIL_BRUSH_MAX */
        bool brush_round = false;
        bool pixel_perfect = false;
        Symmetry symmetry = {};
        bool shape_filled = false;
    };

    /* The canvas view and the canvas window's size as of its last frame; the canvas owns the
     * logic that keeps them current. */
    struct ViewState {
        Viewport viewport = {};
        bool needs_fit = true; /* fit the sprite to the canvas on the next canvas frame */
        bool auto_fit = false; /* the view is still the fitted one: refit when the canvas resizes */
        float canvas_w = 0.0f;
        float canvas_h = 0.0f;
    };

    ToolKind tool() const
    {
        return tool_;
    }
    ToolOptions &options()
    {
        return options_;
    }
    const ToolOptions &options() const
    {
        return options_;
    }
    ViewState &view()
    {
        return view_;
    }
    const ViewState &view() const
    {
        return view_;
    }

    /* The foreground / background colors as straight-alpha RGBA floats in 0..1, for the color
     * pickers to edit in place. */
    float *foregroundFloats()
    {
        return color_;
    }
    float *backgroundFloats()
    {
        return bgColor_;
    }
    const float *foregroundFloats() const
    {
        return color_;
    }
    const float *backgroundFloats() const
    {
        return bgColor_;
    }

    /* Colors drawn with lately, newest first. Tools record a color when a stroke starts. */
    const RecentColors &recent() const
    {
        return recent_;
    }
    void recordRecent(Rgba color)
    {
        recent_.push(color);
    }

    /* Swatch of the sprite palette last picked or added, -1 for none. */
    int paletteSelected() const
    {
        return paletteSelected_;
    }

    bool showGrid() const
    {
        return showGrid_;
    }
    void setShowGrid(bool show)
    {
        showGrid_ = show;
    }

    int activeLayer() const
    {
        return activeLayer_;
    }
    /* Ignores an index with no layer. */
    void setActiveLayer(int index);
    int frame() const
    {
        return frame_;
    }

    /* The cel under the mouse, set by the canvas every frame; valid when hovering() is true. */
    void setHover(int x, int y, bool hovering);
    int hoverX() const
    {
        return hoverX_;
    }
    int hoverY() const
    {
        return hoverY_;
    }
    bool hovering() const
    {
        return hovering_;
    }

    /* Rectangular selection in cel coordinates, always inside the sprite. While a selection is
     * being moved it stays at its origin and moveDx/Dy hold the drag offset (zero otherwise). */
    bool hasSelection() const
    {
        return hasSelection_;
    }
    Rect selection() const
    {
        return selection_;
    }
    int moveDx() const
    {
        return moveDx_;
    }
    int moveDy() const
    {
        return moveDy_;
    }

    /* For the tool drags, which change the selection while stroking() is true: unlike
     * selectAll / clearSelection / setSelectionClipped these are not held off, and setSelection
     * does not clip. */
    void setSelection(Rect r);
    void dropSelection()
    {
        hasSelection_ = false;
    }
    void setMoveOffset(int dx, int dy);

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

    /* Records a name / opacity edit made live with Layer::set*: the layer currently holds the
     * edited value, `before*` is what it held when the edit began. One undo step; nothing if
     * unchanged. */
    void commitLayerName(int index, const std::string &beforeName);
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
    struct TextureDeleter {
        void operator()(SDL_Texture *texture) const
        {
            SDL_DestroyTexture(texture);
        }
    };

    Editor(SDL_Renderer *renderer, Sprite sprite);

    std::unique_ptr<SDL_Texture, TextureDeleter> newTexture(int width, int height) const;

    /* Starts over on the current sprite_ with `texture`: history, selection and view are reset. */
    void resetDocument(std::unique_ptr<SDL_Texture, TextureDeleter> texture);

    Rect spriteBounds() const;
    void afterHistoryStep();

    ToolKind tool_ = ToolKind::Pencil;
    float color_[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float bgColor_[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    RecentColors recent_ = {};
    int paletteSelected_ = -1;
    bool showGrid_ = true;
    int activeLayer_ = 0;
    int frame_ = 0;
    ToolOptions options_;
    ViewState view_;
    int hoverX_ = 0;
    int hoverY_ = 0;
    bool hovering_ = false;
    bool hasSelection_ = false;
    Rect selection_ = {};
    int moveDx_ = 0;
    int moveDy_ = 0;

    SDL_Renderer *renderer_;
    Sprite sprite_;
    History history_;
    std::unique_ptr<SDL_Texture, TextureDeleter> texture_;
    std::optional<Clip> clipboard_; /* last copied / cut selection */
    std::vector<Rgba> pixels_;      /* composited frame */
    bool dirty_ = false;
    Rect dirtyRect_ = {};

    bool tempPick_ = false; /* the eyedropper is active only while a modifier is held */
    ToolKind pickReturn_ = ToolKind::Pencil; /* the tool to go back to when the pick ends */

    /* The live drag between a press and its release, null when idle. Declared after the history
     * it works on, so it is gone first. */
    std::unique_ptr<Drag> drag_;
};

#endif
