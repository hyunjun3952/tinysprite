#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "canvas.h"
#include "cimgui.h"
#include "editor.h"
#include "imgui_backend.h"
#include "layers_panel.h"
#include "palette_panel.h"
#include "tools.h"

enum {
    DEFAULT_SPRITE_SIZE = 64,
    TOOLS_PANEL_WIDTH = 220,
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

static void tools_panel(Editor *editor, float height)
{
    ImGuiViewport *viewport = igGetMainViewport();
    igSetNextWindowPos(viewport->WorkPos, ImGuiCond_Always, (ImVec2){0.0f, 0.0f});
    igSetNextWindowSize((ImVec2){TOOLS_PANEL_WIDTH, height}, ImGuiCond_Always);
    igBegin("Tools", NULL,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    for (int tool = 0; tool < TOOL_COUNT; tool++) {
        if (tool % 2 != 0) {
            igSameLine(110.0f, -1.0f);
        }
        if (igRadioButton_Bool(tools_name((ToolKind)tool), editor->tool == (ToolKind)tool)) {
            editor_select_tool(editor, (ToolKind)tool);
        }
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
    igSameLine(110.0f, -1.0f);
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
    if (igButton("Flip H", (ImVec2){0.0f, 0.0f})) {
        editor_flip_selection(editor, 1);
    }
    igSameLine(0.0f, -1.0f);
    if (igButton("Flip V", (ImVec2){0.0f, 0.0f})) {
        editor_flip_selection(editor, 0);
    }
    igSameLine(0.0f, -1.0f);
    if (igButton("Rot CW", (ImVec2){0.0f, 0.0f})) {
        editor_rotate_selection(editor, 1);
    }
    igSameLine(0.0f, -1.0f);
    if (igButton("CCW", (ImVec2){0.0f, 0.0f})) {
        editor_rotate_selection(editor, 0);
    }
    igEndDisabled();

    igSeparatorText("Color");
    igColorEdit4("##color", editor->color,
                 ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_AlphaBar);
    igColorEdit4("##bg_color", editor->bg_color,
                 ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
    igSameLine(0.0f, -1.0f);
    if (igButton("Swap (X)", (ImVec2){0.0f, 0.0f})) {
        editor_swap_colors(editor);
    }

    palette_panel_show(editor);

    igSeparatorText("View");
    if (igButton("-", (ImVec2){0.0f, 0.0f})) {
        editor_zoom_step(editor, -1);
    }
    igSameLine(0.0f, -1.0f);
    if (igButton("+", (ImVec2){0.0f, 0.0f})) {
        editor_zoom_step(editor, 1);
    }
    igSameLine(0.0f, -1.0f);
    if (igButton("Fit", (ImVec2){0.0f, 0.0f})) {
        editor->needs_fit = 1;
    }
    bool grid = editor->show_grid != 0;
    if (igCheckbox("Grid", &grid)) {
        editor->show_grid = grid;
    }

    igSeparatorText("History");
    igBeginDisabled(!history_can_undo(editor->history) || editor_stroking(editor));
    if (igButton("Undo", (ImVec2){0.0f, 0.0f})) {
        editor_undo(editor);
    }
    igEndDisabled();
    igSameLine(0.0f, -1.0f);
    igBeginDisabled(!history_can_redo(editor->history) || editor_stroking(editor));
    if (igButton("Redo", (ImVec2){0.0f, 0.0f})) {
        editor_redo(editor);
    }
    igEndDisabled();

    layers_panel_show(editor);

    igEnd();
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
    int backend_ready = imgui_backend_init(window, renderer);
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
        ImGuiViewport *viewport = igGetMainViewport();
        float panel_width = TOOLS_PANEL_WIDTH;
        tools_panel(editor, viewport->WorkSize.y);
        canvas_show(editor, viewport->WorkPos.x + panel_width, viewport->WorkPos.y,
                    viewport->WorkSize.x - panel_width, viewport->WorkSize.y);
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
