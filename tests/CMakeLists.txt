add_library(tinysprite_test_support INTERFACE)
target_include_directories(tinysprite_test_support INTERFACE ${PROJECT_SOURCE_DIR}/third_party)

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
endif()
