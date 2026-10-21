#ifndef TINYSPRITE_UI_ICONS_H
#define TINYSPRITE_UI_ICONS_H

typedef enum IconKind {
    ICON_PENCIL,
    ICON_ERASER,
    ICON_BUCKET,
    ICON_EYEDROPPER,
    ICON_LINE,
    ICON_RECT,
    ICON_ELLIPSE,
    ICON_SELECT,
    ICON_FLIP_H,
    ICON_FLIP_V,
    ICON_ROTATE_CW,
    ICON_ROTATE_CCW,
    ICON_SWAP,
    ICON_ADD,
    ICON_REMOVE,
    ICON_DUPLICATE,
    ICON_MERGE_DOWN,
    ICON_MOVE_UP,
    ICON_MOVE_DOWN,
    ICON_POP_OUT,
} IconKind;

/* Loads the icon font into ImGui's atlas; call once after the ImGui context exists. Returns 0 on
 * failure (buttons then draw without an icon). */
int icons_init(void);

/* A square button drawn with a gray Material Symbols icon. `id` must be unique in
 * the current ImGui ID scope; `tooltip` is shown on hover, also while disabled. A selected button
 * gets an accent background. The icon dims with ImGui's disabled alpha. Returns non-zero when
 * clicked. Call between imgui_backend_new_frame and imgui_backend_render. */
int icon_button(IconKind icon, const char *id, const char *tooltip, int selected, float size);

/* A white icon-only button at the right end of the current window's title bar, `right_inset` pixels
 * left of the edge (room for the window's close button). Returns non-zero when clicked. Call right
 * after igBegin. */
int icon_title_button(IconKind icon, const char *tooltip, float right_inset);

#endif
