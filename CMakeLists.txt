cmake_minimum_required(VERSION 3.16)
project(tinysprite CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TINYSPRITE_SANITIZE "Build with ASan/UBSan" ON)
# The UI is built whenever SDL2 is installed; -DTINYSPRITE_BUILD_UI=OFF gives a core-only build.
find_package(SDL2 QUIET)
option(TINYSPRITE_BUILD_UI "Build the SDL2 + ImGui frontend" ${SDL2_FOUND})
if(NOT TINYSPRITE_BUILD_UI)
  message(STATUS "tinysprite: UI not built (SDL2 not found, or TINYSPRITE_BUILD_UI=OFF)")
endif()

option(TINYSPRITE_WERROR "Treat compiler warnings as errors (CI)" OFF)

add_compile_options(-Wall -Wextra -Wpedantic)
if(TINYSPRITE_WERROR)
  # Vendored code (stb, ImGui) is built with -w, which still wins over this.
  add_compile_options(-Werror)
endif()
if(TINYSPRITE_SANITIZE AND NOT MSVC)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# core: no SDL/ImGui dependency allowed here.
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp)
add_library(tinysprite_core STATIC ${CORE_SOURCES})
target_include_directories(tinysprite_core PUBLIC include)
if(NOT MSVC)
  target_link_libraries(tinysprite_core PUBLIC m)
endif()

file(GLOB_RECURSE IO_SOURCES CONFIGURE_DEPENDS src/io/*.cpp)
add_library(tinysprite_io STATIC ${IO_SOURCES})
target_include_directories(tinysprite_io PRIVATE thirdparty/stb)
target_link_libraries(tinysprite_io PUBLIC tinysprite_core)
# stb is third-party code: compile its implementation without our warnings.
set_source_files_properties(src/io/stb_impl.cpp PROPERTIES COMPILE_OPTIONS -w)

if(TINYSPRITE_BUILD_UI)
  add_subdirectory(src/ui)
endif()

enable_testing()
add_subdirectory(tests)
