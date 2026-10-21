# tinysprite

[![ci](https://github.com/hyunjun3952/tinysprite/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/hyunjun3952/tinysprite/actions/workflows/ci.yml)
[![license](https://img.shields.io/github/license/hyunjun3952/tinysprite)](LICENSE)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![platforms](https://img.shields.io/badge/platform-linux%20%7C%20windows-lightgrey)

A small pixel art / 2D sprite animation editor written in C++17, with a UI built on SDL2 and Dear ImGui.

> **Status:** early development. Phase 1 (drawing tools, layers, file I/O) is done; animation and game-pipeline
> features are planned. See [docs/MILESTONES.md](docs/MILESTONES.md).

## Features

- Tools: pencil, eraser, bucket fill, eyedropper, line / rectangle / ellipse and other shapes, rectangular selection with move, copy / paste, flip and rotate
- Pencil options: brush size and shape, pixel-perfect, mirror X / Y
- Layers: add, remove, duplicate, merge, reorder, rename, visibility, lock, opacity
- Undo / redo (one step per stroke, including layer operations)
- Files: native `.pxa`; import and export of png / bmp / tga / jpg; `.gpl` and `.hex` palettes
- Autosave recovery, unsaved-changes prompt, settings kept between runs

Planned: timeline and onion skin, GIF / spritesheet export, tilemaps, JSON export, headless CLI.

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

## Using it

Press **F1** in the app for the list of keyboard shortcuts. Preferences are kept in `settings.ini` in
`$XDG_CONFIG_HOME/tinysprite` or `~/.config/tinysprite` (`%APPDATA%\tinysprite` on Windows); `--config-dir DIR` or
`TINYSPRITE_CONFIG_DIR` picks another directory. A document with unsaved changes is autosaved there, and after a crash
the next start offers to recover it.

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
