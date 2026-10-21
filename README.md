# tinysprite

A small pixel art / 2D sprite animation editor written in C++17, with a UI built on SDL2 and Dear ImGui.

> **Status:** early development. Phase 1 (drawing tools) is mostly done; file I/O (`.pxa`, image export) is in place.
> Animation and game-pipeline features are planned. See [docs/MILESTONES.md](docs/MILESTONES.md).

## Features

- Tools: pencil, eraser, bucket fill, eyedropper, line / rectangle / ellipse / triangle / diamond / hexagon / star / cross, rectangular selection and move
- Pencil options: pixel-perfect, mirror X / Y
- Layers: add, remove, duplicate, merge down, reorder, rename, visibility, lock, opacity
- Unsaved-changes marker and prompt; open files by dragging them onto the window or on the command line
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
./build-ui/src/ui/tinysprite [file | width [height]]
```

ASan/UBSan are enabled by default; pass `-DTINYSPRITE_SANITIZE=OFF` to disable them.

### Devcontainer

`.devcontainer/` sets up an Ubuntu container with the build tools, `clang-format`, `clang-tidy`, `shellcheck` and
SDL2 (`scripts/install-deps.sh`, also used by CI), and enables the git hooks. Open the folder in VS Code and pick
"Reopen in Container". Building, tests, format and lint work out of the box; showing the UI window needs a display
forwarded into the container (e.g. WSLg), which is not configured by default.

## Controls

| Input | Action |
|-------|--------|
| Left drag | Draw with the active tool (limited to the selection, if any) |
| Shift + click | Pencil / eraser: straight line from the last point |
| F1 or ? | Keyboard shortcut list |
| Mouse wheel | Zoom at the cursor |
| + / - (or [ / ]) | Brush size by 1 |
| Right or middle drag, Space + drag | Pan |
| Hold the middle button | Eyedropper while held |
| Ctrl + / Ctrl - / Ctrl+0 | Zoom in / out / fit |
| Shift while dragging a shape | Square or circle; a line snaps to 45 degrees |
| Ctrl+Z / Ctrl+Y (Ctrl+Shift+Z) | Undo / redo |
| Ctrl+N / Ctrl+O | New sprite, open a file |
| Ctrl+S / Ctrl+Shift+S / Ctrl+E | Save, save as, export an image |
| Ctrl+A | Select all |
| Delete | Clear the selection |
| Esc | Cancel the current drag, or clear the selection |

## Files and settings

- `.pxa` is the native format; images (png, bmp, tga, jpg) can be imported and exported.
- File > Open recent lists the last ten files; Export image can enlarge the picture (scale 1-32, nearest neighbor).
- File > Import / Export palette reads and writes `.gpl` (GIMP) and `.hex` (one `RRGGBB[AA]` per line) files.
- Preferences (grid, overlays, brush and shape options, colors, tool, panel layout, window size) are kept in
  `settings.ini` in the config directory: `$XDG_CONFIG_HOME/tinysprite` or `~/.config/tinysprite` (`%APPDATA%\tinysprite`
  on Windows). `--config-dir DIR` or `TINYSPRITE_CONFIG_DIR` picks another one. Symmetry always starts off.
- While a document has unsaved changes it is autosaved there every 30 seconds; after a crash the next start offers to
  recover it.

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
