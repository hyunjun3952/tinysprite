# tinysprite

A small pixel art / 2D sprite animation editor written in C++17, with a UI built on SDL2 and Dear ImGui.

> **Status:** early development. Phase 1 (drawing tools) is mostly done; file I/O (`.pxa`, image export) is in place.
> Animation and game-pipeline features are planned. See [docs/MILESTONES.md](docs/MILESTONES.md).

## Features

- Tools: pencil, eraser, bucket fill, eyedropper, line / rectangle / ellipse, rectangular selection and move
- Pencil options: pixel-perfect, mirror X / Y
- Layers: add, remove, reorder, rename, visibility, opacity
- Undo / redo (one step per stroke, including layer operations)
- Zoomable, pannable canvas with grid and checkerboard background

Planned: timeline and onion skin, GIF / spritesheet export, tilemaps, JSON export, headless CLI.

## Building

Requirements: CMake 3.16+ and a C++17 compiler. The UI also needs SDL2 (`sudo apt install libsdl2-dev`).

```sh
# Core library and tests only
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

# With the SDL2 + ImGui frontend
cmake -S . -B build-ui -DTINYSPRITE_BUILD_UI=ON
cmake --build build-ui
./build-ui/src/ui/tinysprite [width [height]]
```

ASan/UBSan are enabled by default; pass `-DTINYSPRITE_SANITIZE=OFF` to disable them.

## Controls

| Input | Action |
|-------|--------|
| Left drag | Draw with the active tool |
| Mouse wheel | Zoom at the cursor |
| Middle drag | Pan |
| Ctrl+Z / Ctrl+Y (Ctrl+Shift+Z) | Undo / redo |
| Ctrl+A | Select all |
| Delete | Clear the selection |
| Esc | Cancel the current drag, or clear the selection |

## Project layout

```
include/tinysprite/   public headers (core and io)
src/core/             document model, rasterizer, tools, history (no SDL/ImGui)
src/io/               file formats
src/ui/               SDL2 + ImGui frontend
tests/                one test file per feature, using third_party/minitest.h
third_party/          vendored dependencies (see THIRD_PARTY.md)
```

The layering is enforced by CMake targets: `tinysprite_core` → `tinysprite_io` → `tinysprite_ui`.

## Contributing

Development is test-first. Format and lint before committing:

```sh
scripts/setup-hooks.sh   # once per clone
scripts/format.sh        # --check to verify only
scripts/lint.sh
```

Commits follow [Conventional Commits](https://www.conventionalcommits.org/), e.g. `feat(core): add flood fill`.

## License

[MIT](LICENSE). Third-party components and their licenses are listed in [THIRD_PARTY.md](THIRD_PARTY.md).
