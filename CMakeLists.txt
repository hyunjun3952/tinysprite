cmake_minimum_required(VERSION 3.16)
project(tinysprite CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TINYSPRITE_SANITIZE "Build with ASan/UBSan" ON)
# The dependencies (SDL3, Dear ImGui, stb) are declared in vcpkg.json: configure with
# -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake (see README).
# The UI is built whenever SDL3 is found; -DTINYSPRITE_BUILD_UI=OFF gives a core + io build.
find_package(SDL3 CONFIG QUIET)
option(TINYSPRITE_BUILD_UI "Build the SDL3 + ImGui frontend" ${SDL3_FOUND})
if(NOT TINYSPRITE_BUILD_UI)
  message(STATUS "tinysprite: UI not built (SDL3 not found, or TINYSPRITE_BUILD_UI=OFF)")
endif()

# stb_image / stb_image_write (header-only; vcpkg's "stb" or the system's libstb-dev).
find_path(STB_INCLUDE_DIR stb_image.h PATH_SUFFIXES stb)
if(NOT STB_INCLUDE_DIR)
  message(FATAL_ERROR "stb_image.h not found: configure with the vcpkg toolchain (see README) or install libstb-dev")
endif()

option(TINYSPRITE_WERROR "Treat compiler warnings as errors (CI)" OFF)

add_compile_options(-Wall -Wextra -Wpedantic)
if(TINYSPRITE_WERROR)
  # The stb implementation is built with -w, which still wins over this.
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
target_include_directories(tinysprite_io PRIVATE ${STB_INCLUDE_DIR})
target_link_libraries(tinysprite_io PUBLIC tinysprite_core)
# stb's implementation is third-party code: compile it without our warnings.
set_source_files_properties(src/io/stb_impl.cpp PROPERTIES COMPILE_OPTIONS -w)

if(TINYSPRITE_BUILD_UI)
  add_subdirectory(src/ui)
endif()

enable_testing()
add_subdirectory(tests)
