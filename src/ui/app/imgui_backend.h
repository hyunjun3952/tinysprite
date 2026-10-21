#ifndef TINYSPRITE_UI_IMGUI_BACKEND_H
#define TINYSPRITE_UI_IMGUI_BACKEND_H

#include <SDL.h>

/* Thin C-callable wrapper over the Dear ImGui SDL2 platform + SDL_Renderer backends, which are
 * C++ only and not covered by cimgui. Implemented in imgui_backend.cpp; the rest of the UI stays
 * pure C. Call between igCreateContext and igDestroyContext. */
#ifdef __cplusplus
extern "C" {
#endif

/* Returns 1 on success, 0 on failure. */
int imgui_backend_init(SDL_Window *window, SDL_Renderer *renderer);
void imgui_backend_shutdown(void);

void imgui_backend_process_event(const SDL_Event *event);

/* Starts a frame: backend new-frame calls followed by igNewFrame. */
void imgui_backend_new_frame(void);

/* Finishes the frame with igRender and draws it through the SDL renderer. */
void imgui_backend_render(SDL_Renderer *renderer);

#ifdef __cplusplus
}
#endif

#endif
