#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "app/imgui_backend.h"
#include "canvas/canvas.h"
#include "cimgui.h"
#include "editor/editor.h"
#include "editor/tools.h"
#include "icons/icons.h"
#include "panels/layers_panel.h"
#include "panels/palette_panel.h"
#include "panels/panels.h"

enum {
    DEFAULT_SPRITE_SIZE = 64,
    TOOLS_PER_ROW = 4,
    TOOL_BUTTON_SIZE = 40,
    SMALL_BUTTON_SIZE = 28,
    WINDOW_WIDTH = 1100,
    WINDOW_HEIGHT = 720,
};

typedef struct Options {
    int width, height;
    int max_frames; /* >0: exit after this many frames (smoke runs without a display) */
} Options;

static int parse_options(int argc, char **argv, Options *options)
{
    *options = (Options){DEFAULT_SPRITE_SIZE, DEFAULT_SPRITE_SIZE, 0};
    int numbers = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            options->max_frames = atoi(argv[++i]);
        } else if (numbers == 0) {
            options->width = atoi(argv[i]);
            options->height = options->width;
            numbers++;
        } else if (numbers == 1) {
            options->height = atoi(argv[i]);
            numbers++;
        } else {
            return -1;
        }
    }
    if (options->width < 1 || options->width > CEL_MAX_DIMENSION || options->height < 1 ||
        options->height > CEL_MAX_DIMENSION) {
        return -1;
    }
    return 0;
}

enum {
    COLOR_SWATCH = 36,
    COLOR_SWATCH_OFFSET = 18,
};

static ImVec4 color_vec(const float color[4])
{
    return (ImVec4){color[0], color[1], color[2], color[3]};
}

/* Foreground swatch drawn over the background one (click either to open its picker), with a swap
 * button beside them. */
static void color_widget(Editor *editor)
{
    ImGuiColorEditFlags button_flags =
        ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip;
    ImGuiColorEditFlags picker_flags = ImGuiColorEditFlags_PickerHueWheel |
                                       ImGuiColorEditFlags_AlphaBar |
                                       ImGuiColorEditFlags_DisplayHex;
    ImVec2 start = igGetCursorPos();
    ImVec2 size = {COLOR_SWATCH, COLOR_SWATCH};

    /* Background first so the foreground, drawn later, wins the overlap. */
    igSetCursorPos((ImVec2){start.x + COLOR_SWATCH_OFFSET, start.y + COLOR_SWATCH_OFFSET});
    if (igColorButton("##bg_swatch", color_vec(editor->bg_color), button_flags, size)) {
        igOpenPopup_Str("bg_picker", 0);
    }
    if (igIsItemHovered(0)) {
        igSetTooltip("Background color");
    }
    igSetCursorPos(start);
    if (igColorButton("##fg_swatch", color_vec(editor->color), button_flags, size)) {
        igOpenPopup_Str("fg_picker", 0);
    }
    if (igIsItemHovered(0)) {
        igSetTooltip("Foreground color");
    }

    igSetCursorPos((ImVec2){start.x + COLOR_SWATCH + COLOR_SWATCH_OFFSET + 12.0f, start.y});
    if (icon_button(ICON_SWAP, "##swap", "Swap colors (X)", 0, SMALL_BUTTON_SIZE)) {
        editor_swap_colors(editor);
    }
    igSetCursorPos((ImVec2){start.x, start.y + COLOR_SWATCH + COLOR_SWATCH_OFFSET + 6.0f});

    if (igBeginPopup("fg_picker", 0)) {
        igText("Foreground");
        igColorPicker4("##fg_picker", editor->color, picker_flags, NULL);
        igEndPopup();
    }
    if (igBeginPopup("bg_picker", 0)) {
        igText("Background");
        igColorPicker4("##bg_picker", editor->bg_color, picker_flags, NULL);
        igEndPopup();
    }
}

static const char *const TOOL_LABELS[TOOL_COUNT] = {
    "Pencil (B)", "Eraser (E)", "Bucket (G)",  "Eyedropper (I)",
    "Line (L)",   "Rect (R)",   "Ellipse (O)", "Select (M)",
};

static const IconKind TOOL_ICONS[TOOL_COUNT] = {
    ICON_PENCIL, ICON_ERASER, ICON_BUCKET,  ICON_EYEDROPPER,
    ICON_LINE,   ICON_RECT,   ICON_ELLIPSE, ICON_SELECT,
};

static void tools_panel(Editor *editor, float height)
{
    if (!panel_begin(PANEL_TOOLS, height)) {
        panel_end();
        return;
    }

    for (int tool = 0; tool < TOOL_COUNT; tool++) {
        if (tool % TOOLS_PER_ROW != 0) {
            igSameLine(0.0f, -1.0f);
        }
        igPushID_Int(tool);
        if (icon_button(TOOL_ICONS[tool], "##tool", TOOL_LABELS[tool],
                        editor->tool == (ToolKind)tool, TOOL_BUTTON_SIZE)) {
            editor_select_tool(editor, (ToolKind)tool);
        }
        igPopID();
    }

    igSeparatorText("Options");
    bool option = editor->pixel_perfect != 0;
    if (igCheckbox("Pixel-perfect", &option)) {
        editor->pixel_perfect = option;
    }
    option = editor->mirror_x != 0;
    if (igCheckbox("Mirror X", &option)) {
        editor->mirror_x = option;
    }
    igSameLine(125.0f, -1.0f);
    option = editor->mirror_y != 0;
    if (igCheckbox("Mirror Y", &option)) {
        editor->mirror_y = option;
    }
    igSliderInt("Brush", &editor->brush_size, 1, PENCIL_BRUSH_MAX, "%d px", 0);
    option = editor->brush_round != 0;
    if (igCheckbox("Round brush", &option)) {
        editor->brush_round = option;
    }
    option = editor->shape_filled != 0;
    if (igCheckbox("Filled shapes", &option)) {
        editor->shape_filled = option;
    }

    igSeparatorText("Selection");
    igBeginDisabled(!editor->has_selection || editor_stroking(editor));
    const float size = SMALL_BUTTON_SIZE;
    if (icon_button(ICON_FLIP_H, "##flip_h", "Flip horizontally", 0, size)) {
        editor_flip_selection(editor, 1);
    }
    igSameLine(0.0f, -1.0f);
    if (icon_button(ICON_FLIP_V, "##flip_v", "Flip vertically", 0, size)) {
        editor_flip_selection(editor, 0);
    }
    igSameLine(0.0f, -1.0f);
    if (icon_button(ICON_ROTATE_CW, "##rot_cw", "Rotate clockwise", 0, size)) {
        editor_rotate_selection(editor, 1);
    }
    igSameLine(0.0f, -1.0f);
    if (icon_button(ICON_ROTATE_CCW, "##rot_ccw", "Rotate counterclockwise", 0, size)) {
        editor_rotate_selection(editor, 0);
    }
    igEndDisabled();

    igSeparatorText("Color");
    color_widget(editor);

    palette_panel_show(editor);

    panel_end();
}

static void layers_window(Editor *editor, float height)
{
    if (!panel_begin(PANEL_LAYERS, height)) {
        panel_end();
        return;
    }
    layers_panel_show(editor);
    panel_end();
}

static void menu_bar(Editor *editor, int *running)
{
    if (!igBeginMainMenuBar()) {
        return;
    }
    if (igBeginMenu("File", true)) {
        /* Disabled until the .pxa reader / writer exists (src/io is still empty). */
        igMenuItem_Bool("Open...", "Ctrl+O", false, false);
        igMenuItem_Bool("Save", "Ctrl+S", false, false);
        igMenuItem_Bool("Export...", NULL, false, false);
        igSeparator();
        if (igMenuItem_Bool("Quit", NULL, false, true)) {
            *running = 0;
        }
        igEndMenu();
    }
    if (igBeginMenu("Edit", true)) {
        bool idle = !editor_stroking(editor);
        if (igMenuItem_Bool("Undo", "Ctrl+Z", false, idle && history_can_undo(editor->history))) {
            editor_undo(editor);
        }
        if (igMenuItem_Bool("Redo", "Ctrl+Y", false, idle && history_can_redo(editor->history))) {
            editor_redo(editor);
        }
        igSeparator();
        if (igMenuItem_Bool("Cut", "Ctrl+X", false, idle && editor->has_selection)) {
            editor_cut_selection(editor);
        }
        if (igMenuItem_Bool("Copy", "Ctrl+C", false, idle && editor->has_selection)) {
            editor_copy_selection(editor);
        }
        if (igMenuItem_Bool("Paste", "Ctrl+V", false, idle)) {
            editor_paste(editor);
        }
        if (igMenuItem_Bool("Select all", "Ctrl+A", false, idle)) {
            editor_select_all(editor);
        }
        igEndMenu();
    }
    if (igBeginMenu("View", true)) {
        if (igMenuItem_Bool("Zoom in", "+", false, true)) {
            editor_zoom_step(editor, 1);
        }
        if (igMenuItem_Bool("Zoom out", "-", false, true)) {
            editor_zoom_step(editor, -1);
        }
        if (igMenuItem_Bool("Fit", "F", false, true)) {
            editor->needs_fit = 1;
        }
        bool grid = editor->show_grid != 0;
        if (igMenuItem_Bool("Grid", NULL, grid, true)) {
            editor->show_grid = !grid;
        }
        igEndMenu();
    }
    panels_window_menu();
    igEndMainMenuBar();
}

typedef struct ToolKey {
    ImGuiKey key;
    ToolKind tool;
} ToolKey;

static const ToolKey TOOL_KEYS[] = {
    {ImGuiKey_B, TOOL_PENCIL},     {ImGuiKey_E, TOOL_ERASER}, {ImGuiKey_G, TOOL_BUCKET},
    {ImGuiKey_I, TOOL_EYEDROPPER}, {ImGuiKey_L, TOOL_LINE},   {ImGuiKey_R, TOOL_RECT},
    {ImGuiKey_O, TOOL_ELLIPSE},    {ImGuiKey_M, TOOL_SELECT},
};

/* Keys that act without a modifier: tool letters, X (swap colors), [ ] (brush size), F (fit) and
 * +/- (zoom). */
static void plain_shortcuts(Editor *editor)
{
    for (size_t i = 0; i < sizeof(TOOL_KEYS) / sizeof(TOOL_KEYS[0]); i++) {
        if (igIsKeyPressed_Bool(TOOL_KEYS[i].key, false)) {
            editor_select_tool(editor, TOOL_KEYS[i].tool);
        }
    }
    if (igIsKeyPressed_Bool(ImGuiKey_X, false)) {
        editor_swap_colors(editor);
    }
    if (igIsKeyPressed_Bool(ImGuiKey_LeftBracket, true) && editor->brush_size > 1) {
        editor->brush_size--;
    }
    if (igIsKeyPressed_Bool(ImGuiKey_RightBracket, true) && editor->brush_size < PENCIL_BRUSH_MAX) {
        editor->brush_size++;
    }
    if (igIsKeyPressed_Bool(ImGuiKey_F, false)) {
        editor->needs_fit = 1;
    }
    if (igIsKeyPressed_Bool(ImGuiKey_Equal, true) ||
        igIsKeyPressed_Bool(ImGuiKey_KeypadAdd, true)) {
        editor_zoom_step(editor, 1);
    }
    if (igIsKeyPressed_Bool(ImGuiKey_Minus, true) ||
        igIsKeyPressed_Bool(ImGuiKey_KeypadSubtract, true)) {
        editor_zoom_step(editor, -1);
    }
}

static void handle_shortcuts(Editor *editor)
{
    ImGuiIO *io = igGetIO_Nil();
    if (io->WantTextInput) {
        editor_temp_pick(editor, 0);
        return;
    }
    /* Holding Alt turns the active tool into the eyedropper until it is released. */
    editor_temp_pick(editor, io->KeyAlt);
    if (igIsKeyPressed_Bool(ImGuiKey_Escape, false)) {
        if (editor_stroking(editor)) {
            tools_cancel(editor);
        } else {
            editor_clear_selection(editor);
        }
    }
    if (editor_stroking(editor)) {
        return;
    }
    if (igIsKeyPressed_Bool(ImGuiKey_Delete, false)) {
        editor_delete_selection(editor);
    }
    if (!io->KeyCtrl) {
        if (!io->KeyAlt) {
            plain_shortcuts(editor);
        }
        return;
    }
    if (igIsKeyPressed_Bool(ImGuiKey_Z, true)) {
        if (io->KeyShift) {
            editor_redo(editor);
        } else {
            editor_undo(editor);
        }
    } else if (igIsKeyPressed_Bool(ImGuiKey_Y, true)) {
        editor_redo(editor);
    } else if (igIsKeyPressed_Bool(ImGuiKey_A, false)) {
        editor_select_all(editor);
    } else if (igIsKeyPressed_Bool(ImGuiKey_C, false)) {
        editor_copy_selection(editor);
    } else if (igIsKeyPressed_Bool(ImGuiKey_X, false)) {
        editor_cut_selection(editor);
    } else if (igIsKeyPressed_Bool(ImGuiKey_V, false)) {
        editor_paste(editor);
    }
}

int main(int argc, char **argv)
{
    Options options;
    panels_reset();
    if (parse_options(argc, argv, &options) != 0) {
        fprintf(stderr, "usage: %s [width [height]] [--frames N]\n", argv[0]);
        return 2;
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *window =
        SDL_CreateWindow("tinysprite", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
                         WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer =
        window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : NULL;
    if (window && !renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL window/renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    igCreateContext(NULL);
    igGetIO_Nil()->IniFilename = NULL;
    igStyleColorsDark(NULL);
    /* Unchecked boxes and sliders must stand out from the dark panel background. */
    ImGuiStyle *style = igGetStyle();
    style->FrameBorderSize = 1.0f;
    style->Colors[ImGuiCol_FrameBg] = (ImVec4){0.20f, 0.21f, 0.24f, 1.0f};
    style->Colors[ImGuiCol_Border] = (ImVec4){0.42f, 0.43f, 0.48f, 0.8f};
    int backend_ready = imgui_backend_init(window, renderer);
    if (backend_ready && !icons_init()) {
        fprintf(stderr, "icon font failed to load; buttons will have no icons\n");
    }
    Editor *editor = backend_ready ? editor_create(renderer, options.width, options.height) : NULL;
    if (!editor) {
        fprintf(stderr, "failed to initialise the editor\n");
        if (backend_ready) {
            imgui_backend_shutdown();
        }
        igDestroyContext(NULL);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int frames = 0;
    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            imgui_backend_process_event(&event);
            if (event.type == SDL_QUIT) {
                running = 0;
            }
        }

        imgui_backend_new_frame();
        ImGuiViewport *viewport;
        menu_bar(editor, &running);
        viewport = igGetMainViewport();
        float left, right;
        panels_layout(&left, &right);
        tools_panel(editor, viewport->WorkSize.y);
        layers_window(editor, viewport->WorkSize.y);
        canvas_show(editor, viewport->WorkPos.x + left, viewport->WorkPos.y,
                    viewport->WorkSize.x - left - right, viewport->WorkSize.y);
        handle_shortcuts(editor);

        SDL_SetRenderDrawColor(renderer, 30, 30, 32, 255);
        SDL_RenderClear(renderer);
        imgui_backend_render(renderer);
        SDL_RenderPresent(renderer);

        if (options.max_frames > 0 && ++frames >= options.max_frames) {
            running = 0;
        }
    }

    editor_destroy(editor);
    imgui_backend_shutdown();
    igDestroyContext(NULL);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
