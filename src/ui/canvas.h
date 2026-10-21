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
    void drawGrid(ImDrawList *draw, ImVec2 origin, const ScreenRect &visible) const;
    void drawBrushPreview(ImDrawList *draw, ImVec2 origin) const;
    void drawHoverOutline(ImDrawList *draw, ImVec2 origin) const;
    void drawSelection(ImDrawList *draw, ImVec2 origin) const;
    void drawMirrorAxes(ImDrawList *draw, const ScreenRect &sprite) const;
    void drawStatus(ImDrawList *draw, ImVec2 origin, ImVec2 avail) const;

    Editor &editor_;
};

#endif
