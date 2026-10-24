# Milestones

Phase 1 is the drawing-tool core (MVP), phase 2 animation, phase 3 the game pipeline. All three are done; what is left is
the backlog at the end. Rules: `.rules`. How the code is put together: [ARCHITECTURE.md](ARCHITECTURE.md). How it looks:
[../DESIGN.md](../DESIGN.md).

## Milestones

### Phase 1: drawing tools

- [x] **1. Foundation**: git, CMake + CTest, ASan/UBSan, `minitest.hpp`, a smoke test. **1b.** Format and lint automation:
      `.clang-format` / `.clang-tidy`, `.pre-commit-config.yaml` (clang-format on commit, a fast clang-tidy tier on push,
      the full set in CI) and GitHub Actions.
- [x] **2. Core data model**: `Sprite` / `Layer` / `Cel` / `Palette`; a sprite starts with one frame and one layer.
- [x] **3. Rasterizer**: line, rectangle, ellipse, polygon, flood fill, tested against golden pixel arrays.
- [x] **4. Pencil / eraser + History**: one undo step per stroke, stored as pixel deltas.
- [x] **5. App + canvas**: SDL + ImGui, CPU-composited canvas, zoom / pan, grid, checkerboard, `Viewport`, live strokes.
      `tinysprite --frames N` exits after N frames (smoke runs under `SDL_VIDEODRIVER=dummy`).
- [x] **6. Remaining tools and layer operations**: bucket, eyedropper, shapes, selection / move, pixel-perfect, symmetry,
      undoable layer operations (`layer_ops.hpp`), the layer panel.
- [x] **7. File I/O**: `.pxa` (versioned, chunked), image export / import through stb, a File menu with a path dialog (there
      is no native file chooser).
- [x] **8. Polish**: palette files (`.gpl` / `.hex`), `settings.ini`, autosave recovery, shortcuts, Linux and Windows (MinGW)
      builds in CI.

### Phase 2: animation

- [x] **9. Frames**: `Sprite` inserts, detaches and moves frames, each with a duration; `frame_ops.hpp`; the optional
      `FRAM` chunk.
- [x] **10. Timeline and playback**: a current frame and a `Playback` advanced by `tick()`; a strip with a cel grid, frame
      buttons, duration and play mode; `,` `.` `Enter` and a Frame menu.
- [x] **11. Onion skin**: `composite_onion` draws the neighbouring frames tinted and fading under the current one.
- [x] **12. Tags**: named frame ranges with a direction (`tag_ops.hpp`, `TAGS`); a selected tag limits playback.
- [x] **13. Linked cels**: frames of a layer share one cel (`cel_ops.hpp`, `LINK`).
- [x] **14. Export**: `gif_encode` (our own LZW encoder) and `spritesheet_render`, with File menu dialogs; animated GIF import.
- [x] **15-18. Layer blend modes** (Normal, Multiply, Screen, Overlay, Darken, Lighten, Add, Difference; W3C formulas on
      straight alpha, `Cs' = (1 - ab) * Cs + ab * B(Cb, Cs)`, then "over"): `composite_blend`, an undoable mode change on
      every selected layer, the optional `BLND` chunk, and a Blend combo in Properties. Not in scope: layer groups,
      per-cel modes, color tags, cel offsets.

### Phase 3: game pipeline

- [x] **19. Sheet metadata + headless CLI**: `spritesheet_metadata_json` and `tinysprite export <input>
      --sheet|--image|--gif|--tilemap out [...]`, which needs no display (exit 0; 1 unreadable or unwritable; 2 bad command
      line). `--columns` alone means a grid.
- [x] **20. Slices, pivots, hitboxes**: `Sprite::Slice`, `slice_ops.hpp`, the optional `SLCE` chunk, a `slices` list in the
      sheet JSON, the Slice tool (K), outlines on the canvas and a Slices section in the Inspector.
- [x] **21. Sheet packing**: `SheetLayout::Packed` trims every frame (an empty one keeps a 1 x 1 cell) and packs the pieces
      with `pack_rects` (skyline bottom-left, tall first, the width tried from about the square root of the area upward);
      `"trimmed"`, `spriteSourceSize` and `sourceSize` in the JSON; a checkbox that also writes the `.json`.
- [x] **22. Filters**: `filter_outline` (one pixel wide, read from a copy), `filter_replace_color`,
      `filter_hue_brightness` (HSL, alpha kept); `filter_apply_cels` runs one over several cels as a single undo step (a
      `History` group cannot hold strokes, so it records one action); a Filter menu with a dialog per filter.
- [x] **23. Tilemap editor**: `Tileset` (an atlas cut from a cel into equal tiles) and `TileMap` (cells: 0 empty, else tile
      + 1) on the `Sprite`, `tilemap_ops.hpp`, `TileStroke` for painting, optional `TSET` / `TMAP` chunks (tile data is kept
      all or nothing), a Tiled-style `.tmj` export plus one PNG per tileset, a Tilemaps section and a floating Tilemap
      window. Tiles are not flipped or rotated, a tilemap is not drawn on the canvas or in the exports, and a tileset is a
      copy of the pixels.
- [x] **24. Aseprite import**: `aseprite_decode` / `aseprite_load`: image layers (name, visibility, lock, opacity, the blend
      modes this editor has), cels (offset, opacity baked into alpha, links), RGBA / grayscale / indexed color, durations,
      tags, palette and slices. Compressed cels go through our own `zlib_inflate` with an output limit, so a hostile file
      cannot allocate without bound. Not imported: tilemap layers, extra slice keys, user data, frames past
      `SPRITE_MAX_FRAMES`. Writing Aseprite files is not planned.

## The UI, after phase 2

What exists and why. The look itself is in `DESIGN.md`; the structure is in `ARCHITECTURE.md`.

- **Tables, not switches**: `App::fileInfo` (one row per `FileAction`), `App::commandAction` (one per `CommandId`), the
  tool and shape tables and the extension tables in `src/io` (`image_write_formats` and `palette_formats` feed the error
  texts).
- **Icons** are Material Symbols glyphs (a 4 KB subset) merged into the UI font, so they scale, take any color and are
  added by one codepoint. Each is centered by the ink of its glyph and drawn at the size that gives its ink the same
  length (`icons::fitted_glyph_size`). The diamond, the folder and the slice are drawn with lines (not in the set). 16 x 16
  bitmaps were tried first and read worse next to the Material set.
- **Font**: Geist Regular, a 23 KB subset, 14 px. stb_truetype does not hint, so glyphs are pixel-snapped with no
  oversampling (Inter looked soft at 14 px).
- **Layout**: a 52 px tool bar, an options bar above the canvas, and an Inspector (Color with the recent colors, Layers,
  Slices, Tilemaps, Properties) that can be resized from 250 to 480 px. The Layers section has no buttons: a right-click
  menu shared with the Layer menu, and key chords (Ctrl+Shift+N / D / M, Ctrl+] / Ctrl+[; Delete with the list focused
  asks first).
- **Timeline**: like a VS Code panel: drag its top edge to resize (96 px to 70% of the canvas area), below 60 px it folds,
  a double click on the edge resets it, Ctrl+T toggles it; the height is saved. Gotchas: ImGui windows are never smaller
  than 32 px unless `WindowMinSize` is lowered, and a window that appears under the mouse takes the focus and ends a drag.
- **Numbers show their unit** (`theme::int_format` / `input_int`: `100 ms`, `16 px`); the unit is drawn over the right end
  of the field, so it stays while a number is typed. The color picker has one hex field (`theme::hex_field`).
- **Menus**: a menu bar title opens when the mouse rests on it and closes after 0.25 s away from the bar and the open
  popups (`theme::begin_menu`); a click on an open title closes it and the hover does not reopen it until the mouse leaves
  (it used to blink).
- **Status bar**: items (`StatusItem`: tool, frame, layer, zoom, selection, cel) kept in `Editor::Overlays::status_items`;
  a right click on an item offers to hide it, a right click on the bar opens the menu that switches every item.
- **Dialogs**: the popup color is the panel color, so a dialog's buttons and fields show; `theme::dialog_buttons` puts the
  buttons under a hairline, the main one in the accent (red for Delete / Overwrite / Reset).
- **Motion, kept small**: a toast fades in while rising 8 px, tool buttons and the layer eye / lock ease between states
  (`theme::animate`), modal dialogs fade in. Not animated on purpose: menus, panel docking, the timeline fold. The app
  redraws every frame, so no wake-up logic is needed.
- **File dialog**: the path field holds the whole path (`~` is home); a folder button shows a search field and a folder
  tree (`app/file_browser_ui.cpp`): a folder's name picks it as the place to save to, its arrow opens it, a double click on
  a file in a dialog that reads opens it. The search goes 6 levels deep (300 hits); "Expand all" opens up to 4 levels (400
  folders). Hidden names are left out.
- **Narrow windows**: the window opens inside the display's usable area with a minimum size (`settings.hpp`); docked
  panels are drawn narrower so the canvas keeps `PANELS_CANVAS_MIN`; the timeline scrolls sideways; the options bar has no
  scrollbar but marks a cut with a chevron (click it, or Shift + wheel, to scroll). A saved or detected UI scale is capped
  to what the display can hold (`window_max_scale`); an explicit `--scale` is taken as given.
- **Tabs and split view** (`app/tabs.cpp`, `app/split.cpp`): every document has its own `Editor`; what the user carries
  from tab to tab (tool, colors, options, overlays, symmetry, clipboard) is copied by `Editor::adoptPreferences`. Tabs
  reorder, show a dot for unsaved changes, close on a middle click, and have a right-click menu (Close / Split right / Split
  down / Close others / Close to the right); Ctrl+W, Ctrl+Tab. A split shows a second `Canvas` beside or under the first
  (ratio 0.15 to 0.85); a click in a pane makes its document active; an animation plays in every pane that is shown. Closing
  a tab destroys its editor a frame later (`App::retired_`), and a tab cannot be closed while a tool is being dragged.
- **Autosave** keeps one copy per document with unsaved changes (`recovery_<serial>.pxa` / `.path`); a recovered copy
  whose file is open in another tab becomes untitled, and Save As onto a file another tab has is refused.
- **Layer selection**: a click selects one layer, Ctrl+click adds or removes one (the last one stays), Shift+click selects a
  range; the selection is tracked by layer identity. Delete, duplicate, merge, dragging, the toggles and the opacity and
  blend controls act on all selected layers as one undo step (`History::beginGroup`). Merging several layers draws them
  onto the lowest one (selected layers are first moved next to each other, then merged down topmost first). Dragging a row
  moves the selection as a block; a 2 px accent line marks where it will land.
- **Input**: panning with the right or middle button or Space + drag (a right click never draws); a held middle button is a
  temporary eyedropper (`Editor::tempPick`) unless the mouse moves, which pans; B E G I L R O M pick tools, X swaps colors,
  F fits, `+` `-` `[` `]` change the brush size, F1 / `?` list the shortcuts. Shift + click draws a line from the last
  stroke; Shift while dragging a shape keeps it even (`shape_constrain`).
- **Brush**: square or round, 1 to `PENCIL_BRUSH_MAX`; each stamp pixel is mirrored separately. The size is also the
  outline width of lines and unfilled shapes. Pixel-perfect also cuts the corner of a 90-degree turn and applies to a 1 px
  brush only.
- **Selection**: one rectangle clipped to the sprite; moving it loses what leaves the sprite, and transparent selected
  pixels do not erase the destination. The drawing tools only write inside it (`History::setClip`; the bucket still scans
  the whole region but writes only inside). Undo restores it: `History::endStroke(HistoryAction)` runs an action with the
  stroke's undo / redo, which is how move, paste and rotate bring the rectangle back.
- Shortcut `Delete` clears the selected pixels; copy / cut / paste, flip and rotate are in `selection.hpp`. Recent colors
  are not part of undo and are recorded on press by every drawing tool except the eraser and the eyedropper.

## Decisions and gotchas

**Model and undo**
- A `Layer` holds its cels as `shared_ptr<Cel>`; `Sprite::insertFrame` / `detachFrame` / `moveFrame` keep every layer, the
  durations and the tags in step. Tags shift with their frames (insert or remove before or inside, a tag vanishes with its
  last frame; moving a frame leaves them); a frame step records the tag list before and after so undo brings back a tag
  that vanished.
- Linked cels are one `Cel` shared by several frames, so a stroke shows on all of them and `History::forgetCel` and pixel
  deltas need no special case. Duplicating a frame copies instead of linking; duplicating a layer keeps its links; merging
  onto a layer with linked cels gives each frame its own cel for the merge and undo links them again.
- A removed layer or frame lives on in its history step so pixel steps on its cels stay valid. Release callbacks must not
  dereference layers because steps are dropped oldest-first. A refused `Sprite::insertFrame` leaves the frame it was given
  untouched (an rvalue reference), because `Slot::attach` tries again with it.
- `layer_op_merge_down` snapshots the lower layer's cels before and after (whole-cel copies) instead of pixel deltas, so
  earlier steps on either layer stay valid after undo. It needs both layers visible, the lower one unlocked, and a stack
  that looks the same afterwards (`layer_can_merge_down`): a blend sees every layer under it, a merge only the lower one.
- Undo is per stroke; only the internal storage is per pixel. Live shape and move strokes redraw from scratch on every
  update (`LiveStroke::restart`), so previews never accumulate.
- Layer lock (`Layer::locked`, undoable): `Editor::editableCel` is null for a locked, hidden layer or while playing, so
  tools, paste, cut and delete skip it. The model does not enforce it. Tools also ignore a press on a hidden layer (the
  eyedropper still samples the flattened image).
- The layer panel edits name and opacity live and records one step on release (`layer_op_record_*`). Palette edits are undo
  steps (`palette_op_set`, a snapshot). Only a document recovered from an autosave counts as modified outside the history.
- Playback stops on any press, frame edit, undo / redo or Esc, the onion skin is hidden while it runs, and a selected tag
  decides the range and direction. A document stops playing when it is no longer shown in a pane.

**Files and formats**
- `.pxa` stays version 1: `FRAM`, `LINK`, `BLND`, `TAGS`, `SLCE`, `TSET` and `TMAP` are optional chunks a reader skips when it
  does not know them, and a plain one-frame sprite writes none of them. The decoder grows the frames only after checking
  that the first `LAYR` chunk holds their pixels, so a lying header cannot make it allocate more than the file is big.
  `FRAM` has to precede the layers. The lock is bit 1 of the `LAYR` flags byte (bit 0 is visible), so older readers still
  load the file.
- GIF: 256 colors and 1-bit transparency (a pixel at least half opaque shows); more colors go through a median cut. Delays
  are rounded to 1/100 s and are at least 2/100 s. stb has no GIF writer, so the encoder is ours; tests decode with
  `stbi_load_gif_from_memory`. Importing a GIF whose frames alone exceed the cel budget is refused before stb decodes them.
- Paths are UTF-8 everywhere and opened through `path_from_utf8`; stb gets bytes, not a path (on Windows its `fopen` reads
  the ANSI code page).
- Zoom ranges 1/16x to 64x; Fit picks a whole-number zoom, or a power of two below 1x for sprites larger than the canvas.

**Build and tooling**
- The libraries come from vcpkg (`vcpkg.json`, pinned by its baseline); the UI is built whenever SDL3 is found. stb is
  compiled only in `src/io/stb_impl.cpp`, without warnings. `tinysprite_core` links `libm`.
- clang-tidy runs with warnings-as-errors; the cognitive complexity limit is 15 for `src/` (not for tests, where `ASSERT_*`
  expands to branches). The pre-push hook lints only the `.cpp` files being pushed, so a changed header is re-checked by CI,
  which lints everything. `clang-format` and `clang-tidy` are pinned in `.pre-commit-config.yaml` (pip wheels), because a
  newer local tool let CI-only findings through. CI runs `lint`, `linux` and `windows` as separate jobs.
- `Panels::end` only calls `ImGui::End` if `begin` started a window (a hidden panel used to trip "Calling End() too many
  times!"; `ui_panels.cpp` covers it).
- UI tests count every frame as 1/60 s (the harness sets `io.DeltaTime`), so a test of a timed gesture does not depend on
  how fast the machine is.

## Maintenance since milestone 24

- A UI audit (headless): icon ink lengths made equal, the UI scale capped to the display, a chevron on a cut-off options
  bar, readable shortcut hints while pressed, and the tile picker colors moved to `theme::overlay`.
- An audit of the comments against the code found real bugs, now fixed: a refused `insertFrame` took the frame's cels,
  `tilemap_render` refused a map whose tileset slot was not 0, `image_import` could not open a Korean file name on
  Windows, and removed layers and frames weighed 0 bytes in the history limit.
- The animated GIF import is bounded in memory, saving no longer copies the whole file, and an animation plays in both panes
  of a split.
- The dependencies moved from vendored copies to vcpkg, and the UI from SDL2 to SDL3.

## Known limits

- The model does not enforce layer locks; the `Editor` does.
- Rasterizers clip no pixels; the caller bounds-checks (filled ones take an optional clip rect that only skips fill rows
  and columns outside it). A selection is one rectangle.
- Cels are RGBA only (indexed color is deferred); the palette holds at most 256 colors.
- No native file chooser (a path dialog instead); one UI font (a Geist subset), no mono font and no CJK glyphs.
- The split and the open tabs are not saved between runs (the autosave recovery covers unsaved work).
- Not done: HiDPI window flags (the UI scale follows the display's content scale instead).
- The Windows (MinGW) job is not built locally.
- A GIF has no partial transparency and at most 256 colors (the format's limit).

## Backlog, not scheduled

Each is large or needs a design decision first: indexed color mode, a several-rectangle selection, a native file chooser,
Korean text. Smaller refactoring candidates from the pattern survey: an `Editor` change observer, a `theme::labeledRow`
helper, merging the tool tables, declarative settings bindings, one holder for `Slot` (frames) and `Membership` (layers),
and a `Playback` state if a fourth mode is added. The `if (stroking()) return;` guard (about forty times in `Editor`) has
no helper: a function cannot return for its caller, so only a macro would shorten it; reconsider if `Editor` gets a command
dispatch that can refuse in one place.
