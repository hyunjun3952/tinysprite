#ifndef TINYSPRITE_UI_EDITOR_HPP
#define TINYSPRITE_UI_EDITOR_HPP

#include <SDL.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "composite_surface.hpp"
#include "layer_selection.hpp"
#include "settings.hpp"
#include "tinysprite/composite.hpp"
#include "tinysprite/history.hpp"
#include "tinysprite/playback.hpp"
#include "tinysprite/selection.hpp"
#include "tinysprite/sprite.hpp"
#include "tinysprite/tools.hpp"
#include "tinysprite/viewport.hpp"
#include "tool.hpp"

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
    Editor(Editor &&) = delete;
    Editor &operator=(Editor &&) = delete;

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
        return surface_->texture();
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

        /* The pencil / eraser's view of these options. */
        PencilOptions pencil() const
        {
            return {pixel_perfect, symmetry, brush_size, brush_round};
        }
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

    /* Which canvas decorations are drawn (the pixel grid is showGrid): the status line, the outline
     * of the cel under the mouse (with the brush footprint) and the mirror axes. Kept when a
     * document is opened. */
    struct Overlays {
        bool status = true;
        bool cursor = true;
        bool mirror_axes = true;
        bool timeline = true;                /* the timeline strip along the bottom of the window */
        float timeline_height = 246.0f;      /* at UI scale 1; set by dragging its top edge */
        bool options_bar = true;             /* the options of the active tool, above the canvas */
        unsigned status_items = 0x7FFFFFFFu; /* a bit per StatusItem (statusbar.hpp): which show */
    };
    Overlays &overlays()
    {
        return overlays_;
    }
    const Overlays &overlays() const
    {
        return overlays_;
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
        return layers_.active();
    }
    /* Makes `index` the only selected layer. Ignores an index with no layer. */
    void setActiveLayer(int index);

    /* The layer selection: the active layer (what the tools draw on) plus any layers picked with
     * Ctrl or Shift. Only picks the list gets; drawing always uses the active layer. */
    void pickLayer(int index, LayerPick mode);
    bool layerSelected(int index) const;

    /* Indexes of the selected layers, ascending; never empty. */
    std::vector<int> selectedLayers() const;
    int frame() const
    {
        return frame_;
    }

    /* Makes `frame` the one the canvas shows and the tools draw on, and stops playback. Returns
     * false (nothing changes) if it is out of range or a tool is being dragged. */
    bool setFrame(int frame);

    /* Frame operations (all do nothing while a tool is being dragged and stop playback). Each is
     * one undo step. Add puts a blank frame after the current one and duplicate a copy of it; both
     * select the new frame. Removing the last frame is refused; the selection stays on the same
     * frame, or moves to a neighbour when that one was removed. */
    void addFrame();
    void duplicateFrame();
    void removeFrame(int index);
    void moveFrame(int from, int to);
    void setFrameDuration(int index, int ms);

    /* Linked cels: linkCel makes frame `to` of the layer at `layer` show the same cel as `from`, so
     * drawing on either changes both; unlinkCel gives the frame its own copy again. Each is one
     * undo step; a locked layer and a tool being dragged hold them off. linkCelToPrevious links
     * the active layer's cel in the current frame to the frame before it. */
    void linkCel(int layer, int from, int to);
    void unlinkCel(int layer, int frame);
    void linkCelToPrevious();

    /* Tags: addTag makes one over the current frame and selects it; edit / remove are undoable. The
     * selected tag (-1 for none) is what playback runs over, in the tag's own direction, instead
     * of all the frames. */
    int selectedTag() const
    {
        return selectedTag_;
    }
    void selectTag(int index);
    void addTag();
    void editTag(int index, const Sprite::Tag &tag);
    void removeTag(int index);

    /* Playback runs over all the frames and loops; tick(elapsedMs) is called every UI frame with
     * the time that passed and moves the shown frame on while playing. Pressing a tool, editing
     * frames, undo and redo all stop it. */
    bool playing() const
    {
        return playback_.playing();
    }
    void togglePlayback();
    void stopPlayback();
    void tick(int elapsedMs);
    PlayMode playMode() const
    {
        return playMode_;
    }
    void setPlayMode(PlayMode mode);

    /* Onion skin: the neighbouring frames drawn faintly under the current one. The ghost counts
     * are limited to 0..ONION_MAX_FRAMES. It is hidden while the animation plays. */
    bool onionEnabled() const
    {
        return onionOn_;
    }
    const OnionSkin &onionSkin() const
    {
        return onion_;
    }
    void setOnion(bool enabled, OnionSkin skin);

    /* The color the canvas last showed at (x, y), ghosts included. */
    Rgba shownPixel(int x, int y) const
    {
        return surface_->pixelAt(x, y);
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
        return selection_.has_value();
    }
    Rect selection() const
    {
        return selection_.value_or(Rect{});
    }
    int moveDx() const
    {
        return moveDx_;
    }
    int moveDy() const
    {
        return moveDy_;
    }

    /* An undo / redo pair that puts the selection back to `before` / `after` (none = no selection).
     * Recorded with the pixel step of an edit that moves or reshapes the selection (move, paste,
     * rotate), so undoing it brings the selection back too. */
    HistoryAction selectionAction(std::optional<Rect> before, std::optional<Rect> after);

    /* `r` clipped to the sprite, or none if nothing is left. */
    std::optional<Rect> clippedSelection(Rect r) const;

    /* For the tool drags, which change the selection while stroking() is true: unlike
     * selectAll / clearSelection / setSelectionClipped these are not held off, and setSelection
     * does not clip. */
    void setSelection(Rect r);
    void dropSelection()
    {
        selection_.reset();
    }
    void setMoveOffset(int dx, int dy);

    Rgba foreground() const;
    void setForeground(Rgba rgba);

    /* Sprite palette helpers (each edit is one undo step). Add puts the foreground color in
     * the palette unless it is already there and returns its index, or -1 if the palette is full.
     * Pick makes a swatch the foreground color. Remove deletes a swatch; the selection stays on
     * the same color, or is cleared when that color was removed. Out-of-range indexes are
     * ignored. */
    int paletteAdd();
    /* Whether Add would do something: the foreground color is not in the palette yet and there is
     * room. */
    bool paletteCanAdd() const;
    /* Whether Replace would do something: a swatch is selected and the foreground color is not in
     * the palette (so the swatch changes and no duplicate appears). */
    bool paletteCanReplace() const;
    /* Gives the selected swatch the foreground color (one undo step). The selection stays. */
    bool paletteReplaceSelected();
    /* Whether the palette is the default one, so Reset would change nothing. */
    bool paletteIsDefault() const;

    /* The document has been saved: the history and the palette are clean again. */
    void markSaved();
    /* Counts the document as having unsaved changes although the history is at its save point. */
    void markModified()
    {
        modifiedOutsideHistory_ = true;
    }

    /* A blank document of the given size with the default palette, replacing the current one.
     * Returns false (nothing changes) for an invalid size or while a tool is being dragged. */
    bool newDocument(int width, int height);
    /* Replaces the sprite's palette (one undo step); the swatch selection is dropped. */
    void setPalette(Palette palette);
    /* True when the document counts as changed although the undo history is at its save point: one
     * that was recovered from an autosave (see markModified). */
    bool modifiedOutsideHistory() const
    {
        return modifiedOutsideHistory_;
    }
    void palettePick(int index);
    /* Replaces the palette with the default colors. */
    void paletteReset();
    void paletteRemove(int index);

    /* Makes the brush `delta` pixels bigger (smaller if negative), within 1..PENCIL_BRUSH_MAX. */
    void changeBrushSize(int delta);

    /* Exchanges the foreground and background colors. */
    void swapColors();

    /* Switches tools (does nothing while a tool is being dragged) and ends any temporary pick. */
    void selectTool(ToolKind kind);

    /* While `on` is true the eyedropper is the active tool, and the previous tool comes back once
     * it turns false. Meant to be called every frame with the state of the trigger (the held middle
     * button); does nothing while a tool is being dragged, so a pick that ends mid-drag is undone
     * on a later call. */
    void tempPick(bool on);

    /* Mouse handling for the active tool, in cel coordinates (which may lie outside the sprite).
     * press starts a drag when the tool accepts it, drag is called every frame while the button
     * is held, release commits. Shift held while dragging a shape keeps it square (a line snaps to
     * 45 degrees). Each one marks the cels it changes dirty. The drawing tools only change cels
     * inside the selection while there is one. */
    void press(int cx, int cy, bool shift = false);

    /* Shift+press with the pencil or eraser draws a line from the end of the previous pencil /
     * eraser stroke to the pressed cel, instead of a dot. lineFrom() is that start point, set
     * only while such a press is being handled; setLastDrawPoint() records where a stroke ended
     * (forgotten when the document is replaced). */
    std::optional<ToolPoint> lineFrom() const
    {
        return lineFrom_;
    }
    void setLastDrawPoint(int x, int y)
    {
        lastDrawPoint_ = ToolPoint{x, y};
    }
    void drag(int cx, int cy, bool shift = false);
    void release();

    /* Aborts the current drag, reverting its pixels. Does nothing when no drag is active. */
    void cancelDrag();

    /* True while a tool is being dragged (undo / redo / layer changes are held off meanwhile). */
    bool stroking() const
    {
        return drag_ != nullptr;
    }

    /* Counts the calls of markDirty / markAllDirty, i.e. every change of what the layers show, so
     * something derived from the pixels (the layer thumbnails) can tell when to refresh. */
    uint64_t revision() const
    {
        return revision_;
    }

    /* The renderer the editor was created with, for things that need textures of their own. */
    SDL_Renderer *renderer() const
    {
        return renderer_;
    }

    /* Marks cels for re-compositing; `area` must not be empty. */
    void markDirty(Rect area);
    void markAllDirty();
    bool dirty() const
    {
        return surface_->dirty();
    }
    Rect dirtyRect() const
    {
        return surface_->dirtyRect();
    }

    /* Re-composites the dirty region and uploads it to the texture. */
    void updateTexture();

    /* The preferences worth keeping between runs (grid, overlays, brush and shape options, colors,
     * tool) go to / come from the settings; invalid entries keep the current value. Symmetry is not
     * kept: it starts off every time. */
    void store(SettingsMap &settings) const;
    void load(const SettingsMap &settings);

    /* Zooms one step in (direction > 0) or out (< 0) around the canvas center. */
    void zoomStep(int direction);

    /* True when the mouse is over a cel of the sprite. */
    bool hoverInSprite() const;

    /* The cel of the active layer and frame; null if there is none. */
    Cel *activeCel();

    /* The active cel when a tool may change it; null while a tool is being dragged or when the
     * layer is hidden or locked. */
    Cel *editableCel();

    /* Undo / redo; they do nothing while a stroke is being drawn. */
    void undo();
    void redo();

    /* Layer operations (all do nothing while a tool is being dragged). Each is one undo step. */
    void addLayer();
    void removeLayer(int index);
    void moveLayer(int from, int to);
    void setLayerVisible(int index, bool visible);
    void setLayerLocked(int index, bool locked);

    /* The same for every selected layer, each as one undo step however many layers there are:
     * remove (refused if it would leave no layer; the layer under the removed ones becomes the
     * selection), duplicate (the copies become the selection), show / hide and lock / unlock. */
    void removeSelectedLayers();
    bool canRemoveSelectedLayers() const; /* false if it would leave no layer, or while stroking */
    void duplicateSelectedLayers();

    /* Merge draws the selected layers (bottom to top, with their opacities) onto the lowest of
     * them, which keeps its name and properties and becomes the only selection; a lone selected
     * layer is merged down. Unselected layers between them end up above the result. canMerge is
     * true when it would work: every selected layer is visible, the lowest is not locked, and a
     * lone layer has one below it. */
    bool canMergeLayers() const;
    void mergeSelectedLayers();
    void setSelectedLayersVisible(bool visible);
    void setSelectedLayersLocked(bool locked);

    /* Moves the layer at `from`, which was dragged onto the layer at `to`. If `from` is part of a
     * multi-layer selection the whole selection moves as a block, keeping its order, to just above
     * `to` (when dragged up the stack) or just below it (when dragged down); dropping onto a
     * selected layer does nothing. Otherwise it is moveLayer(from, to). */
    void moveSelectedLayers(int from, int to);

    /* The Up / Down buttons: every selected layer swaps with the unselected one above (direction
     * > 0) or below it, as one undo step; a selected block moves as a block. canMoveLayersUp /
     * Down tell whether that would change anything. */
    void moveSelectedLayersBy(int direction);
    bool canMoveLayersUp() const;
    bool canMoveLayersDown() const;

    /* Merge draws the layer onto the one below it (both must be visible). If the merged layer was
     * active the one below becomes active, otherwise the active layer keeps pointing at the same
     * layer. */
    void mergeLayerDown(int index);

    /* The opacity slider edits every selected layer: selectedLayerOpacities is what they hold when
     * the edit begins (layer index, opacity), setSelectedLayersOpacityLive gives them all the value
     * while the slider moves, and commitSelectedLayersOpacity records the whole edit as one undo
     * step once it is released (nothing if no layer changed). */
    std::vector<std::pair<int, uint8_t>> selectedLayerOpacities() const;
    void setSelectedLayersOpacityLive(uint8_t opacity);
    void commitSelectedLayersOpacity(const std::vector<std::pair<int, uint8_t>> &before);

    /* The blend mode combo changes every selected layer at once; one undo step, nothing for the
     * layers that already hold the mode. Held off while stroking. */
    void setSelectedLayersBlendMode(BlendMode mode);

    /* Records a name edit made live with Layer::setName: the layer currently holds the edited
     * value, `beforeName` is what it held when the edit began. One undo step; nothing if
     * unchanged. */
    void commitLayerName(int index, const std::string &beforeName);

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

    /* Moves the selected pixels and the selection by (dx, dy), one undo step (the arrow keys);
     * with no selection the whole cel moves. Returns false for a cel that cannot be edited, a
     * zero offset or an active stroke. */
    bool nudgeSelection(int dx, int dy);

  private:
    /* The editable selected pixels, copied, with the cel and area they came from; nullopt when
     * there is no selection or the cel cannot be edited. */
    struct SelectionEdit {
        Cel &cel;
        Rect rect;
        Clip clip;
    };
    std::optional<SelectionEdit> beginSelectionEdit();

    /* Runs `edit` on every selected layer as one undo step; false (nothing runs) while a tool is
     * being dragged. */
    bool editSelectedLayers(const std::function<void(Layer &)> &edit);

    Editor(SDL_Renderer *renderer, Sprite sprite);

    /* Starts over on the current sprite_ with `surface`: history, selection and view are reset. */
    void resetDocument(std::unique_ptr<CompositeSurface> surface);

    Rect spriteBounds() const;
    void reorderLayers(const std::vector<Layer *> &order);
    std::vector<int> shiftedLayerOrder(int direction) const;
    bool canMoveLayers(int direction) const;
    void startPlayback();
    void afterHistoryStep(const Layer *active = nullptr);

    ToolKind tool_ = ToolKind::Pencil;
    float color_[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float bgColor_[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    RecentColors recent_ = {};
    int paletteSelected_ = -1;
    bool modifiedOutsideHistory_ = false;
    bool showGrid_ = true;
    Overlays overlays_;
    LayerSelection layers_;
    int frame_ = 0;
    Playback playback_;
    PlayMode playMode_ = PlayMode::Forward;
    int selectedTag_ = -1;
    bool onionOn_ = false;
    OnionSkin onion_;
    ToolOptions options_;
    ViewState view_;
    int hoverX_ = 0;
    int hoverY_ = 0;
    bool hovering_ = false;
    std::optional<Rect> selection_;
    int moveDx_ = 0;
    int moveDy_ = 0;

    SDL_Renderer *renderer_;
    Sprite sprite_;
    History history_;
    std::unique_ptr<CompositeSurface> surface_;
    std::optional<Clip> clipboard_; /* last copied / cut selection */
    uint64_t revision_ = 0;

    std::optional<ToolPoint> lastDrawPoint_;
    std::optional<ToolPoint> lineFrom_;

    bool tempPick_ = false; /* the eyedropper is active only while the middle button is held */
    ToolKind pickReturn_ = ToolKind::Pencil; /* the tool to go back to when the pick ends */

    /* The live drag between a press and its release, null when idle. Declared after the history
     * it works on, so it is gone first. */
    std::unique_ptr<Drag> drag_;
};

#endif
