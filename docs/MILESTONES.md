# Milestones

Phase 1 = drawing-tool core (MVP). Animation is phase 2, game pipeline is phase 3.
Every step is TDD: failing test first, then implementation, then refactor.
Rules: `CLAUDE.md`. Design map: [ARCHITECTURE.md](ARCHITECTURE.md).

## Phase 1

Done. The design is described in [ARCHITECTURE.md](ARCHITECTURE.md); the gotchas that came out of it are below.

- [x] **1. Project foundation** — git, CMake + CTest, ASan/UBSan, `minitest.hpp`, smoke test.
- [x] **1b. Format / lint automation** — `.clang-format` / `.clang-tidy`, `.pre-commit-config.yaml` (clang-format on commit, clang-tidy on push), GitHub Actions CI.
- [x] **2. Core data model** — `Sprite` / `Layer` / `Cel` / `Palette`, public headers in `include/tinysprite/`.
      Cel is RGBA only; the palette holds at most 256 colors; a sprite starts with 1 frame and 1 layer.
- [x] **3. Rasterizer** — line, rectangle, ellipse, polygon, flood fill, tested against golden pixel arrays.
- [x] **4. Pencil / eraser + History** — one undo step per stroke, stored as pixel deltas.
- [x] **5. SDL2 + ImGui app + CPU-composited canvas** — zoom / pan, grid, checkerboard, `Viewport` transform,
      `composite_sprite`, live strokes. `tinysprite [width [height]] [--frames N]`: `--frames` exits after N frames
      (smoke runs, e.g. `SDL_VIDEODRIVER=dummy`).
- [x] **6. Remaining tools + layer operations** — bucket, eyedropper, shapes, selection / move, pixel-perfect,
      symmetry, undoable layer operations (`layer_ops.hpp`), layer panel.
- [x] **7. File I/O** — `.pxa` (versioned, chunked) and image export / import via stb; File menu with a path dialog,
      since there is no native file chooser.
- [x] **8. Polish** — palette files (`.gpl` / `.hex`), settings (`settings.ini`), autosave recovery, shortcuts,
      Linux and Windows (MinGW) builds in CI. The Windows job could not be built locally; it only relies on
      `std::filesystem` and `%APPDATA%`.

## Phase 2: animation

Done. The design is in [ARCHITECTURE.md](ARCHITECTURE.md); the gotchas are in the list below.

- [x] **9. Frames** — `Sprite` inserts, detaches and moves frames, each with a duration; `frame_ops.hpp` records them as
      undo steps; `.pxa` stores durations in an optional `FRAM` chunk and reads several frames.
- [x] **10. Timeline and playback** — the `Editor` has a current frame and a `Playback` that `tick()` advances; a
      timeline strip with a cel grid, frame buttons, duration and play mode; `,` `.` `Enter` and a Frame menu.
- [x] **11. Onion skin** — `composite_onion` draws neighbouring frames tinted and fading under the current one.
- [x] **12. Tags** — named frame ranges with a direction (`tag_ops.hpp`, `TAGS` chunk); a selected tag limits playback.
- [x] **13. Linked cels** — frames of a layer can share one cel (`cel_ops.hpp`, `LINK` chunk).
- [x] **14. Export** — `gif_encode` (own LZW encoder) and `spritesheet_render`, with File menu dialogs; animated GIF
      import.

## UI polish (after phase 2)

Done: panel headers without repeats, a status bar and toasts, brighter panel borders, a UI scale (View menu,
`--scale`, saved in `settings.ini`; the default comes from the display DPI), a path dialog that remembers its folder,
adds the extension and asks before overwriting, and a Properties section (opacity, size, linked cels) in Layers.

- Tables instead of switches: `App::fileInfo` (one row per `FileAction`), `App::commandAction` (one row per
  `CommandId`) and the extension tables in `src/io` (`image_write_formats`, `palette_formats` feed the error texts).
- Not done: a TTF font (the 13 px bitmap font is only scaled), a
  file browser, HiDPI window flags, the OS reason in a failed-write message (the io functions return `bool`).
- Left from the pattern survey: an `Editor` change observer, a `theme::labeledRow` helper, merging the tool tables,
  declarative settings bindings; also a guard helper for `if (stroking()) return;` (about forty times in `Editor`), one holder
  for `Slot` and `Membership`, and a `Playback` State if a fourth mode is added.
- Cleanup pass: `History::perform` (record + run redo) replaced six hand-written copies; dead `History::grouping`,
  `Editor::duplicateLayer` and `commitLayerOpacity` removed; every function in `src/` is at most 12 cognitive complexity
  (`pxa` encode / decode, timeline grid, GIF palette, dialogs and shortcuts were split), and clang-tidy now fails a function
  above 15.

## Phase 2b: layer blend modes

A blend mode decides how a layer's color combines with what is under it. Today every layer is a plain
straight-alpha "over" scaled by its opacity (`composite_over`); a mode replaces the color mix and keeps the alpha
rules: Multiply darkens (pixel x backdrop), Screen lightens, Overlay does both by contrast, Add / Linear dodge sums,
Darken / Lighten keep the darker / lighter channel, Difference subtracts. Pixel artists use them for shading and
lighting layers (a Multiply shadow layer, an Add glow) without repainting the base.

Scope: Normal, Multiply, Screen, Overlay, Darken, Lighten, Add, Difference (W3C compositing formulas, straight alpha:
`Cs' = (1 - ab) * Cs + ab * B(Cb, Cs)`, then "over" with the layer alpha x opacity). Not in scope: layer groups,
per-cel modes, color tags, cel offsets.

Done (PR #8, plus the merge-down fix in PR #9). TDD, one step per commit; the UI part came last.

- [x] **15. Core model + compositing** — `BlendMode` enum and `Layer::blendMode()` / `setBlendMode()` in
      `layer.hpp`; `composite_blend(dst, src, opacity, mode)` in `composite.hpp` (Normal must equal `composite_over`);
      `composite_sprite` / `composite_pixel` use it per layer. Golden-pixel tests per mode (opaque, half alpha,
      transparent backdrop). Onion skin ghosts stay plain "over".
- [x] **16. Undoable edit** — `layer_ops` step for a mode change (as opacity), `Editor::setSelectedLayersBlendMode`
      applying to every selected layer in one step; duplicate keeps the mode; decide and test what "merge down" does
      (bake the upper layer's mode into the pixels: composite with its mode, result Normal). Merge is only
      allowed when it keeps the look (`layer_merge_keeps_look`): a blend sees every layer under it, a merge only
      the lower one, so it needs both layers Normal or no visible layer under the lower one.
- [x] **17. `.pxa`** — an optional `BLND` chunk after the layers (layer index + mode byte, only for layers that are
      not Normal), so version stays 1 and a file without blend modes is byte-identical; an unknown mode value reads
      as Normal. Round-trip and old-reader (skips the chunk) tests. Also check `recovery` (autosave) keeps modes.
- [x] **18. UI** — a "Blend" combo in the Layers panel's Properties section (applies to all selected layers, one
      undo step), a small marker on rows whose mode is not Normal, and the row tooltip names it. The exports (PNG,
      GIF, sheet) already go through `composite_sprite`, so they follow; add a test that an export differs by mode.

Gotchas to settle first: rounding of the 0..255 channel math (use the same rounding as `composite_over`), what the
canvas dirty-rect re-composite needs (nothing new: the mode is read per layer), and the Properties section now has
only Opacity, size and link info, so Blend is its first mode-like control.

## Phase 3: game pipeline

Ordered by value for a game developer and by risk (smallest, most self-contained first). Each is its own
branch and PR.

- [x] **19. Sheet metadata + headless CLI** — `spritesheet_metadata_json` (`meta` / `frames` / `tags`; tags are
      renumbered from the first exported frame and clamped to the range) and `spritesheet_export_with_json`;
      `tinysprite export <input> --sheet|--image|--gif out [--json f] [--layout] [--columns] [--spacing] [--scale]
      [--first] [--last] [--frame]` runs in `src/io/cli.cpp` before any SDL init, so it needs no display
      (exit 0 / 1 unreadable or unwritable / 2 bad command line). `--columns` alone means a grid.
- [ ] **20. Slices, pivots, hitboxes** — named rects with an optional pivot per sprite (`slice_ops.h`, `SLCE`
      chunk, JSON `slices`), a slice tool and panel.
- [ ] **21. Sheet packing** — trim transparent borders and pack frames (skyline), with the trim offsets in the JSON.
- [ ] **22. Filters** — outline, palette swap, hue / brightness, as undoable layer operations.
- [ ] **23. Tilemap editor** — tileset from a cel grid, tile layer, `.tmj`-like export.
- [ ] **24. Aseprite import** (optional).

Backlog, not scheduled (each is large or needs a design decision first): indexed color mode, several-rectangle
selection, a TTF font and HiDPI, a file browser, the OS reason in failed-write messages, the refactoring
candidates listed under UI polish.

## Decisions and gotchas
- Frames (phase 2): a `Layer` holds its cels as `shared_ptr<Cel>`, in frame order. `Sprite::insertFrame` /
  `detachFrame` / `moveFrame` keep every layer, the durations and the tags in step; a removed frame (or one added and
  then undone) is kept alive by the history step so pixel steps on its cels stay valid, like removed layers.
- `.pxa` stays version 1: `FRAM`, `LINK`, `BLND` and `TAGS` are optional chunks a reader of this version skips when it does not
  know them, and a plain one-frame sprite writes none of them (the file is byte-identical to before). The decoder grows
  the frames only after checking that the first `LAYR` chunk holds their pixels, so a lying header cannot make it
  allocate more than the file is big. `FRAM` has to precede the layers; `LINK`, `BLND` and `TAGS` come after them.
- Tags shift with their frames: insert before / inside, remove before / inside, a tag vanishes with its last frame.
  Moving a frame keeps tags by position. A frame step records the tag list before and after so undo brings back a tag
  that vanished.
- Linked cels are one `Cel` shared by several frames, so a stroke on one shows on the others and `History::forgetCel`
  and pixel deltas need no special case. Duplicating a frame copies instead of linking; duplicating a layer keeps its
  links; merging onto a layer whose cels are linked gives each frame its own cel for the merge (the upper layer differs
  per frame) and undo links them again.
- Playback ignores nothing: it stops on any press, frame edit, undo / redo or Esc, `editableCel` is null while it runs,
  and the onion skin is hidden meanwhile. A selected tag decides the range and direction, else all frames play in the
  play mode picked in the timeline.
- GIF: 256 colors max and 1-bit transparency (a pixel at least half opaque shows, else it is transparent); more colors
  go through a median cut. Delays are rounded to 1/100 s and at least 2/100 s. The encoder is ours (`src/io/gif.cpp`):
  stb has no GIF writer. Tests decode with `stbi_load_gif_from_memory`.
- The brush size is the outline width of lines and unfilled shapes (a filled shape ignores it); the bucket still ignores
  it. Pixel-perfect still applies to a 1 px pencil brush only.
- Undo restores the selection: `History::endStroke(HistoryAction)` runs the action with the stroke's undo / redo, which
  is how move, paste and rotate bring the selection rectangle back (and a move that changed no pixel is still a step).
- Palette edits (add, remove, reset, import) are undo steps (`palette_op_set`, a whole-palette snapshot). Only a
  document recovered from an autosave counts as modified outside the history now.
- The opacity slider changes every selected layer and records one step; Up / Down buttons move the selection by one
  place (a block stays a block).
- Rasterizers clip nothing; the caller (tool/history layer) must bounds-check plotted points.
- UI stack is SDL2 + Dear ImGui (native C++ API, `thirdparty/imgui`); the whole project is C++17 (it was C11 for core / io, with a C++ UI, until the cpp-migration).
- Undo is per stroke (not per pixel); only the internal storage is per pixel.
- Cel is RGBA now; the indexed-color mode from the original design is deferred.
- The clang-tidy hook (pre-push) lints only the .cpp files being pushed, so a changed header is not re-checked against
  the files that include it; CI lints everything (`pre-commit run --all-files --hook-stage manual`).
- clang-tidy runs with warnings-as-errors; explicit casts are required for multi-level pointer
  conversions such as `calloc`/`realloc`/`free` of `T **`.
- stb is compiled only in `src/io/stb_impl.cpp`, without warnings; the rest of `src/io` includes the headers.
- There is one build dir, `build/`: the UI is built whenever SDL2 is found (`TINYSPRITE_BUILD_UI` defaults to that), so
  the clang-tidy hook covers `src/ui` and `tests/ui` too. An old `build/` keeps a cached `OFF`: re-run cmake with `-DTINYSPRITE_BUILD_UI=ON`.
  CI builds the UI explicitly (`-DTINYSPRITE_BUILD_UI=ON`) and runs these tests.
- Zoom ranges 1/16x..64x (`VIEWPORT_ZOOM_MIN/MAX`); Fit picks a whole-number zoom, or a power of two below 1x for
  sprites larger than the canvas.
- `clang-format` / `clang-tidy` are pinned in `.pre-commit-config.yaml` (18.1.8, pip wheels used by CI and locally);
  a newer local tool let CI-only findings through (`bugprone-unchecked-optional-access` in #8). CI runs `lint`, `linux`
  and `windows` as separate jobs.
- `tinysprite_core` links `libm` (viewport uses `floorf`).
- The canvas edits `Editor.active_layer` / `Editor.frame` (frame stays 0 until phase 2).
- Live shape / move strokes redraw from scratch on every update (`LiveStroke::restart`), so
  previews never accumulate and the whole drag is one undo step.
- Layer add / remove / reorder / rename / visibility / opacity are undoable through `layer_ops.hpp`, which records
  them with `History::pushAction` (a step with its own undo / redo callables, destroyed with the step). A removed layer, or an
  added layer that was undone, is detached from the sprite and owned by its history step, so pixel steps on its
  cels stay valid and replay correctly; release callbacks must not dereference layers because steps are dropped
  oldest-first. `History::forgetCel` remains for callers that destroy a cel outside this path.
- The layer panel edits name / opacity live and records one step on widget release
  (`Editor::commitLayerName` / `commitSelectedLayersOpacity` record before -> current through `layer_op_record_*`).
- Pixel-perfect also cuts the corner of a 90-degree turn (the L-shaped corner pixel is removed), like Aseprite.
  A removed pixel is restored to the color it had before the stroke touched it.
- Symmetry (`symmetry.hpp`) mirrors across the cel's center and applies to pencil / eraser, shapes
  (`shape_stroke` / `ShapeStroke::begin` with a `ShapeStyle`) and bucket (`bucket_fill`).
- The selection is a single rectangle clipped to the sprite. Moving it loses pixels pushed off the sprite, and
  transparent selected pixels do not erase the destination. A click without dragging selects the one cel under
  it, unless the click only dismisses an existing selection.
- Tools ignore the press while the active layer is hidden (the eyedropper still samples the flattened image).
- `tests/ui/ui_editor_*.cpp` (ctest `ui_editor_{selection,layers,colors,drawing,document}`, shared helpers in
  `ui_editor_support.hpp`; only when the UI is built, run under `SDL_VIDEODRIVER=dummy`) cover `editor*.cpp` and `drags.cpp`; of `layers_panel.cpp` (ImGui widgets) only the in-place rename is covered (`ui_layers_panel`).
  CI builds the UI, so the `ui_*` tests run there too.
- Zoom and hovered cel are drawn as a canvas overlay (`draw_status`), after the canvas applied the frame's
  zoom / pan / hover, so they no longer lag the image by a frame.

## Convenience features (outside the milestones)
- Panning: the right or middle button drag (or Space + left drag) moves the view with a hand cursor; the canvas item accepts
  the right button for that, so a right click never draws (`tests/ui/ui_canvas.cpp`).
- Shortcuts live in `src/ui/app_shortcuts.cpp` (`App::plainShortcuts`): B E G I L R O M pick tools, X swaps colors, F fits,
  `+` / `-` and `[` / `]` brush size (1 px; `Editor::changeBrushSize`), the wheel zooms, holding the middle button = temporary eyedropper (`Editor::tempPick`; dragging it pans), Space + drag pans.
- Clipboard (`selection.hpp`): copy / cut / paste and selection flip / rotate. A rotated selection keeps its center and
  loses what leaves the sprite; undoing it restores the selection rectangle.
- `RecentColors` is not part of undo. Recent colors are recorded on press by every drawing tool except the eraser and
  the eyedropper.
- Brush (`PencilOptions.size` / `round`): square or round, 1..`PENCIL_BRUSH_MAX`; each stamp pixel is mirrored
  separately. Pixel-perfect is ignored above size 1. Shapes use it for their outline, the bucket does not.
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
  embedded yet). `tinysprite --frames N --screenshot out.png` saves the last frame, for checking the look without a
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
  Layers are reordered by dragging a row onto another (`moveLayer`, one undo step, the dragged layer stays active) or
  with the Up / Down buttons (`moveSelectedLayersBy`). While dragging, a 2 px off-white line across the hovered row marks where the layer
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
  edit that replaces the saved steps or `forgetCel` makes it permanently dirty). `Editor::modifiedOutsideHistory` (set by
  `markModified`, i.e. for a document recovered from an autosave) counts as unsaved until the next save. `App` shows `*` in the window title and asks Save / Discard / Cancel before
  Open, Quit and a dropped file replace the document.
- Opening files: `tinysprite [file | width [height]]`, `--help`, and `SDL_DROPFILE` (drop a .pxa, an image or an
  animated GIF on the window).
- Layer lock (`Layer::locked`, `layer_op_set_locked`, undoable): `Editor::editableCel` returns null for a locked layer,
  so tools, paste, cut and delete skip it; merge down refuses a locked lower layer. The model does not enforce it.
  Stored in bit 1 of the LAYR visible byte of `.pxa` (bit 0 is visible), so older readers still load the file.
- Selection clipping: `History::setClip` rejects `setPixel` outside a rect. `Editor::press` sets it to the selection
  for the drawing tools (not select / eyedropper) and lifts it on release / cancel. The bucket still scans the whole
  region but only writes inside the selection.
- Shift+click with the pencil / eraser draws a line from `Editor::lastDrawPoint` (end of the previous stroke; reset when
  the document is replaced). Shift is only read at the press, not while dragging.
- Shift while dragging a shape keeps even proportions (`shape_constrain` in `tools.hpp`: a square box, a circle, a line snapped to
  horizontal / vertical / 45 degrees); `Editor::drag(cx, cy, shift)` passes it to `Drag::setShift`.
- `image_export(..., scale)` enlarges by whole pixels (1..`IMAGE_EXPORT_MAX_SCALE`, at most `IMAGE_EXPORT_MAX_SIZE` px); the
  Export image dialog has a Scale field that is remembered. File > Open recent keeps ten `.pxa` paths (`recent_add`,
  `recent.N` keys), the New dialog has 16 / 32 / 64 / 128 / 256 presets and remembers its size, Ctrl + / - / 0 zoom.
- F1 / `?` opens the keyboard shortcut list (`App::helpWindow`, also in the Help menu).
- Dev packages are installed by the `Install tools` steps of `ci.yml` (and `sudo apt install libsdl2-dev pipx` locally,
  see the README); add a new apt dependency in both.

## Known limits (what is left)
- The model does not enforce layer locks; the `Editor` does (`editableCel`).
- Rasterizers clip no pixels; the caller bounds-checks. Filled ones take an optional clip rect that only skips fill rows and columns outside it (a shape dragged far past the cel stays cheap), and shapes draw once and reflect each plotted pixel for symmetry. A selection is one rectangle, and pixels moved off the sprite are
  lost. Merging needs both layers visible.
- Cels are RGBA only (indexed color is deferred); the palette holds at most 256 colors.
- No native file chooser (a path dialog instead); ImGui's built-in font (Inter / DM Mono are not embedded).
- The Windows (MinGW) job is not built locally.
- A GIF has no partial transparency and at most 256 colors; the sprite sheet is a plain grid / strip (no packing or JSON
  yet, that is phase 3).
