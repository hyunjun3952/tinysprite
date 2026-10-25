#ifndef TINYSPRITE_UI_CANVAS_HPP
#define TINYSPRITE_UI_CANVAS_HPP

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "editor.hpp"
#include "imgui.h"

/* The canvas window: draws the checkerboard, the composited sprite and the pixel grid, and turns
 * mouse input into zoom, pan and tool strokes. With the rulers on it also lets a drag from a ruler
 * pull out a guide, and a drag on a guide move it (back onto the ruler deletes it). */
class Canvas
{
  public:
    /* `windowName` is the ImGui window of this canvas; two canvases (a split view) need different
     * ones. */
    explicit Canvas(Editor &editor, const char *windowName = "Canvas");

    /* Called when a mouse button goes down over the canvas, before the press is handled: a split
     * view uses it to make this pane's document the active one. */
    void setOnPress(std::function<void()> onPress)
    {
        onPress_ = std::move(onPress);
    }

    /* Marks the canvas as the active pane of a split view with a line along its top edge. */
    void setHighlighted(bool highlighted)
    {
        highlighted_ = highlighted;
    }

    /* Call once per frame between the backends' new-frame calls and ImGui::Render. `x`, `y`, `w`,
     * `h` place the window. */
    void show(float x, float y, float w, float h);

    /* Shows another document from now on (a tab was picked); what the panel remembers about the
     * old one is dropped. */
    void setEditor(Editor &editor);

  private:
    /* Screen-space rectangle (canvas window origin + view transform) of the whole sprite. */
    struct ScreenRect {
        float x0, y0, x1, y1;
    };

    /* A guide being dragged: which list it is in and where. */
    struct GuideDrag {
        bool vertical = false;
        int index = -1; /* -1: none */
    };

    void handleInput(ImVec2 origin, ImVec2 avail, bool hovered, bool active);
    float rulerSize() const;
    bool guideInput(ImVec2 origin, ImVec2 avail);
    bool pullGuide(bool vertical, ImVec2 origin);
    bool grabGuide(ImVec2 mouse, ImVec2 origin);
    GuideDrag pickGuide(ImVec2 mouse, ImVec2 origin) const;
    void beginGuide(bool vertical, int index, ImVec2 origin);
    void dragGuide(ImVec2 origin);
    void drawRulers(ImDrawList *draw, ImVec2 windowOrigin, ImVec2 origin, ImVec2 avail) const;
    void drawGuides(ImDrawList *draw, ImVec2 origin, ImVec2 avail) const;
    ImVec2 updateMiddle(ImVec2 mouse, bool hovered);
    bool applyPan(ImVec2 pan, bool active);
    void strokeInput(int cx, int cy, bool hovered);
    void updateFit(ImVec2 avail);
    void drawContent(ImVec2 origin, ImVec2 avail) const;
    void drawGrid(ImDrawList *draw, ImVec2 origin, const ScreenRect &visible) const;
    void drawBrushPreview(ImDrawList *draw, ImVec2 origin) const;
    void drawHoverOutline(ImDrawList *draw, ImVec2 origin) const;
    void drawSelection(ImDrawList *draw, ImVec2 origin) const;
    void refreshOutline(const std::shared_ptr<const Mask> &mask) const;
    void drawMaskOutline(ImDrawList *draw, ImVec2 origin,
                         const std::shared_ptr<const Mask> &mask) const;
    void drawLasso(ImDrawList *draw, ImVec2 origin) const;
    void drawMirrorAxes(ImDrawList *draw, const ScreenRect &sprite) const;
    void drawLoupe(ImVec2 origin, ImVec2 avail) const;

    /* The middle button does two things: held still it is the eyedropper, dragged it pans. The
     * choice is made by whichever comes first, a long press or movement. */
    enum class Middle { Idle, Pending, Panning, Picking };
    Middle middle_ = Middle::Idle;
    GuideDrag guideDrag_;
    ImVec2 middlePress_;
    double middleTime_ = 0.0;

    /* The outline of a mask kept in cel corners until another mask is shown: finding the sides
     * visits every pixel of it, too much to do again in every frame of a big selection. */
    static constexpr size_t MAX_OUTLINE_SIDES = 20000; /* more than this: show the bounding box */
    struct Side {
        int x0, y0, x1, y1;
    };
    mutable std::shared_ptr<const Mask> outlineOf_;
    mutable std::vector<Side> outline_;
    mutable bool outlineTooBig_ = false; /* so many sides that the bounding box is shown instead */

    Editor *editor_; /* the document shown; changed by setEditor */
    std::string windowName_;
    std::function<void()> onPress_;
    bool highlighted_ = false;
};

#endif
