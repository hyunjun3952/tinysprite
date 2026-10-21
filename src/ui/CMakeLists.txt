find_package(SDL2 REQUIRED)

# Dear ImGui is vendored (third_party/imgui). Warnings from vendored code are silenced.
set(IMGUI_DIR ${PROJECT_SOURCE_DIR}/third_party/imgui)
add_library(tinysprite_imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp)
target_include_directories(tinysprite_imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(tinysprite_imgui PUBLIC SDL2::SDL2)
target_compile_options(tinysprite_imgui PRIVATE -w)

# The UI is C++ on top of the C core.
add_library(tinysprite_ui STATIC
  editor/editor.cpp editor/tools.cpp
  canvas/canvas.cpp
  panels/panels.cpp panels/tools_panel.cpp panels/layers_panel.cpp panels/palette_panel.cpp
  icons/icons.cpp icons/icon_font.cpp
  app/app.cpp)
target_include_directories(tinysprite_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tinysprite_ui PUBLIC tinysprite_core tinysprite_io tinysprite_imgui)

add_executable(tinysprite app/main.cpp)
target_link_libraries(tinysprite PRIVATE tinysprite_ui)
