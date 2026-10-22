#ifndef TINYSPRITE_UI_CANVAS_H
#define TINYSPRITE_UI_CANVAS_H

#include "editor.h"
#include "imgui.h"

/* The canvas window: draws the checkerboard, the composited sprite and the pixel grid, and turns
 * mouse input into zoom, pan and tool strokes. */
class Canvas
{
  public:
    explicit Canvas(Editor &editor);

    /* Call once per frame between the backends' new-frame calls and ImGui::Render. `x`, `y`, `w`,
     * `h` place the window. */
    void show(float x, float y, float w, float h);

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

    Editor &editor_;
};

#endif
