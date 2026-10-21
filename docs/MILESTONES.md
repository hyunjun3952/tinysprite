# Milestones

Phase 1 = drawing-tool core (MVP). Animation is phase 2, game pipeline is phase 3.
Every step is TDD: failing test first, then implementation, then refactor.
Full design rationale: see the "Architecture" and "Conventions" sections of `CLAUDE.md`.

## Phase 1

- [x] **1. Project foundation** — git, CMake + CTest, ASan/UBSan, `minitest.h`, smoke test.
      SDL2/cimgui are only checked for availability here (SDL2 is not installed yet: `sudo apt install libsdl2-dev`).
- [x] **1b. Format / lint automation** — K&R `.clang-format` (4-space indent), `.clang-tidy`,
      `scripts/{format,lint,setup-hooks}.sh`, `.githooks/`, GitHub Actions CI.
- [x] **2. Core data model** — `Sprite` / `Layer` / `Cel` / `Palette`, opaque types, public headers in `include/tinysprite/`.
      Cel is RGBA only for now; palette is 256 colors max; sprite starts with 1 frame and 1 layer.
- [ ] **3. Rasterizer** — Bresenham line, rectangle, ellipse, flood fill, tested against golden pixel arrays.
- [ ] **4. Pencil / eraser + History** — undo/redo unit is one stroke; storage is a pixel delta `(index, old, new)`.
      All edits must go through history.
- [ ] **5. SDL2 + ImGui app + CPU-composited canvas** — zoom/pan, grid, checkerboard, coordinate transform
      (pure function, unit-tested), minimal foreground color picker. Enable with `-DTINYSPRITE_BUILD_UI=ON`
      (`src/ui/CMakeLists.txt` does not exist yet).
- [ ] **6. Remaining tools + layer operations** — bucket, eyedropper, shapes, selection/move, pixel-perfect, symmetry,
      layer panel (remove / reorder are not implemented yet).
- [ ] **7. File I/O** — `.pxa` (versioned, length-prefixed chunks, round-trip tests) and PNG via stb.
- [ ] **8. Polish** — palette files (.gpl/.hex), settings, autosave recovery, shortcuts, Linux/Windows build check.

## Phase 2: animation
Timeline, per-frame duration, onion skin, playback, tags, linked cels, GIF / spritesheet export.

## Phase 3: game pipeline
Spritesheet packing + JSON metadata, slices/pivots/hitboxes, tilemap editor, filters,
headless CLI export, optional Aseprite import.

## Decisions and gotchas
- UI stack is SDL2 + Dear ImGui via cimgui; the app code stays pure C.
- Undo is per stroke (not per pixel); only the internal storage is per pixel.
- Cel is RGBA now; the indexed-color mode from the original design is deferred.
- `scripts/lint.sh` and `scripts/format.sh` use `git ls-files -co`, so untracked files are checked too.
- clang-tidy runs with warnings-as-errors; explicit casts are required for multi-level pointer
  conversions such as `calloc`/`realloc`/`free` of `T **`.
- `tinysprite_io` has no sources yet (INTERFACE library); it becomes STATIC once `src/io/*.c` exists
  (core was already converted — see `CMakeLists.txt`).
