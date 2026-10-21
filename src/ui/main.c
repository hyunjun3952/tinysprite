#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "canvas.h"
#include "cimgui.h"
#include "editor.h"
#include "imgui_backend.h"

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

    if (igRadioButton_Bool("Pencil", editor->tool == TOOL_PENCIL)) {
        editor->tool = TOOL_PENCIL;
    }
    igSameLine(0.0f, -1.0f);
    if (igRadioButton_Bool("Eraser", editor->tool == TOOL_ERASER)) {
        editor->tool = TOOL_ERASER;
    }

    igSeparatorText("Color");
    igColorEdit4("##color", editor->color,
                 ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_AlphaBar);

    igSeparatorText("View");
    igText("Zoom: %gx", (double)editor->view.zoom);
    if (igButton("-", (ImVec2){0.0f, 0.0f})) {
        editor->view.zoom = viewport_next_zoom(editor->view.zoom, -1);
    }
    igSameLine(0.0f, -1.0f);
    if (igButton("+", (ImVec2){0.0f, 0.0f})) {
        editor->view.zoom = viewport_next_zoom(editor->view.zoom, 1);
    }
    igSameLine(0.0f, -1.0f);
    if (igButton("Fit", (ImVec2){0.0f, 0.0f})) {
        editor->needs_fit = 1;
    }
    bool grid = editor->show_grid != 0;
    if (igCheckbox("Grid", &grid)) {
        editor->show_grid = grid;
    }
    if (editor->hovering && editor->hover_x >= 0 && editor->hover_y >= 0 &&
        editor->hover_x < sprite_width(editor->sprite) &&
        editor->hover_y < sprite_height(editor->sprite)) {
        igText("Cel: %d, %d", editor->hover_x, editor->hover_y);
    } else {
        igText("Cel: -");
    }

    igSeparatorText("History");
    igBeginDisabled(!history_can_undo(editor->history) || editor->stroke != NULL);
    if (igButton("Undo", (ImVec2){0.0f, 0.0f})) {
        editor_undo(editor);
    }
    igEndDisabled();
    igSameLine(0.0f, -1.0f);
    igBeginDisabled(!history_can_redo(editor->history) || editor->stroke != NULL);
    if (igButton("Redo", (ImVec2){0.0f, 0.0f})) {
        editor_redo(editor);
    }
    igEndDisabled();

    igEnd();
}

static void handle_shortcuts(Editor *editor)
{
    ImGuiIO *io = igGetIO_Nil();
    if (io->WantTextInput || !io->KeyCtrl || editor->stroke) {
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
    Editor *editor = imgui_backend_init(window, renderer)
                         ? editor_create(renderer, options.width, options.height)
                         : NULL;
    if (!editor) {
        fprintf(stderr, "failed to initialise the editor\n");
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
