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
- [x] **3. Rasterizer** — Bresenham line, rectangle, ellipse, flood fill, tested against golden pixel arrays.
      `include/tinysprite/raster.h`: pure functions that report pixels through a plot callback and never touch a cel,
      so tools can route edits through history. Ellipses may report a pixel twice; flood fill only reads the cel.
- [x] **4. Pencil / eraser + History** — undo/redo unit is one stroke; storage is a pixel delta `(index, old, new)`.
      All edits must go through history.
      `history.h` (begin/set_pixel/end stroke, undo, redo) and `pencil.h` (`pencil_stroke` / `eraser_stroke` take a
      point list and join it with `raster_line`). Out-of-bounds pixels are clipped by history. Only the batch form
      exists; the UI (milestone 5) will need a live begin/extend/end stroke while the mouse is dragging.
- [x] **5. SDL2 + ImGui app + CPU-composited canvas** — zoom/pan, grid, checkerboard, coordinate transform
      (pure function, unit-tested), minimal foreground color picker. Enable with `-DTINYSPRITE_BUILD_UI=ON`
      (needs `libsdl2-dev`; the `tinysprite` binary lands in `<build>/src/ui/`).
      Core additions: `viewport.h` (zoom/pan/fit + screen<->cel transform), `composite.h` (flatten visible layers
      into an RGBA buffer, dirty-rect aware), `pencil_stroke_begin/extend/end/cancel` (live stroke for dragging)
      and `history_cancel_stroke`. UI: `src/ui/{app/main,canvas/canvas,editor/editor}.c`; `app/imgui_backend.cpp` is the only C++ of
      ours (wraps the SDL2 + SDL_Renderer backends that cimgui does not expose).
      Controls: left drag draws, wheel zooms at the cursor, middle drag pans, Ctrl+Z / Ctrl+Y (Ctrl+Shift+Z) undo/redo.
      `tinysprite [width [height]] [--frames N]` — `--frames` exits after N frames (smoke runs, e.g.
      `SDL_VIDEODRIVER=dummy`).
- [x] **6. Remaining tools + layer operations** — bucket, eyedropper, shapes, selection/move, pixel-perfect,
      symmetry, layer panel.
      Core (all TDD, public headers in `include/tinysprite/`): `bucket.h` (`bucket_fill`), `eyedropper.h`
      (`eyedropper_pick_cel` / `_sprite`, the latter via new `composite_pixel`), `shape.h` (line / rect / ellipse;
      batch `shape_stroke` plus live `shape_stroke_begin/update/end/cancel`), `move.h` (rectangular selection move,
      batch `move_stroke` plus live `move_stroke_*`), `PencilOptions` + `pencil_stroke_begin` (pixel-perfect,
      mirror X / Y), `sprite_remove_layer` / `sprite_move_layer`, `layer_set_name`, `history_forget_cel`.
      UI: `src/ui/editor/tools.c` (press / drag / release / cancel per tool), `src/ui/panels/layers_panel.c`; the tools panel
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
- UI stack is SDL2 + Dear ImGui via cimgui; the app code stays pure C.
- Undo is per stroke (not per pixel); only the internal storage is per pixel.
- Cel is RGBA now; the indexed-color mode from the original design is deferred.
- `scripts/lint.sh` and `scripts/format.sh` use `git ls-files -co`, so untracked files are checked too.
- clang-tidy runs with warnings-as-errors; explicit casts are required for multi-level pointer
  conversions such as `calloc`/`realloc`/`free` of `T **`.
- `tinysprite_io` has no sources yet (INTERFACE library); it becomes STATIC once `src/io/*.c` exists
  (core was already converted — see `CMakeLists.txt`).
- UI build: `cmake -S . -B build-ui -DTINYSPRITE_BUILD_UI=ON`; lint it with `scripts/lint.sh build-ui`
  (the default `build/` compile database has no `src/ui`). CI still builds core only.
- Zoom ranges 1/16x..64x (`VIEWPORT_ZOOM_MIN/MAX`); Fit picks a whole-number zoom, or a power of two below 1x for
  sprites larger than the canvas.
- Shell scripts are checked with `shellcheck` in CI (`shellcheck scripts/*.sh .githooks/*`).
- `tinysprite_core` links `libm` (viewport uses `floorf`).
- The canvas edits `Editor.active_layer` / `Editor.frame` (frame stays 0 until phase 2).
- Live shape / move strokes redraw from scratch on every update (`history_cancel_stroke` + a new stroke), so
  previews never accumulate and the whole drag is one undo step.
- Layer add / remove / reorder / rename / visibility / opacity are undoable through `layer_ops.h`, which records
  them with `history_push_action` (a step with its own undo / redo / release callbacks). A removed layer, or an
  added layer that was undone, is detached from the sprite and owned by its history step, so pixel steps on its
  cels stay valid and replay correctly; release callbacks must not dereference layers because steps are dropped
  oldest-first. `history_forget_cel` remains for callers that destroy a cel outside this path.
- The layer panel edits name / opacity live and records one step on widget release
  (`editor_commit_layer_name` / `_opacity` put the old value back and let `layer_op_*` record old -> new).
- Pixel-perfect also cuts the corner of a 90-degree turn (the L-shaped corner pixel is removed), like Aseprite.
  A removed pixel is restored to the color it had before the stroke touched it.
- Symmetry (`symmetry.h`) mirrors across the cel's center and applies to pencil / eraser, shapes
  (`shape_stroke` / `shape_stroke_begin` with a `ShapeStyle`) and bucket (`bucket_fill`).
- The selection is a single rectangle clipped to the sprite. Moving it loses pixels pushed off the sprite, and
  transparent selected pixels do not erase the destination. A click without dragging selects the one cel under
  it, unless the click only dismisses an existing selection. There is no selection copy / paste yet.
- Tools ignore the press while the active layer is hidden (the eyedropper still samples the flattened image).
- `third_party/cimgui` declares `ImDrawList_AddRect(..., rounding, thickness, flags)`; the order differs from
  upstream ImGui's `(rounding, flags, thickness)`, and clang-tidy's narrowing check is what caught the swap.
- `tests/ui/ui_editor.c` (ctest `ui_editor`, only with `-DTINYSPRITE_BUILD_UI=ON`, runs under
  `SDL_VIDEODRIVER=dummy`) covers `editor.c` and `tools.c`; `layers_panel.c` (ImGui widgets) is still not covered.
  CI builds core only, so the UI tests run locally. `scripts/lint.sh` skips `tests/ui` like `src/ui`.
- Zoom and hovered cel are drawn as a canvas overlay (`draw_status`), after the canvas applied the frame's
  zoom / pan / hover, so they no longer lag the image by a frame.

## Convenience features (outside the milestones)
- Shortcuts live in `src/ui/app/main.c` (`plain_shortcuts`): B E G I L R O M pick tools, X swaps colors, F fits,
  `+` / `-` zoom, `[` / `]` brush size, Alt = temporary eyedropper (`editor_temp_pick`), Space + drag pans.
- Clipboard (`clip.h`): copy / cut / paste and selection flip / rotate. A rotated selection keeps its center and
  loses what leaves the sprite. Undoing a flip / rotate does not restore the selection rectangle.
- Palette edits (`palette_remove`, add / pick in `editor.c`) and `RecentColors` are not part of undo. Recent
  colors are recorded on press by every drawing tool except the eraser and the eyedropper.
- Brush (`PencilOptions.size` / `round`): square or round, 1..`PENCIL_BRUSH_MAX`; each stamp pixel is mirrored
  separately. Pixel-perfect is ignored above size 1. Shapes and the bucket do not use the brush.
- `layer_op_merge_down` snapshots the lower layer's cels before / after (whole-cel memcpy) instead of pixel
  deltas, so earlier pixel steps on either layer stay valid after undo. Both layers must be visible.
- UI layout (`src/ui/panels/panels.c`): Tools / Layers are docked left / right (resizable by the inner edge, 170-480 px) or
  floating windows; dragging a floating title bar to an edge docks it. Layout is not persisted. The canvas refits
  on resize until the user zooms or pans (`Editor.auto_fit`). Open / Save / Export in the File menu are disabled
  until `src/io` has the `.pxa` reader / writer.
- Buttons use Material Symbols Outlined glyphs from an embedded font subset (`icons/icon_font.c`, regenerate with
  `scripts/gen-icon-font.py`; needs `pip install fonttools`). Glyph centering offsets in `icons/icons.c` were measured
  from rendered output, so re-measure if the font size or ImGui version changes.
