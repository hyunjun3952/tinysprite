find_package(SDL2 REQUIRED)

# Dear ImGui + cimgui are vendored C++; the app code itself stays C. Warnings from vendored code
# are silenced.
set(CIMGUI_DIR ${PROJECT_SOURCE_DIR}/third_party/cimgui)
add_library(tinysprite_cimgui STATIC
  ${CIMGUI_DIR}/cimgui.cpp
  ${CIMGUI_DIR}/imgui/imgui.cpp
  ${CIMGUI_DIR}/imgui/imgui_demo.cpp
  ${CIMGUI_DIR}/imgui/imgui_draw.cpp
  ${CIMGUI_DIR}/imgui/imgui_tables.cpp
  ${CIMGUI_DIR}/imgui/imgui_widgets.cpp
  ${CIMGUI_DIR}/imgui/backends/imgui_impl_sdl2.cpp
  ${CIMGUI_DIR}/imgui/backends/imgui_impl_sdlrenderer2.cpp
  imgui_backend.cpp)
target_include_directories(tinysprite_cimgui
  PUBLIC ${CIMGUI_DIR}
  PRIVATE ${CIMGUI_DIR}/imgui ${CIMGUI_DIR}/imgui/backends)
target_link_libraries(tinysprite_cimgui PUBLIC SDL2::SDL2)
target_compile_options(tinysprite_cimgui PRIVATE -w)

add_library(tinysprite_ui STATIC editor.c canvas.c tools.c layers_panel.c palette_panel.c icons.c icon_font.c panels.c)
target_include_directories(tinysprite_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tinysprite_ui PUBLIC tinysprite_core tinysprite_io tinysprite_cimgui)

add_executable(tinysprite main.c)
target_link_libraries(tinysprite PRIVATE tinysprite_ui)
