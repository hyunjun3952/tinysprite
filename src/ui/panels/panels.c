#include "panels/panels.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "icons/icons.h"

enum {
    TOOLS_PANEL_WIDTH = 250,
    LAYERS_PANEL_WIDTH = 220,
    FLOAT_OFFSET = 40,
    RESIZE_GRAB = 4,
    PANEL_MIN_WIDTH = 170,
    PANEL_MAX_WIDTH = 480,
    DOCK_ZONE = 60, /* mouse distance from a window edge that docks a dragged panel */
};

/* A side panel is docked to the left or right edge (taking space from the canvas, side by side with
 * the other panels on that edge) or floating (a movable, resizable, closable window). Dragging a
 * floating panel's title bar to an edge docks it there. Hidden panels give their space away. */
/* Translucent blue, ABGR. */
#define COL32_PANEL_DROP 0x66d29a4aU

typedef enum DockSide { DOCK_LEFT, DOCK_RIGHT, DOCK_FLOAT } DockSide;

typedef struct Panel {
    const char *title;
    float width;
    int visible;
    DockSide side;
    DockSide home; /* where "dock" puts the panel back */
    int place;     /* set when a panel just started floating: put it near where it was docked */
    int title_press;
    int resizing;
    float dock_x; /* left edge while docked, assigned by panels_layout */
} Panel;

static Panel panels[PANEL_COUNT];

void panels_reset(void)
{
    panels[PANEL_TOOLS] =
        (Panel){"Tools", TOOLS_PANEL_WIDTH, 1, DOCK_LEFT, DOCK_LEFT, 0, 0, 0, 0.0f};
    panels[PANEL_LAYERS] =
        (Panel){"Layers", LAYERS_PANEL_WIDTH, 1, DOCK_RIGHT, DOCK_RIGHT, 0, 0, 0, 0.0f};
}

static void panel_dock(Panel *panel, DockSide side)
{
    panel->side = side;
    panel->visible = 1;
    panel->place = side == DOCK_FLOAT;
    if (side != DOCK_FLOAT) {
        panel->home = side;
    }
}

/* Assigns the docked panels their x position and returns the width the canvas loses on each side.
 */
void panels_layout(float *left, float *right)
{
    ImGuiViewport *viewport = igGetMainViewport();
    float right_edge = viewport->WorkPos.x + viewport->WorkSize.x;
    *left = 0.0f;
    *right = 0.0f;
    for (int i = 0; i < PANEL_COUNT; i++) {
        Panel *panel = &panels[i];
        if (panel->visible && panel->side == DOCK_LEFT) {
            panel->dock_x = viewport->WorkPos.x + *left;
            *left += panel->width;
        }
    }
    /* Right-docked panels keep the same order, so walk them from the outer edge inwards. */
    for (int i = PANEL_COUNT - 1; i >= 0; i--) {
        Panel *panel = &panels[i];
        if (panel->visible && panel->side == DOCK_RIGHT) {
            *right += panel->width;
            panel->dock_x = right_edge - *right;
        }
    }
}

/* While a floating panel's title bar is dragged near a window edge, highlights where it would dock
 * and docks it on release. */
static void panel_drag_to_dock(Panel *panel)
{
    ImGuiViewport *viewport = igGetMainViewport();
    ImVec2 mouse = igGetMousePos();
    ImVec2 pos = igGetWindowPos();
    if (igIsMouseClicked_Bool(ImGuiMouseButton_Left, false)) {
        panel->title_press = igIsWindowHovered(0) && mouse.y >= pos.y &&
                             mouse.y < pos.y + igGetFrameHeight() &&
                             mouse.x < pos.x + igGetWindowWidth();
    }
    if (!panel->title_press) {
        return;
    }
    float left_edge = viewport->WorkPos.x;
    float right_edge = left_edge + viewport->WorkSize.x;
    int side = -1;
    if (mouse.x < left_edge + DOCK_ZONE) {
        side = DOCK_LEFT;
    } else if (mouse.x > right_edge - DOCK_ZONE) {
        side = DOCK_RIGHT;
    }
    if (igIsMouseReleased_Nil(ImGuiMouseButton_Left)) {
        panel->title_press = 0;
        if (side >= 0) {
            panel_dock(panel, (DockSide)side);
        }
    } else if (side >= 0 && igIsMouseDragging(ImGuiMouseButton_Left, 4.0f)) {
        float x0 = side == DOCK_LEFT ? left_edge : right_edge - panel->width;
        ImDrawList *draw = igGetForegroundDrawList_Nil();
        ImDrawList_AddRectFilled(
            draw, (ImVec2){x0, viewport->WorkPos.y},
            (ImVec2){x0 + panel->width, viewport->WorkPos.y + viewport->WorkSize.y},
            COL32_PANEL_DROP, 0.0f, 0);
    }
}

/* Drag the inner edge of a docked panel to change its width. */
static void panel_resize_edge(Panel *panel)
{
    ImGuiViewport *viewport = igGetMainViewport();
    ImVec2 mouse = igGetMousePos();
    float edge = panel->side == DOCK_LEFT ? panel->dock_x + panel->width : panel->dock_x;
    int inside =
        mouse.y >= viewport->WorkPos.y && mouse.y < viewport->WorkPos.y + viewport->WorkSize.y;
    /* The grab strip lies inside the panel: outside it the canvas would take the click. */
    float grab_x0 = panel->side == DOCK_LEFT ? edge - RESIZE_GRAB : edge;
    int hovered = inside && mouse.x >= grab_x0 && mouse.x <= grab_x0 + RESIZE_GRAB;
    if (hovered && !igIsAnyItemActive()) {
        igSetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (igIsMouseClicked_Bool(ImGuiMouseButton_Left, false)) {
            panel->resizing = 1;
        }
    }
    if (panel->resizing && !igIsMouseDown_Nil(ImGuiMouseButton_Left)) {
        panel->resizing = 0;
    }
    if (panel->resizing) {
        igSetMouseCursor(ImGuiMouseCursor_ResizeEW);
        float width = panel->side == DOCK_LEFT ? mouse.x - panel->dock_x
                                               : panel->dock_x + panel->width - mouse.x;
        panel->width = width < PANEL_MIN_WIDTH ? PANEL_MIN_WIDTH : width;
        if (panel->width > PANEL_MAX_WIDTH) {
            panel->width = PANEL_MAX_WIDTH;
        }
    }
    if (hovered || panel->resizing) {
        ImDrawList *draw = igGetForegroundDrawList_Nil();
        ImDrawList_AddRectFilled(draw, (ImVec2){edge - 1.0f, viewport->WorkPos.y},
                                 (ImVec2){edge + 1.0f, viewport->WorkPos.y + viewport->WorkSize.y},
                                 COL32_PANEL_DROP, 0.0f, 0);
    }
}

/* Pop-out / dock icon at the right end of the title bar (left of the close button when floating).
 */
static void panel_title_button(Panel *panel)
{
    int floating = panel->side == DOCK_FLOAT;
    float close_button = floating ? igGetFrameHeight() : 0.0f;
    if (icon_title_button(ICON_POP_OUT, floating ? "Dock panel" : "Float panel", close_button)) {
        panel_dock(panel, floating ? panel->home : DOCK_FLOAT);
    }
}

/* Starts the panel's window; the caller draws the content when this returns non-zero and always
 * calls panel_end. */
int panel_begin(PanelId id, float height)
{
    Panel *panel = &panels[id];
    ImGuiViewport *viewport = igGetMainViewport();
    if (!panel->visible) {
        return 0;
    }
    if (panel->side != DOCK_FLOAT) {
        igSetNextWindowPos((ImVec2){panel->dock_x, viewport->WorkPos.y}, ImGuiCond_Always,
                           (ImVec2){0.0f, 0.0f});
        igSetNextWindowSize((ImVec2){panel->width, height}, ImGuiCond_Always);
        int shown = igBegin(panel->title, NULL,
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                ImGuiWindowFlags_NoCollapse);
        panel_title_button(panel);
        panel_resize_edge(panel);
        return shown;
    }
    if (panel->place) {
        igSetNextWindowPos(
            (ImVec2){panel->dock_x + FLOAT_OFFSET, viewport->WorkPos.y + FLOAT_OFFSET},
            ImGuiCond_Always, (ImVec2){0.0f, 0.0f});
        panel->place = 0;
    }
    igSetNextWindowSize((ImVec2){panel->width, height * 0.8f}, ImGuiCond_FirstUseEver);
    bool open = true;
    int shown = igBegin(panel->title, &open, 0);
    panel_title_button(panel);
    if (!open) {
        panel->visible = 0;
    }
    panel_drag_to_dock(panel);
    return shown;
}

void panel_end(void)
{
    igEnd();
}

void panels_window_menu(void)
{
    if (igBeginMenu("Window", true)) {
        for (int i = 0; i < PANEL_COUNT; i++) {
            Panel *panel = &panels[i];
            if (igBeginMenu(panel->title, true)) {
                bool shown = panel->visible != 0;
                if (igMenuItem_BoolPtr("Show", NULL, &shown, true)) {
                    panel->visible = shown;
                }
                igSeparator();
                if (igMenuItem_Bool("Dock left", NULL, panel->side == DOCK_LEFT, true)) {
                    panel_dock(panel, DOCK_LEFT);
                }
                if (igMenuItem_Bool("Dock right", NULL, panel->side == DOCK_RIGHT, true)) {
                    panel_dock(panel, DOCK_RIGHT);
                }
                if (igMenuItem_Bool("Float", NULL, panel->side == DOCK_FLOAT, true)) {
                    panel_dock(panel, DOCK_FLOAT);
                }
                igEndMenu();
            }
        }
        igSeparator();
        if (igMenuItem_Bool("Reset layout", NULL, false, true)) {
            panels_reset();
        }
        igEndMenu();
    }
}
