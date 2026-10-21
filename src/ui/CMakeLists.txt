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

add_library(tinysprite_ui STATIC
  editor.cpp drags.cpp canvas.cpp panels.cpp tools_panel.cpp layers_panel.cpp layer_thumbnails.cpp theme.cpp icons.cpp settings.cpp recovery.cpp app.cpp)
target_include_directories(tinysprite_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tinysprite_ui PUBLIC tinysprite_core tinysprite_io tinysprite_imgui)

add_executable(tinysprite main.cpp)
if(WIN32)
  # SDL.h renames main to SDL_main; SDL2main supplies the WinMain that calls it. It must come
  # before SDL2 on the link line, so it is listed ahead of tinysprite_ui.
  target_link_libraries(tinysprite PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(tinysprite PRIVATE tinysprite_ui)
if(WIN32)
  # Windows finds SDL2.dll next to the executable (tests and ui_smoke run from the build tree too).
  add_custom_command(TARGET tinysprite POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:tinysprite>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> ${CMAKE_BINARY_DIR}/tests)
endif()
