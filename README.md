# tinysprite

[![ci](https://github.com/hyunjun3952/tinysprite/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/hyunjun3952/tinysprite/actions/workflows/ci.yml)
[![license](https://img.shields.io/github/license/hyunjun3952/tinysprite)](LICENSE.md)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![platforms](https://img.shields.io/badge/platform-linux%20%7C%20windows-lightgrey)

A small pixel art and sprite editor written in C++17, with SDL3 and Dear ImGui.

## Features

- Drawing tools: pencil, eraser, bucket fill, eyedropper, shapes, selection
- Layers with blend modes, undo / redo, several documents in tabs and a split view
- Animation: a timeline of frames with durations, playback, onion skin, tags (named frame ranges) and linked cels
- Import and export of `.pxa` (native), png, bmp, tga and jpg; animated GIF and sprite sheet export (with a JSON
  description, trimmed and packed layouts); animated GIF import; Aseprite (`.aseprite`) import
- Filters (outline, replace color, hue / brightness), slices with pivots, and tilemaps with a Tiled-style `.tmj` export
- A headless `tinysprite export` command for build scripts

## Build

You need CMake 3.16+, a C++17 compiler and [vcpkg](https://vcpkg.io). vcpkg provides SDL3, Dear ImGui and stb: they are
declared in [`vcpkg.json`](vcpkg.json), and nothing of them is kept in this repository.

```sh
git clone https://github.com/microsoft/vcpkg ~/vcpkg && ~/vcpkg/bootstrap-vcpkg.sh   # once
export VCPKG_ROOT=~/vcpkg
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
ctest --test-dir build --output-on-failure
./build/src/ui/tinysprite [file | width [height]]
```

`CMakePresets.json` offers the same steps as presets (`cmake --preset dev`, `cmake --build --preset dev`, `ctest --preset dev`; also `core` and
`release`), which is what an IDE such as CLion reads. They need `VCPKG_ROOT` set.

The first configure builds the libraries (a few minutes; vcpkg caches them). On Debian and Ubuntu, vcpkg needs
`sudo apt install autoconf autoconf-archive automake libtool libx11-dev libxft-dev libxext-dev libwayland-dev
libxkbcommon-dev libegl1-mesa-dev libibus-1.0-dev` for the window system and the input method. vcpkg's version of
each library is pinned by `builtin-baseline` in `vcpkg.json`.

The UI is built whenever SDL3 is found; `-DTINYSPRITE_BUILD_UI=OFF` builds only the core and io libraries, which need
just stb (vcpkg's, or `sudo apt install libstb-dev`). ASan/UBSan are on by default; `-DTINYSPRITE_SANITIZE=OFF` turns
them off.

## Contributing

[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) maps the code and [.rules](.rules) has every rule for changing it (tests,
code, lint, commits, branches). Format and lint run as git hooks (clang-format on commit, clang-tidy on push, both pinned
in `.pre-commit-config.yaml`):

```sh
pip install pre-commit && pre-commit install       # once per clone
pre-commit run --all-files --hook-stage manual    # everything, as CI does (needs build/compile_commands.json)
```

On Windows, work in WSL 2 (VS Code "WSL: Reopen Folder in WSL") and follow the Linux steps above; the window shows up
through WSLg. For clangd, point it at `build/compile_commands.json` (`--compile-commands-dir=build`).

## License

[MIT](LICENSE.md). The fonts in the repository and their licenses are listed in
[thirdparty/README.md](thirdparty/README.md).
