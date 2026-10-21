# Milestones

Phase 1 = drawing-tool core (MVP). Animation is phase 2, game pipeline is phase 3.
Every step is TDD: failing test first, then implementation, then refactor.
Full design rationale: see the "Architecture" and "Conventions" sections of `CLAUDE.md`.

## Phase 1

- [x] **1. Project foundation** — git, CMake + CTest, ASan/UBSan, `minitest.h`, smoke test.
      SDL2/ImGui are only checked for availability here (SDL2 is not installed yet: `sudo apt install libsdl2-dev`).
- [x] **1b. Format / lint automation** — K&R `.clang-format` (4-space indent), `.clang-tidy`,
      `scripts/{format,lint,setup-hooks}.sh`, `.githooks/`, GitHub Actions CI.
- [x] **2. Core data model** — `Sprite` / `Layer` / `Cel` / `Palette`, opaque types, public headers in `include/tinysprite/`.
      Cel is RGBA only for now; palette is 256 colors max; sprite starts with 1 frame and 1 layer.
- [x] **3. Rasterizer** — Bresenham line, rectangle, ellipse, flood fill, tested against golden pixel arrays.
      `include/tinysprite/raster.h`: pure functions that report pixels through a plot callback and never touch a cel,
      so tools can route edits through history. Ellipses may report a pixel twice; flood fill only reads the cel.
- [x] **4. Pencil / eraser + History** — undo/redo unit is one stroke; storage is a pixel delta `(index, old, new)`.
      All edits must go through history.
      `history.h` (`beginStroke` / `setPixel` / `endStroke`, undo, redo) and `tools.h` (`pencil_stroke` takes a
      point list and joins it with `raster_line`). Out-of-bounds pixels are clipped by history. Only the batch form
      exists; the UI (milestone 5) will need a live begin/extend/end stroke while the mouse is dragging.
- [x] **5. SDL2 + ImGui app + CPU-composited canvas** — zoom/pan, grid, checkerboard, coordinate transform
      (pure function, unit-tested), minimal foreground color picker. Enable with `-DTINYSPRITE_BUILD_UI=ON`
      (needs `libsdl2-dev`; the `tinysprite` binary lands in `<build>/src/ui/`).
      Core additions: `viewport.h` (zoom/pan/fit + screen<->cel transform), `composite.h` (flatten visible layers
      into an RGBA buffer, dirty-rect aware), `PencilStroke` (live stroke for dragging)
      and `History::cancelStroke`. UI: `src/ui/{app,canvas,editor}.cpp` (the backends are called from `App` directly).
      Controls: left drag draws, wheel zooms at the cursor, middle drag pans, Ctrl+Z / Ctrl+Y (Ctrl+Shift+Z) undo/redo.
      `tinysprite [width [height]] [--frames N]` — `--frames` exits after N frames (smoke runs, e.g.
      `SDL_VIDEODRIVER=dummy`).
- [x] **6. Remaining tools + layer operations** — bucket, eyedropper, shapes, selection/move, pixel-perfect,
      symmetry, layer panel.
      Core (all TDD, public headers in `include/tinysprite/`): `tools.h` (`bucket_fill`, eyedropper
      (`eyedropper_pick_cel` / `_sprite`, the latter via new `composite_pixel`), shape (line / rect / ellipse;
      batch `shape_stroke` plus live `ShapeStroke`), `selection.h` (rectangular selection move,
      batch `move_stroke` plus live `MoveStroke`), `PencilOptions` + `PencilStroke::begin` (pixel-perfect,
      mirror X / Y), `Sprite::removeLayer` / `moveLayer`, `Layer::setName`, `History::forgetCel`.
      UI: `src/ui/drags.cpp` (one `Drag` subclass per tool), `src/ui/layers_panel.cpp`; the tools panel
      lists all eight tools plus Pixel-perfect / Mirror X / Mirror Y / Filled options.
      Controls: Esc cancels a drag (or clears the selection), Delete clears the selection, Ctrl+A selects all.
- [ ] **7. File I/O** — `.pxa` (versioned, length-prefixed chunks, round-trip tests) and PNG via stb.
- [ ] **8. Polish** — palette files (.gpl/.hex), settings, autosave recovery, shortcuts, Linux/Windows build check.

## Phase 2: animation
Timeline, per-frame duration, onion skin, playback, tags, linked cels, GIF / spritesheet export.

## Phase 3: game pipeline
Spritesheet packing + JSON metadata, slices/pivots/hitboxes, tilemap editor, filters,
headless CLI export, optional Aseprite import.

## Decisions and gotchas
- Rasterizers clip nothing; the caller (tool/history layer) must bounds-check plotted points.
- UI stack is SDL2 + Dear ImGui (native C++ API, `third_party/imgui`); the whole project is C++17 (it was C11 for core / io, with a C++ UI, until the cpp-migration).
- Undo is per stroke (not per pixel); only the internal storage is per pixel.
- Cel is RGBA now; the indexed-color mode from the original design is deferred.
- `scripts/lint.sh` and `scripts/format.sh` use `git ls-files -co`, so untracked files are checked too.
- clang-tidy runs with warnings-as-errors; explicit casts are required for multi-level pointer
  conversions such as `calloc`/`realloc`/`free` of `T **`.
- `tinysprite_io` has no sources yet (INTERFACE library); it becomes STATIC once `src/io/*.cpp` exists
  (core was already converted — see `CMakeLists.txt`).
- UI build: `cmake -S . -B build-ui -DTINYSPRITE_BUILD_UI=ON`; lint it with `scripts/lint.sh build-ui`
  (the default `build/` compile database has no `src/ui`). CI still builds core only.
- Zoom ranges 1/16x..64x (`VIEWPORT_ZOOM_MIN/MAX`); Fit picks a whole-number zoom, or a power of two below 1x for
  sprites larger than the canvas.
- Shell scripts are checked with `shellcheck` in CI (`shellcheck scripts/*.sh .githooks/*`).
- `tinysprite_core` links `libm` (viewport uses `floorf`).
- The canvas edits `Editor.active_layer` / `Editor.frame` (frame stays 0 until phase 2).
- Live shape / move strokes redraw from scratch on every update (`LiveStroke::restart`), so
  previews never accumulate and the whole drag is one undo step.
- Layer add / remove / reorder / rename / visibility / opacity are undoable through `layer_ops.h`, which records
  them with `History::pushAction` (a step with its own undo / redo callables, destroyed with the step). A removed layer, or an
  added layer that was undone, is detached from the sprite and owned by its history step, so pixel steps on its
  cels stay valid and replay correctly; release callbacks must not dereference layers because steps are dropped
  oldest-first. `History::forgetCel` remains for callers that destroy a cel outside this path.
- The layer panel edits name / opacity live and records one step on widget release
  (`Editor::commitLayerName` / `commitLayerOpacity` record before -> current through `layer_op_record_*`).
- Pixel-perfect also cuts the corner of a 90-degree turn (the L-shaped corner pixel is removed), like Aseprite.
  A removed pixel is restored to the color it had before the stroke touched it.
- Symmetry (`symmetry.h`) mirrors across the cel's center and applies to pencil / eraser, shapes
  (`shape_stroke` / `ShapeStroke::begin` with a `ShapeStyle`) and bucket (`bucket_fill`).
- The selection is a single rectangle clipped to the sprite. Moving it loses pixels pushed off the sprite, and
  transparent selected pixels do not erase the destination. A click without dragging selects the one cel under
  it, unless the click only dismisses an existing selection. There is no selection copy / paste yet.
- Tools ignore the press while the active layer is hidden (the eyedropper still samples the flattened image).
- `tests/ui/ui_editor.cpp` (ctest `ui_editor`, only with `-DTINYSPRITE_BUILD_UI=ON`, runs under
  `SDL_VIDEODRIVER=dummy`) covers `editor.cpp` and `tools.cpp`; `layers_panel.cpp` (ImGui widgets) is still not covered.
  CI builds core only, so the UI tests run locally. `scripts/lint.sh` skips `tests/ui` like `src/ui`.
- Zoom and hovered cel are drawn as a canvas overlay (`draw_status`), after the canvas applied the frame's
  zoom / pan / hover, so they no longer lag the image by a frame.

## Convenience features (outside the milestones)
- Shortcuts live in `src/ui/app.cpp` (`App::plainShortcuts`): B E G I L R O M pick tools, X swaps colors, F fits,
  `+` / `-` zoom, `[` / `]` brush size, Alt = temporary eyedropper (`editor_temp_pick`), Space + drag pans.
- Clipboard (`selection.h`): copy / cut / paste and selection flip / rotate. A rotated selection keeps its center and
  loses what leaves the sprite. Undoing a flip / rotate does not restore the selection rectangle.
- Palette edits (`palette_remove`, add / pick in `editor.cpp`) and `RecentColors` are not part of undo. Recent
  colors are recorded on press by every drawing tool except the eraser and the eyedropper.
- Brush (`PencilOptions.size` / `round`): square or round, 1..`PENCIL_BRUSH_MAX`; each stamp pixel is mirrored
  separately. Pixel-perfect is ignored above size 1. Shapes and the bucket do not use the brush.
- `layer_op_merge_down` snapshots the lower layer's cels before / after (whole-cel memcpy) instead of pixel
  deltas, so earlier pixel steps on either layer stay valid after undo. Both layers must be visible.
- UI layout (`src/ui/panels.cpp`): Tools / Layers are docked left / right (resizable by the inner edge, 170-480 px) or
  floating windows; dragging a floating title bar to an edge docks it. Layout is not persisted. The canvas refits
  on resize until the user zooms or pans (`Editor.auto_fit`). Open / Save / Export in the File menu are disabled
  until `src/io` has the `.pxa` reader / writer.
- Buttons use Material Symbols Outlined glyphs from an embedded font subset (`icon_font.inc`, regenerate with
  `scripts/gen-icon-font.py`; needs `pip install fonttools`). ImGui sizes a font by its line height, so the script
  makes the line box exactly one em (descent 1 unit, not 0: ImGui `ImCeil`s the scaled ascent and an exact fit can
  round up a pixel). That is what lets `icons/icons.cpp` center a glyph with plain arithmetic instead of per-size fudge
  factors. Material Symbols have 2-unit strokes on a 24-unit grid, so glyphs are crispest at a 12 px multiple (24 px has about
  half as many smeared edge pixels as 16 px; buttons use 20 px, a little softer, because 24 px looked too big). For the same reason `OversampleH = 1`; oversampling blurs aligned strokes and shifts the glyph
  by about half a pixel. Re-check centering and crispness if the ImGui version changes.
