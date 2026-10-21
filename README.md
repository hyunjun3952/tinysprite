# tinysprite

[![ci](https://github.com/hyunjun3952/tinysprite/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/hyunjun3952/tinysprite/actions/workflows/ci.yml)
[![license](https://img.shields.io/github/license/hyunjun3952/tinysprite)](LICENSE)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![platforms](https://img.shields.io/badge/platform-linux%20%7C%20windows-lightgrey)

A small pixel art / 2D sprite animation editor written in C++17, with a UI built on SDL2 and Dear ImGui.

## Features

- Drawing tools: pencil, eraser, bucket fill, eyedropper, shapes, selection
- Layers with undo / redo
- Import and export of `.pxa` (native), png, bmp, tga and jpg

## Building

Requirements: CMake 3.16+ and a C++17 compiler. The UI also needs SDL2 (`sudo apt install libsdl2-dev`).

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/src/ui/tinysprite [file | width [height]]
```

The UI is built whenever SDL2 is installed; `-DTINYSPRITE_BUILD_UI=OFF` gives a core-only build. ASan/UBSan are on by
default; `-DTINYSPRITE_SANITIZE=OFF` turns them off.

## Development

Development is test-first; [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) maps the code. Format and lint before committing:

```sh
scripts/setup-hooks.sh   # once per clone: git hooks run the checks
scripts/format.sh        # --check to verify only
scripts/lint.sh
```

A devcontainer (`.devcontainer/`) and VS Code settings (`.vscode/`) are included. Commits follow
[Conventional Commits](https://www.conventionalcommits.org/), e.g. `feat(core): add flood fill`.

## License

[MIT](LICENSE). Third-party components and their licenses are listed in [thirdparty/README.md](thirdparty/README.md).
