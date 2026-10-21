#ifndef TINYSPRITE_UI_ICONS_H
#define TINYSPRITE_UI_ICONS_H

namespace icons
{

enum class Icon {
    Pencil,
    Eraser,
    Bucket,
    Eyedropper,
    Line,
    Rect,
    Ellipse,
    Select,
    FlipH,
    FlipV,
    RotateCw,
    RotateCcw,
    Swap,
    Add,
    Remove,
    Duplicate,
    MergeDown,
    MoveUp,
    MoveDown,
    PopOut,
};

/* Loads the icon font into ImGui's atlas; call once after the ImGui context exists. Returns false
 * on failure (buttons then draw without an icon). */
bool init();

/* A square button drawn with a gray Material Symbols icon. `id` must be unique in
 * the current ImGui ID scope; `tooltip` is shown on hover, also while disabled. A selected button
 * gets an accent background. The icon dims with ImGui's disabled alpha. Returns true when
 * clicked. Call between the backends' new-frame calls and ImGui::Render. */
bool button(Icon icon, const char *id, const char *tooltip, bool selected, float size);

/* A white icon-only button at the right end of the current window's title bar, `right_inset` pixels
 * left of the edge (room for the window's close button). Returns true when clicked. Call right
 * after ImGui::Begin. */
bool titleButton(Icon icon, const char *tooltip, float right_inset);

} // namespace icons

#endif
