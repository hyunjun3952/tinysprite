cmake_minimum_required(VERSION 3.16)
project(tinysprite C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TINYSPRITE_SANITIZE "Build with ASan/UBSan" ON)
option(TINYSPRITE_BUILD_UI "Build the SDL2 + ImGui frontend" OFF)

add_compile_options(-Wall -Wextra -Wpedantic)
if(TINYSPRITE_SANITIZE AND NOT MSVC)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# core: no SDL/ImGui dependency allowed here.
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS src/core/*.c)
add_library(tinysprite_core STATIC ${CORE_SOURCES})
target_include_directories(tinysprite_core PUBLIC include)
if(NOT MSVC)
  target_link_libraries(tinysprite_core PUBLIC m)
endif()

file(GLOB_RECURSE IO_SOURCES CONFIGURE_DEPENDS src/io/*.c)
if(IO_SOURCES)
  add_library(tinysprite_io STATIC ${IO_SOURCES})
  target_link_libraries(tinysprite_io PUBLIC tinysprite_core)
else()
  add_library(tinysprite_io INTERFACE)
  target_link_libraries(tinysprite_io INTERFACE tinysprite_core)
endif()

if(TINYSPRITE_BUILD_UI)
  add_subdirectory(src/ui)
endif()

enable_testing()
add_subdirectory(tests)
