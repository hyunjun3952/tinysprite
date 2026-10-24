# tinysprite

[![ci](https://github.com/hyunjun3952/tinysprite/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/hyunjun3952/tinysprite/actions/workflows/ci.yml)
[![license](https://img.shields.io/github/license/hyunjun3952/tinysprite)](LICENSE.md)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![platforms](https://img.shields.io/badge/platform-linux%20%7C%20windows-lightgrey)

A small pixel art and sprite editor written in C++17, with SDL2 and Dear ImGui.

## Features

- Drawing tools: pencil, eraser, bucket fill, eyedropper, shapes, selection
- Layers with undo / redo
- Animation: a timeline of frames with durations, playback, onion skin, tags (named frame ranges) and
  linked cels
- Import and export of `.pxa` (native), png, bmp, tga and jpg; animated GIF and sprite sheet export (with a
  JSON description, trimmed and packed layouts); animated GIF import; Aseprite (`.aseprite`) import
- Filters (outline, replace color, hue / brightness), slices with pivots, and tilemaps with a Tiled-style
  `.tmj` export
- A headless `tinysprite export` command for build scripts

## Build

Requirements: CMake 3.16+ and a C++17 compiler. The UI also needs SDL2 (`sudo apt install libsdl2-dev`).

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/src/ui/tinysprite [file | width [height]]
```

The UI is built whenever SDL2 is installed; `-DTINYSPRITE_BUILD_UI=OFF` gives a core-only build. ASan/UBSan are on by
default; `-DTINYSPRITE_SANITIZE=OFF` turns them off.

## Contributing

[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) maps the code and [.rules](.rules) has every rule for changing it (tests, code, lint, commits, branches). Format and lint run as git hooks (clang-format on commit, clang-tidy on push, both pinned in `.pre-commit-config.yaml`):

```sh
pip install pre-commit && pre-commit install       # once per clone
cmake -S . -B build                                # clang-tidy reads build/compile_commands.json
pre-commit run --all-files --hook-stage manual    # everything, as CI does
```

On Windows, work in WSL 2 (VS Code "WSL: Reopen Folder in WSL"): run `sudo apt install libsdl2-dev pipx` once,
then build, test, format and lint there. The UI window shows up from WSL (WSLg). For clangd, point it at
`build/compile_commands.json` (`--compile-commands-dir=build`).

## License

[MIT](LICENSE.md). Third-party components and their licenses are listed in [thirdparty/README.md](thirdparty/README.md).
