find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED) # built with the SDL3 and SDL3 renderer backends (vcpkg.json)

add_library(tinysprite_ui STATIC
  editor/editor.cpp editor/detail.cpp editor/layers.cpp editor/frames.cpp editor/colors.cpp editor/selection.cpp editor/preferences.cpp editor/slices.cpp editor/filters.cpp editor/tilemaps.cpp
  drag.cpp tool.cpp layer/layer_selection.cpp composite_surface.cpp command_table.cpp
  app/app.cpp app/menus.cpp app/commands.cpp app/shortcuts.cpp app/dialogs.cpp app/filter_dialog.cpp app/files.cpp app/file_browser_ui.cpp app/documents.cpp app/tabs.cpp app/split.cpp
  canvas.cpp statusbar.cpp file/file_paths.cpp file/file_browser.cpp panels.cpp tools_panel.cpp layers_panel.cpp slices_panel.cpp tilemap/tilemap_panel.cpp tilemap/tilemap_window.cpp cel_texture.cpp timeline_panel.cpp layer/layer_thumbnails.cpp theme.cpp icons.cpp settings.cpp recovery.cpp)
target_include_directories(tinysprite_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(tinysprite_ui PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/geist
                                                  ${PROJECT_SOURCE_DIR}/thirdparty/material-symbols
                                                  ${PROJECT_SOURCE_DIR}/thirdparty/noto-sans-kr)
target_link_libraries(tinysprite_ui PUBLIC tinysprite_core tinysprite_io imgui::imgui SDL3::SDL3)

add_executable(tinysprite main.cpp)
if(WIN32)
  set_target_properties(tinysprite PROPERTIES WIN32_EXECUTABLE ON) # no console window
endif()
target_link_libraries(tinysprite PRIVATE tinysprite_ui)
