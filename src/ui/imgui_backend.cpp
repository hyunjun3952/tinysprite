#include "imgui_backend.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

extern "C" int imgui_backend_init(SDL_Window *window, SDL_Renderer *renderer)
{
    if (!ImGui_ImplSDL2_InitForSDLRenderer(window, renderer)) {
        return 0;
    }
    if (!ImGui_ImplSDLRenderer2_Init(renderer)) {
        ImGui_ImplSDL2_Shutdown();
        return 0;
    }
    return 1;
}

extern "C" void imgui_backend_shutdown(void)
{
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
}

extern "C" void imgui_backend_process_event(const SDL_Event *event)
{
    ImGui_ImplSDL2_ProcessEvent(event);
}

extern "C" void imgui_backend_new_frame(void)
{
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

extern "C" void imgui_backend_render(SDL_Renderer *renderer)
{
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
}
