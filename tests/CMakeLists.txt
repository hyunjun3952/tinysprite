add_library(tinysprite_test_support INTERFACE)
target_include_directories(tinysprite_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
  # Tests are console programs with a plain main: keep SDL.h from renaming it, and undo the
  # -mwindows that SDL2::SDL2 adds (this library is linked last, so -mconsole wins).
  target_compile_definitions(tinysprite_test_support INTERFACE SDL_MAIN_HANDLED)
  target_link_libraries(tinysprite_test_support INTERFACE -mconsole)
endif()

# One executable per test file: tests/test_<name>.cpp -> ctest "<name>". A group of four or more files
# with one feature prefix lives in a folder of that name (tests/tilemap/test_tilemap_*.cpp); the folder
# does not change the ctest name, so test file names stay unique across the tree.
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)
foreach(src ${TEST_SOURCES})
  get_filename_component(base ${src} NAME_WE)
  string(REGEX REPLACE "^test_" "" name ${base})
  add_executable(${base} ${src})
  target_include_directories(${base} PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/stb)
  target_link_libraries(${base} PRIVATE tinysprite_io tinysprite_test_support)
  add_test(NAME ${name} COMMAND ${base})
endforeach()

# UI tests: tests/ui/ui_<name>.cpp (or tests/ui/<group>/ui_<name>.cpp) -> ctest "ui_<name>". They need SDL,
# on the dummy video driver (no display).
if(TINYSPRITE_BUILD_UI)
  file(GLOB_RECURSE UI_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui_*.cpp)
  foreach(src ${UI_TEST_SOURCES})
    get_filename_component(base ${src} NAME_WE)
    add_executable(test_${base} ${src})
    target_include_directories(test_${base} PRIVATE ${PROJECT_SOURCE_DIR}/src/ui ${PROJECT_SOURCE_DIR}/thirdparty/imgui ${CMAKE_CURRENT_SOURCE_DIR}/ui)
    target_link_libraries(test_${base} PRIVATE tinysprite_ui tinysprite_test_support)
    add_test(NAME ${base} COMMAND test_${base})
    set_tests_properties(${base} PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
  endforeach()

  # Runs a few frames of the whole app (menus, panels, dialogs), so an ImGui assertion or a crash
  # while building the UI fails the build.
  add_test(NAME ui_smoke COMMAND tinysprite 16 --frames 3)
  set_tests_properties(ui_smoke PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")

  # --screenshot writes the last frame as an image (the extension picks the format).
  set(SHOT ${CMAKE_CURRENT_BINARY_DIR}/ui_screenshot.png)
  add_test(NAME ui_screenshot_run COMMAND ${CMAKE_COMMAND} -E remove ${SHOT})
  add_test(NAME ui_screenshot_write COMMAND tinysprite 16 --frames 3 --screenshot ${SHOT})
  add_test(NAME ui_screenshot_check
    COMMAND ${CMAKE_COMMAND} -DFILE=${SHOT} -DPNG=ON -P ${CMAKE_CURRENT_SOURCE_DIR}/check_file.cmake)
  set_tests_properties(ui_screenshot_run PROPERTIES FIXTURES_SETUP shot_clean)
  set_tests_properties(ui_screenshot_write PROPERTIES FIXTURES_REQUIRED shot_clean FIXTURES_SETUP shot_written ENVIRONMENT "SDL_VIDEODRIVER=dummy")
  set_tests_properties(ui_screenshot_check PROPERTIES FIXTURES_REQUIRED shot_written)

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
