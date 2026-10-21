#ifndef TINYSPRITE_TESTS_UI_HARNESS_H
#define TINYSPRITE_TESTS_UI_HARNESS_H

/* A window, a renderer and an ImGui context with no display (SDL_VIDEODRIVER=dummy), driven by
 * synthetic input, for tests of the panels. */
#include <SDL.h>

#include <functional>

#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "panels.h"

namespace test_ui
{

constexpr float PANEL_HEIGHT = 600.0f;

struct Harness {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    Panels panels;
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    std::function<void()> layersBody; /* drawn inside the Layers panel, if set */
    std::function<void()> body;       /* drawn after the panels (the canvas), if set */
    int button = 0;                   /* which mouse button frame() presses: 0 left, 1 right */

    Harness()
    {
        SDL_Init(SDL_INIT_VIDEO);
        window = SDL_CreateWindow("test", 0, 0, 1000, 700, 0);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        /* Frames are slow under the sanitizers, so give the double click all the time it needs. */
        ImGui::GetIO().MouseSingleClickDelay = 20.0f;
        ImGui::GetIO().MouseDoubleClickTime = 10.0f;
        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);
    }
    ~Harness()
    {
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    /* One frame with the mouse at (x, y) and the left button held or not. */
    void frame(float x, float y, bool down)
    {
        mouseX = x;
        mouseY = y;
        ImGuiIO &io = ImGui::GetIO();
        io.AddMousePosEvent(x, y);
        io.AddMouseButtonEvent(button, down);
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        io.DisplaySize = ImVec2(1000.0f, 700.0f);
        ImGui::NewFrame();
        float left = 0.0f;
        float right = 0.0f;
        panels.layout(left, right);
        panels.begin(PanelId::Tools, PANEL_HEIGHT);
        panels.end();
        if (panels.begin(PanelId::Layers, PANEL_HEIGHT) && layersBody) {
            layersBody();
        }
        panels.end();
        if (body) {
            body();
        }
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    }

    /* Drags from (x0, y0) to (x1, y1) in small steps and releases there. */
    void drag(float x0, float y0, float x1, float y1)
    {
        frame(x0, y0, false);
        frame(x0, y0, true);
        for (int i = 1; i <= 10; i++) {
            float t = static_cast<float>(i) / 10.0f;
            frame(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, true);
        }
        frame(x1, y1, false);
        frame(x1, y1, false);
    }

    /* A key press and release as separate frames, the mouse left where it is. */
    void key(ImGuiKey key)
    {
        ImGui::GetIO().AddKeyEvent(key, true);
        frame(mouseX, mouseY, false);
        ImGui::GetIO().AddKeyEvent(key, false);
        frame(mouseX, mouseY, false);
    }

    /* The color last rendered at (x, y), as 0xRRGGBB. */
    unsigned pixel(int x, int y) const
    {
        SDL_Rect rect = {x, y, 1, 1};
        Uint32 value = 0;
        SDL_RenderReadPixels(renderer, &rect, SDL_PIXELFORMAT_ARGB8888, &value, sizeof(value));
        return value & 0xffffffu;
    }

    ImVec2 windowPos(const char *name) const
    {
        return ImGui::FindWindowByName(name)->Pos;
    }
};

} // namespace test_ui

#endif
