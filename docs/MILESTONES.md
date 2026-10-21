# Milestones

Phase 1 = drawing-tool core (MVP). Animation is phase 2, game pipeline is phase 3.
Every step is TDD: failing test first, then implementation, then refactor.
Rules: `CLAUDE.md`. Design map: [ARCHITECTURE.md](ARCHITECTURE.md).

## Phase 1

Done. The design is described in [ARCHITECTURE.md](ARCHITECTURE.md); the gotchas that came out of it are below.

- [x] **1. Project foundation** — git, CMake + CTest, ASan/UBSan, `minitest.h`, smoke test.
- [x] **1b. Format / lint automation** — `.clang-format` / `.clang-tidy`, `scripts/{format,lint,setup-hooks}.sh`,
      `.githooks/`, GitHub Actions CI.
- [x] **2. Core data model** — `Sprite` / `Layer` / `Cel` / `Palette`, public headers in `include/tinysprite/`.
      Cel is RGBA only; the palette holds at most 256 colors; a sprite starts with 1 frame and 1 layer.
- [x] **3. Rasterizer** — line, rectangle, ellipse, polygon, flood fill, tested against golden pixel arrays.
- [x] **4. Pencil / eraser + History** — one undo step per stroke, stored as pixel deltas.
- [x] **5. SDL2 + ImGui app + CPU-composited canvas** — zoom / pan, grid, checkerboard, `Viewport` transform,
      `composite_sprite`, live strokes. `tinysprite [width [height]] [--frames N]`: `--frames` exits after N frames
      (smoke runs, e.g. `SDL_VIDEODRIVER=dummy`).
- [x] **6. Remaining tools + layer operations** — bucket, eyedropper, shapes, selection / move, pixel-perfect,
      symmetry, undoable layer operations (`layer_ops.h`), layer panel.
- [x] **7. File I/O** — `.pxa` (versioned, chunked) and image export / import via stb; File menu with a path dialog,
      since there is no native file chooser.
- [x] **8. Polish** — palette files (`.gpl` / `.hex`), settings (`settings.ini`), autosave recovery, shortcuts,
      Linux and Windows (MinGW) builds in CI. The Windows job could not be built locally; it only relies on
      `std::filesystem` and `%APPDATA%`.

## Phase 2: animation
Timeline, per-frame duration, onion skin, playback, tags, linked cels, GIF / spritesheet export.

## Phase 3: game pipeline
Spritesheet packing + JSON metadata, slices/pivots/hitboxes, tilemap editor, filters,
headless CLI export, optional Aseprite import.

## Decisions and gotchas
- Rasterizers clip nothing; the caller (tool/history layer) must bounds-check plotted points.
- UI stack is SDL2 + Dear ImGui (native C++ API, `thirdparty/imgui`); the whole project is C++17 (it was C11 for core / io, with a C++ UI, until the cpp-migration).
- Undo is per stroke (not per pixel); only the internal storage is per pixel.
- Cel is RGBA now; the indexed-color mode from the original design is deferred.
- `scripts/lint.sh` and `scripts/format.sh` use `git ls-files -co`, so untracked files are checked too.
- clang-tidy runs with warnings-as-errors; explicit casts are required for multi-level pointer
  conversions such as `calloc`/`realloc`/`free` of `T **`.
- stb is compiled only in `src/io/stb_impl.cpp`, without warnings; the rest of `src/io` includes the headers.
- There is one build dir, `build/`: the UI is built whenever SDL2 is found (`TINYSPRITE_BUILD_UI` defaults to that), so
  `scripts/lint.sh` covers `src/ui` and `tests/ui` too. An old `build/` keeps a cached `OFF`: re-run cmake with `-DTINYSPRITE_BUILD_UI=ON`.
  CI builds the UI explicitly (`-DTINYSPRITE_BUILD_UI=ON`) and runs these tests.
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
  it, unless the click only dismisses an existing selection.
- Tools ignore the press while the active layer is hidden (the eyedropper still samples the flattened image).
- `tests/ui/ui_editor_*.cpp` (ctest `ui_editor_{selection,layers,colors,drawing,document}`, shared helpers in
  `ui_editor_support.h`; only when the UI is built, run under `SDL_VIDEODRIVER=dummy`) cover `editor*.cpp` and `drags.cpp`; of `layers_panel.cpp` (ImGui widgets) only the in-place rename is covered (`ui_layers_panel`).
  CI builds the UI, so the `ui_*` tests run there too.
- Zoom and hovered cel are drawn as a canvas overlay (`draw_status`), after the canvas applied the frame's
  zoom / pan / hover, so they no longer lag the image by a frame.

## Convenience features (outside the milestones)
- Panning: the right or middle button drag (or Space + left drag) moves the view with a hand cursor; the canvas item accepts
  the right button for that, so a right click never draws (`tests/ui/ui_canvas.cpp`).
- Shortcuts live in `src/ui/app_shortcuts.cpp` (`App::plainShortcuts`): B E G I L R O M pick tools, X swaps colors, F fits,
  `+` / `-` and `[` / `]` brush size (1 px; `Editor::changeBrushSize`), the wheel zooms, holding the middle button = temporary eyedropper (`Editor::tempPick`; dragging it pans), Space + drag pans.
- Clipboard (`selection.h`): copy / cut / paste and selection flip / rotate. A rotated selection keeps its center and
  loses what leaves the sprite. Undoing a flip / rotate does not restore the selection rectangle.
- Palette edits (`palette_remove`, add / pick in `editor_palette.cpp`) and `RecentColors` are not part of undo. Recent
  colors are recorded on press by every drawing tool except the eraser and the eyedropper.
- Brush (`PencilOptions.size` / `round`): square or round, 1..`PENCIL_BRUSH_MAX`; each stamp pixel is mirrored
  separately. Pixel-perfect is ignored above size 1. Shapes and the bucket do not use the brush.
- `layer_op_merge_down` snapshots the lower layer's cels before / after (whole-cel memcpy) instead of pixel
  deltas, so earlier pixel steps on either layer stay valid after undo. Both layers must be visible.
- UI layout (`src/ui/panels.cpp`): Tools / Layers are docked left / right (resizable by the inner edge, 170-480 px) or
  floating windows; dragging a docked panel's title bar pulls it out (it follows the mouse until release), and
  dragging a floating title bar to an edge docks it. The layout is persisted (`Panels::store` / `load`). The canvas refits
  on resize until the user zooms or pans (`Editor.auto_fit`).
- Look (`src/ui/theme.cpp`): a warm near-charcoal dark theme after the Warp design language (canvas `#2b2622`, cards /
  inputs `#383330`, 1 px hairlines `#3f3a36`, 3-4 px corner radii, no shadows). The off-white `#f7f5f0` is the only accent:
  the selected tool is drawn inverted (`theme::choiceButton`). Buttons are text, grouped into collapsible
  sections (`theme::section`) in the Tools and Layers panels, with the shortcut letter at the button's right end;
  only the layer rows and layer buttons have icons (`src/ui/icons.cpp`: 16x16 pixel bitmaps kept as strings, drawn
  as rects, so there is no font or generator: edit the rows to change one; `tests/ui/ui_icons.cpp` checks their shape). The
  Tool options section shows only what the active tool uses. ImGui's built-in font is still used (Inter / DM Mono are not
  embedded yet). `tinysprite --frames N --screenshot out.bmp` saves the last frame, for checking the look without a
  display; `ctest -R ui_smoke` runs three frames of the app.
- Customizing the UI: Window > <panel> > Sections toggles each collapsible section (`Panels::sectionVisible`, names in
  `namespace sections`; the menu stays open while toggling; "Reset layout and sections" restores everything). View
  toggles the pixel grid, cursor outline, mirror axes and status line (`Editor::overlays`, kept across documents).
  Each layer row shows a thumbnail of what is painted on that layer (`LayerThumbnails`: one texture per layer, sized by
  `thumbnail_size`, filled by `thumbnail_pixels` which samples nearest so the cost is independent of the sprite size,
  refreshed when `Editor::revision()` changes, dimmed while the layer is hidden).
  Layer selection (`Editor::pickLayer`): a click selects one layer, Ctrl+click adds / removes one (the last one cannot
  be removed), Shift+click selects from the last plain / Ctrl pick to the clicked row. The selection is the active
  layer (what the tools draw on, marked by a bar) plus the picked layers, kept by identity so it follows moves and
  adds. Delete, duplicate, merge, dragging and the eye / lock toggles act on all selected layers as one undo step each
  (`History::beginGroup` / `endGroup` fold the per-layer steps into one); deleting is refused when it would leave no
  layer, the opacity slider stays single-layer, and a Ctrl / Shift double click never starts a rename.
  Merging several layers (`Editor::mergeSelectedLayers`) draws them, bottom to top with their opacities, onto the
  lowest selected one, which keeps its name and properties; unselected layers between them are first left above the
  result (selected layers are moved next to each other, then merged down topmost first). All must be visible and the
  lowest unlocked (`canMergeLayers`); one selected layer merges down as before.
  Dragging a selected row moves the whole selection as a block (order kept) next to the drop row; dropping on a
  selected row does nothing.
  Layers are reordered by dragging a row onto another (`moveLayer`, one undo step, the dragged layer stays active;
  there are no up / down buttons). While dragging, a 2 px off-white line across the hovered row marks where the layer
  will land (above the row for a layer coming from below, under it for one coming from above; none over its own row). With the layer list focused (`LayersPanel::focused`, set from the window's
  focus in the frame it is drawn) the Delete key and the trash button ask "Delete layer?" first (Enter confirms,
  Escape cancels, undo brings it back); elsewhere Delete still clears the selected pixels. While that dialog is
  open the app's shortcuts are off.
  Double-clicking a layer row renames it in place (live, one undo step on release; Esc cancels).
  A disabled button shows no tooltip but the "not allowed" mouse cursor (`theme::hint`). None of this is persisted yet. Items whose purpose is not obvious from the label have a tooltip (`theme::tip`, shown
  after the style's hover delay, also on disabled widgets).
- `Panels::end` only calls `ImGui::End` if `begin` started a window: a hidden panel (closed with its X button or
  hidden from the Window menu) used to trip "Calling End() too many times!". `tests/ui/ui_panels.cpp` covers it.
- Unsaved changes: `History::markSaved` / `atSavePoint` track the saved state (undoing back to it counts as clean; an
  edit that replaces the saved steps or `forgetCel` makes it permanently dirty). Palette edits are outside undo, so
  `Editor::modifiedOutsideHistory` (also set by `markModified`) counts them as unsaved until the next save. `App` shows `*` in the window title and asks Save / Discard / Cancel before
  Open, Quit and a dropped file replace the document.
- Opening files: `tinysprite [file | width [height]]`, `--help`, and `SDL_DROPFILE` (drop a .pxa or image on the window).
- Layer lock (`Layer::locked`, `layer_op_set_locked`, undoable): `Editor::editableCel` returns null for a locked layer,
  so tools, paste, cut and delete skip it; merge down refuses a locked lower layer. The model does not enforce it.
  Stored in bit 1 of the LAYR visible byte of `.pxa` (bit 0 is visible), so older readers still load the file.
- Selection clipping: `History::setClip` rejects `setPixel` outside a rect. `Editor::press` sets it to the selection
  for the drawing tools (not select / eyedropper) and lifts it on release / cancel. The bucket still scans the whole
  region but only writes inside the selection.
- Shift+click with the pencil / eraser draws a line from `Editor::lastDrawPoint` (end of the previous stroke; reset when
  the document is replaced). Shift is only read at the press, not while dragging.
- Shift while dragging a shape keeps even proportions (`shape_constrain` in `tools.h`: a square box, a circle, a line snapped to
  horizontal / vertical / 45 degrees); `Editor::drag(cx, cy, shift)` passes it to `Drag::setShift`.
- `image_export(..., scale)` enlarges by whole pixels (1..`IMAGE_EXPORT_MAX_SCALE`, at most `IMAGE_EXPORT_MAX_SIZE` px); the
  Export image dialog has a Scale field that is remembered. File > Open recent keeps ten `.pxa` paths (`recent_add`,
  `recent.N` keys), the New dialog has 16 / 32 / 64 / 128 / 256 presets and remembers its size, Ctrl + / - / 0 zoom.
- F1 / `?` opens the keyboard shortcut list (`App::helpWindow`, also in the Help menu).
- Dev packages are listed once in `scripts/install-deps.sh` (CI and `.devcontainer/` both call it); add new apt
  dependencies there. The devcontainer does not forward a display, so the UI window is not shown from inside it.
