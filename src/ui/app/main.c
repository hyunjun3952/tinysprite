#include <SDL.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/imgui_backend.h"
#include "canvas/canvas.h"
#include "editor/editor.h"
#include "editor/tools.h"
#include "icons/icons.h"
#include "panels/layers_panel.h"
#include "panels/palette_panel.h"
#include "panels/panels.h"
#include "tinysprite/util.h"
#include "ui_common.h"

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

/* Parses a whole-string decimal number within [min, max]. */
static bool parse_number(const char *text, int min, int max, int *out)
{
    char *end;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < min || value > max) {
        return false;
    }
    *out = (int)value;
    return true;
}

/* usage: [width [height]] [--frames N]; a lone width is also the height. */
static bool parse_options(int argc, char **argv, Options *options)
{
    *options = (Options){DEFAULT_SPRITE_SIZE, DEFAULT_SPRITE_SIZE, 0};
    int sizes[2];
    int size_count = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0) {
            i++;
            if (i >= argc || !parse_number(argv[i], 0, INT_MAX, &options->max_frames)) {
                return false;
            }
        } else if (size_count < 2 &&
                   parse_number(argv[i], 1, CEL_MAX_DIMENSION, &sizes[size_count])) {
            size_count++;
        } else {
            return false;
        }
    }
    if (size_count > 0) {
        options->width = sizes[0];
        options->height = sizes[size_count - 1];
    }
    return true;
}

enum {
    COLOR_SWATCH = 36,
    COLOR_SWATCH_OFFSET =
        18, /* how far the background swatch peeks out from behind the foreground */
    SWAP_BUTTON_GAP = 12,
    COLOR_SECTION_GAP = 6,
    MIRROR_Y_COLUMN = 125, /* x of the "Mirror Y" checkbox, beside "Mirror X" */
};

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
    if (igColorButton("##bg_swatch", imvec4_from_floats(editor->bg_color), button_flags, size)) {
        igOpenPopup_Str("bg_picker", 0);
    }
    if (igIsItemHovered(0)) {
        igSetTooltip("Background color");
    }
    igSetCursorPos(start);
    if (igColorButton("##fg_swatch", imvec4_from_floats(editor->color), button_flags, size)) {
        igOpenPopup_Str("fg_picker", 0);
    }
    if (igIsItemHovered(0)) {
        igSetTooltip("Foreground color");
    }

    igSetCursorPos(
        (ImVec2){start.x + COLOR_SWATCH + COLOR_SWATCH_OFFSET + SWAP_BUTTON_GAP, start.y});
    if (icon_button(ICON_SWAP, "##swap", "Swap colors (X)", false, SMALL_BUTTON_SIZE)) {
        editor_swap_colors(editor);
    }
    igSetCursorPos(
        (ImVec2){start.x, start.y + COLOR_SWATCH + COLOR_SWATCH_OFFSET + COLOR_SECTION_GAP});

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

/* Everything the UI shows for a tool, indexed by ToolKind. */
typedef struct ToolInfo {
    const char *label;
    IconKind icon;
    ImGuiKey key;
} ToolInfo;

static const ToolInfo TOOLS[TOOL_COUNT] = {
    [TOOL_PENCIL] = {"Pencil (B)", ICON_PENCIL, ImGuiKey_B},
    [TOOL_ERASER] = {"Eraser (E)", ICON_ERASER, ImGuiKey_E},
    [TOOL_BUCKET] = {"Bucket (G)", ICON_BUCKET, ImGuiKey_G},
    [TOOL_EYEDROPPER] = {"Eyedropper (I)", ICON_EYEDROPPER, ImGuiKey_I},
    [TOOL_LINE] = {"Line (L)", ICON_LINE, ImGuiKey_L},
    [TOOL_RECT] = {"Rect (R)", ICON_RECT, ImGuiKey_R},
    [TOOL_ELLIPSE] = {"Ellipse (O)", ICON_ELLIPSE, ImGuiKey_O},
    [TOOL_SELECT] = {"Select (M)", ICON_SELECT, ImGuiKey_M},
};

static void tools_panel(Editor *editor, Panels *panels, float height)
{
    if (!panel_begin(panels, PANEL_TOOLS, height)) {
        panel_end();
        return;
    }

    for (int tool = 0; tool < TOOL_COUNT; tool++) {
        if (tool % TOOLS_PER_ROW != 0) {
            igSameLine(0.0f, -1.0f);
        }
        igPushID_Int(tool);
        if (icon_button(TOOLS[tool].icon, "##tool", TOOLS[tool].label,
                        editor->tool == (ToolKind)tool, TOOL_BUTTON_SIZE)) {
            editor_select_tool(editor, (ToolKind)tool);
        }
        igPopID();
    }

    igSeparatorText("Options");
    igCheckbox("Pixel-perfect", &editor->pixel_perfect);
    igCheckbox("Mirror X", &editor->symmetry.mirror_x);
    igSameLine(MIRROR_Y_COLUMN, -1.0f);
    igCheckbox("Mirror Y", &editor->symmetry.mirror_y);
    igSliderInt("Brush", &editor->brush_size, 1, PENCIL_BRUSH_MAX, "%d px", 0);
    igCheckbox("Round brush", &editor->brush_round);
    igCheckbox("Filled shapes", &editor->shape_filled);

    igSeparatorText("Selection");
    igBeginDisabled(!editor->has_selection || editor_stroking(editor));
    const float size = SMALL_BUTTON_SIZE;
    if (icon_button(ICON_FLIP_H, "##flip_h", "Flip horizontally", false, size)) {
        editor_flip_selection(editor, true);
    }
    igSameLine(0.0f, -1.0f);
    if (icon_button(ICON_FLIP_V, "##flip_v", "Flip vertically", false, size)) {
        editor_flip_selection(editor, false);
    }
    igSameLine(0.0f, -1.0f);
    if (icon_button(ICON_ROTATE_CW, "##rot_cw", "Rotate clockwise", false, size)) {
        editor_rotate_selection(editor, true);
    }
    igSameLine(0.0f, -1.0f);
    if (icon_button(ICON_ROTATE_CCW, "##rot_ccw", "Rotate counterclockwise", false, size)) {
        editor_rotate_selection(editor, false);
    }
    igEndDisabled();

    igSeparatorText("Color");
    color_widget(editor);

    palette_panel_show(editor);

    panel_end();
}

static void layers_window(Editor *editor, Panels *panels, float height)
{
    if (!panel_begin(panels, PANEL_LAYERS, height)) {
        panel_end();
        return;
    }
    layers_panel_show(editor);
    panel_end();
}

static void menu_bar(Editor *editor, Panels *panels, bool *running)
{
    if (!igBeginMainMenuBar()) {
        return;
    }
    if (igBeginMenu("File", true)) {
        /* Disabled until the .pxa reader / writer exists (milestone 7, src/io is still empty). */
        igMenuItem_Bool("Open...", "Ctrl+O", false, false);
        igMenuItem_Bool("Save", "Ctrl+S", false, false);
        igMenuItem_Bool("Export...", NULL, false, false);
        igSeparator();
        if (igMenuItem_Bool("Quit", NULL, false, true)) {
            *running = false;
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
            editor->needs_fit = true;
        }
        igMenuItem_BoolPtr("Grid", NULL, &editor->show_grid, true);
        igEndMenu();
    }
    panels_window_menu(panels);
    igEndMainMenuBar();
}

/* Keys that act without a modifier: tool letters, X (swap colors), [ ] (brush size), F (fit) and
 * +/- (zoom). */
static void plain_shortcuts(Editor *editor)
{
    for (int tool = 0; tool < TOOL_COUNT; tool++) {
        if (igIsKeyPressed_Bool(TOOLS[tool].key, false)) {
            editor_select_tool(editor, (ToolKind)tool);
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
        editor->needs_fit = true;
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
        editor_temp_pick(editor, false);
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

/* Everything main sets up, so one function can tear down whatever a failed start got through. */
typedef struct App {
    SDL_Window *window;
    SDL_Renderer *renderer;
    bool imgui_context;
    bool backend;
    Panels *panels;
    Editor *editor;
} App;

static void app_destroy(App *app)
{
    editor_destroy(app->editor);
    panels_destroy(app->panels);
    if (app->backend) {
        imgui_backend_shutdown();
    }
    if (app->imgui_context) {
        igDestroyContext(NULL);
    }
    if (app->renderer) {
        SDL_DestroyRenderer(app->renderer);
    }
    if (app->window) {
        SDL_DestroyWindow(app->window);
    }
    SDL_Quit();
}

/* Prints the reason and returns false on failure; the caller still runs app_destroy. */
static bool app_init(App *app, const Options *options)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }
    app->window = SDL_CreateWindow("tinysprite", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
    if (app->window) {
        app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_ACCELERATED);
        if (!app->renderer) {
            app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_SOFTWARE);
        }
    }
    if (!app->renderer) {
        fprintf(stderr, "SDL window/renderer: %s\n", SDL_GetError());
        return false;
    }

    igCreateContext(NULL);
    app->imgui_context = true;
    igGetIO_Nil()->IniFilename = NULL;
    igStyleColorsDark(NULL);
    /* Unchecked boxes and sliders must stand out from the dark panel background. */
    ImGuiStyle *style = igGetStyle();
    style->FrameBorderSize = 1.0f;
    style->Colors[ImGuiCol_FrameBg] = (ImVec4){0.20f, 0.21f, 0.24f, 1.0f};
    style->Colors[ImGuiCol_Border] = (ImVec4){0.42f, 0.43f, 0.48f, 0.8f};
    app->backend = imgui_backend_init(app->window, app->renderer);
    if (app->backend && !icons_init()) {
        fprintf(stderr, "icon font failed to load; buttons will have no icons\n");
    }
    app->panels = panels_create();
    if (app->backend && app->panels) {
        app->editor = editor_create(app->renderer, options->width, options->height);
    }
    if (!app->editor) {
        fprintf(stderr, "failed to initialise the editor\n");
        return false;
    }
    return true;
}

static void run(App *app, const Options *options)
{
    Editor *editor = app->editor;
    int frames = 0;
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            imgui_backend_process_event(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        imgui_backend_new_frame();
        menu_bar(editor, app->panels, &running);
        ImGuiViewport *viewport = igGetMainViewport();
        float left, right;
        panels_layout(app->panels, &left, &right);
        tools_panel(editor, app->panels, viewport->WorkSize.y);
        layers_window(editor, app->panels, viewport->WorkSize.y);
        canvas_show(editor, viewport->WorkPos.x + left, viewport->WorkPos.y,
                    viewport->WorkSize.x - left - right, viewport->WorkSize.y);
        handle_shortcuts(editor);

        SDL_SetRenderDrawColor(app->renderer, 30, 30, 32, 255);
        SDL_RenderClear(app->renderer);
        imgui_backend_render(app->renderer);
        SDL_RenderPresent(app->renderer);

        if (options->max_frames > 0 && ++frames >= options->max_frames) {
            running = false;
        }
    }
}

int main(int argc, char **argv)
{
    Options options;
    if (!parse_options(argc, argv, &options)) {
        fprintf(stderr, "usage: %s [width [height]] [--frames N]\n", argv[0]);
        return 2;
    }
    App app = {0};
    int status = 1;
    if (app_init(&app, &options)) {
        run(&app, &options);
        status = 0;
    }
    app_destroy(&app);
    return status;
}
