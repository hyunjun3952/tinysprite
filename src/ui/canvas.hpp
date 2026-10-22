#ifndef TINYSPRITE_UI_CANVAS_HPP
#define TINYSPRITE_UI_CANVAS_HPP

#include <functional>
#include <string>
#include <utility>

#include "editor.hpp"
#include "imgui.h"

/* The canvas window: draws the checkerboard, the composited sprite and the pixel grid, and turns
 * mouse input into zoom, pan and tool strokes. */
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

    void handleInput(ImVec2 origin, bool hovered, bool active);
    ImVec2 updateMiddle(ImVec2 mouse, bool hovered);
    bool applyPan(ImVec2 pan, bool active);
    void strokeInput(int cx, int cy, bool hovered);
    void updateFit(ImVec2 avail);
    void drawContent(ImVec2 origin, ImVec2 avail) const;
    void drawGrid(ImDrawList *draw, ImVec2 origin, const ScreenRect &visible) const;
    void drawBrushPreview(ImDrawList *draw, ImVec2 origin) const;
    void drawHoverOutline(ImDrawList *draw, ImVec2 origin) const;
    void drawSelection(ImDrawList *draw, ImVec2 origin) const;
    void drawMirrorAxes(ImDrawList *draw, const ScreenRect &sprite) const;
    void drawLoupe(ImVec2 origin, ImVec2 avail) const;

    /* The middle button does two things: held still it is the eyedropper, dragged it pans. The
     * choice is made by whichever comes first, a long press or movement. */
    enum class Middle { Idle, Pending, Panning, Picking };
    Middle middle_ = Middle::Idle;
    ImVec2 middlePress_;
    double middleTime_ = 0.0;

    Editor *editor_; /* the document shown; changed by setEditor */
    std::string windowName_;
    std::function<void()> onPress_;
    bool highlighted_ = false;
};

#endif
