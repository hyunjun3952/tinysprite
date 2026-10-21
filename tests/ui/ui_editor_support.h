#ifndef TINYSPRITE_TESTS_UI_EDITOR_SUPPORT_H
#define TINYSPRITE_TESTS_UI_EDITOR_SUPPORT_H

/* What the ui_editor_* tests share: an SDL window and renderer on the dummy video driver (no
 * display), and small helpers that drive an Editor without ImGui frames. */
#include <SDL.h>

#include <cstdio>

#include "editor.h"

inline SDL_Window *window;
inline SDL_Renderer *renderer;

/* Returns false (after printing why) if SDL cannot be set up. */
inline bool ui_init()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::printf("SDL_Init: %s\n", SDL_GetError());
        return false;
    }
    window = SDL_CreateWindow("test", 0, 0, 64, 64, 0);
    renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE) : nullptr;
    if (!renderer) {
        std::printf("SDL renderer: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

inline void ui_quit()
{
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

inline Editor *make_editor(void)
{
    Editor *e = Editor::create(renderer, 8, 6).release();
    e->foregroundFloats()[0] = 1.0f;
    e->foregroundFloats()[3] = 1.0f; /* opaque red */
    return e;
}

inline int alpha(Editor *e, int x, int y)
{
    return e->activeCel()->get(x, y).a;
}

inline void click(Editor *e, int x, int y)
{
    e->press(x, y);
    e->release();
}

/* Draws a red pixel at each of the given cels with the pencil. */
inline void dot(Editor *e, int x, int y)
{
    e->selectTool(ToolKind::Pencil);
    click(e, x, y);
}

#endif
