add_library(tinysprite_test_support INTERFACE)
target_include_directories(tinysprite_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
  # Tests are console programs with a plain main: keep SDL.h from renaming it, and undo the
  # -mwindows that SDL2::SDL2 adds (this library is linked last, so -mconsole wins).
  target_compile_definitions(tinysprite_test_support INTERFACE SDL_MAIN_HANDLED)
  target_link_libraries(tinysprite_test_support INTERFACE -mconsole)
endif()

# One executable per test file: tests/test_<name>.cpp -> ctest "<name>"
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)
foreach(src ${TEST_SOURCES})
  get_filename_component(base ${src} NAME_WE)
  string(REGEX REPLACE "^test_" "" name ${base})
  add_executable(${base} ${src})
  target_link_libraries(${base} PRIVATE tinysprite_io tinysprite_test_support)
  add_test(NAME ${name} COMMAND ${base})
endforeach()

# UI tests need SDL (dummy video driver, no display): ctest name "ui_editor".
if(TINYSPRITE_BUILD_UI)
  add_executable(test_ui_editor ui/ui_editor.cpp)
  target_link_libraries(test_ui_editor PRIVATE tinysprite_ui tinysprite_test_support)
  add_test(NAME ui_editor COMMAND test_ui_editor)
  set_tests_properties(ui_editor PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")

  # Panel docking driven by synthetic mouse input (ImGui needs a window and a renderer).
  add_executable(test_ui_panels ui/ui_panels.cpp)
  target_link_libraries(test_ui_panels PRIVATE tinysprite_ui tinysprite_test_support)
  add_test(NAME ui_panels COMMAND test_ui_panels)
  set_tests_properties(ui_panels PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")

  # The layer list with synthetic input (rename in place).
  add_executable(test_ui_layers_panel ui/ui_layers_panel.cpp)
  target_include_directories(test_ui_layers_panel PRIVATE ${PROJECT_SOURCE_DIR}/src/ui ${PROJECT_SOURCE_DIR}/thirdparty/imgui)
  target_link_libraries(test_ui_layers_panel PRIVATE tinysprite_ui tinysprite_test_support)
  add_test(NAME ui_layers_panel COMMAND test_ui_layers_panel)
  set_tests_properties(ui_layers_panel PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")

  # The per-layer thumbnail textures.
  add_executable(test_ui_layer_thumbnails ui/ui_layer_thumbnails.cpp)
  target_link_libraries(test_ui_layer_thumbnails PRIVATE tinysprite_ui tinysprite_test_support)
  add_test(NAME ui_layer_thumbnails COMMAND test_ui_layer_thumbnails)
  set_tests_properties(ui_layer_thumbnails PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")

  # The canvas with synthetic mouse input (panning).
  add_executable(test_ui_canvas ui/ui_canvas.cpp)
  target_include_directories(test_ui_canvas PRIVATE ${PROJECT_SOURCE_DIR}/src/ui ${PROJECT_SOURCE_DIR}/thirdparty/imgui)
  target_link_libraries(test_ui_canvas PRIVATE tinysprite_ui tinysprite_test_support)
  add_test(NAME ui_canvas COMMAND test_ui_canvas)
  set_tests_properties(ui_canvas PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")

  # Settings file, panel layout persistence and the autosave recovery copy (no window needed).
  add_executable(test_ui_settings ui/ui_settings.cpp)
  target_include_directories(test_ui_settings PRIVATE ${PROJECT_SOURCE_DIR}/src/ui ${PROJECT_SOURCE_DIR}/thirdparty/imgui)
  target_link_libraries(test_ui_settings PRIVATE tinysprite_ui tinysprite_test_support)
  add_test(NAME ui_settings COMMAND test_ui_settings)

  # The icon bitmaps are data: check their shape.
  add_executable(test_ui_icons ui/ui_icons.cpp)
  target_link_libraries(test_ui_icons PRIVATE tinysprite_ui tinysprite_test_support)
  add_test(NAME ui_icons COMMAND test_ui_icons)

  # Runs a few frames of the whole app (menus, panels, dialogs), so an ImGui assertion or a crash
  # while building the UI fails the build.
  add_test(NAME ui_smoke COMMAND tinysprite 16 --frames 3)
  set_tests_properties(ui_smoke PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")

  # A run with --config-dir leaves its settings behind on exit; the second test looks for them.
  set(PERSIST_DIR ${CMAKE_CURRENT_BINARY_DIR}/persist_config)
  add_test(NAME ui_persist_run
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${PERSIST_DIR})
  add_test(NAME ui_persist_write COMMAND tinysprite 16 --frames 3 --config-dir ${PERSIST_DIR})
  add_test(NAME ui_persist_check
    COMMAND ${CMAKE_COMMAND} -DFILE=${PERSIST_DIR}/settings.ini -P ${CMAKE_CURRENT_SOURCE_DIR}/check_file.cmake)
  set_tests_properties(ui_persist_run PROPERTIES FIXTURES_SETUP persist_clean)
  set_tests_properties(ui_persist_write PROPERTIES FIXTURES_REQUIRED persist_clean FIXTURES_SETUP persist_written ENVIRONMENT "SDL_VIDEODRIVER=dummy")
  set_tests_properties(ui_persist_check PROPERTIES FIXTURES_REQUIRED persist_written)
endif()
