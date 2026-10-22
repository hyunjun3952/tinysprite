# Milestones

Phase 1 = drawing-tool core (MVP). Animation is phase 2, game pipeline is phase 3.
Every step is TDD: failing test first, then implementation, then refactor.
Rules: `CLAUDE.md`. Design map: [ARCHITECTURE.md](ARCHITECTURE.md).

## Phase 1

Done. The design is described in [ARCHITECTURE.md](ARCHITECTURE.md); the gotchas that came out of it are below.

- [x] **1. Project foundation** — git, CMake + CTest, ASan/UBSan, `minitest.hpp`, smoke test.
- [x] **1b. Format / lint automation** — `.clang-format` / `.clang-tidy`, `.pre-commit-config.yaml` (clang-format on commit, a fast clang-tidy tier on push, the full set in CI), GitHub Actions CI.
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
- Not done: HiDPI window flags, the OS reason for image exports (only `.pxa` saves show it, via `file_write_error`).
- Left from the pattern survey: an `Editor` change observer, a `theme::labeledRow` helper, merging the tool tables,
  declarative settings bindings; also a guard helper for `if (stroking()) return;` (about forty times in `Editor`), one holder
  for `Slot` and `Membership`, and a `Playback` State if a fourth mode is added.
- Cleanup pass: `History::perform` (record + run redo) replaced six hand-written copies; dead `History::grouping`,
  `Editor::duplicateLayer` and `commitLayerOpacity` removed; every function in `src/` is at most 12 cognitive complexity
  (`pxa` encode / decode, timeline grid, GIF palette, dialogs and shortcuts were split), and clang-tidy now fails a function
  above 15.

## UI polish 2 (ideas from Aseprite's ui library, MIT; the app's own skin is not reused)

- Icons are Material Symbols Outlined glyphs (Apache 2.0, a 4 KB subset in `thirdparty/material-symbols/`) merged into
  the UI font, so they scale with the UI, take any color and are added by one codepoint (`icons::codepoint`). Each is
  centered by the ink of its glyph (`icons::glyph_origin`), not its em box. The diamond is drawn with lines because the
  set has no plain rhombus. A first version used 16x16 pixel bitmaps drawn at a whole-number scale (Aseprite's
  `guiscale()` idea); next to the Material set the tools were harder to read (eraser, bucket, eyedropper), so they were
  replaced.
- The five main tools are icon buttons in one row (name, shortcut and help in the tooltip, like Aseprite's tool bar),
  the same helper (`ToolsPanel::iconRow`) as the shape row; the Tools panel is one row shorter.
- An embedded font: Geist Regular, a 23 KB subset (`thirdparty/geist/`, SIL OFL, `theme::load_fonts`, 14 px base)
  instead of the 13 px bitmap font. stb_truetype does not hint, so the glyphs are pixel-snapped with no oversampling:
  compared with Inter at default settings the letter spacing and stems are even (Inter Regular was the first choice,
  and looked soft at 14 px). Aseprite keeps font choice in its theme data; we keep one font and a constant.
- A neutral near-black look (surface ladder `#0f1011` / `#1d1e21` / `#26282c` / `#2f3136`, hairline `#2a2c31`, 6-8 px
  corners, no borders on controls, one off-white accent `#e8eaed` for what is selected or focused, `theme::ACCENT`). The
  palette follows the principles of the Linear DESIGN.md analysis (VoltAgent/awesome-design-md, MIT; a description of
  the marketing site, not the app): a surface ladder instead of shadows, a scarce accent, 4 px spacing grid.
  Figma's analysis (white canvas, pastel blocks, pills) was not used.
- Menus: a Layer menu (the Layers panel's actions, `Editor::canRemoveSelectedLayers` is shared with the panel's
  Delete button), File groups Export and Palette into submenus, Edit gains Deselect / Clear / Transform selection,
  View is split into zoom, canvas overlays and panels.
- Docks: section headers are a hairline plus a caret and title instead of filled bars. Layout, chosen after
  prototyping three variants side by side (tidy the old structure / move Color to the right / Aseprite's context bar):
  a 52 px vertical tool bar on the left (tools and shapes, no headers), an options bar above the canvas (brush, shape,
  selection transforms, mirror X / Y; View > Tool options hides it), and a right panel "Inspector" with Color (and the
  recent colors under the palette), Layers (no buttons: a right-click menu, shared with the Layer menu, and key
  chords; the list fills the free height) and Properties (a two-column grid: blend, opacity, then size, frames,
  palette) pinned to the bottom. The tool bar has one width; the Inspector can be resized between 250 and 480 px. The sections Tool options,
  Symmetry, Selection and Recent no longer exist as dock sections.
- The timeline works like a VS Code panel: drag its top edge to resize it (`timeline_drag`: between 96 px and 70% of
  the canvas area), drag it below 60 px to fold it away, and drag the top of the status bar up to bring it back; a
  double click on the edge restores the default height; Ctrl+T (a `CommandId`, so the View menu item and the key cannot
  drift) does the same as folding. `view.timeline`, `view.timeline_height` and `view.options_bar` are saved. (A first
  version had a small tab on the edge; the drag edge replaced it. Gotchas: ImGui windows are never smaller than 32 px
  unless `WindowMinSize` is lowered, and a window that appears under the mouse takes the focus and ends a drag.)
- Layer actions have key chords as `CommandId`s (Ctrl+Shift+N new, Ctrl+Shift+D duplicate, Ctrl+Shift+M merge down,
  Ctrl+] / Ctrl+[ move up / down; Delete with the list focused deletes); a right click on a row selects it (unless it
  is already selected) and opens the menu. `New...` is now Ctrl+N without Shift.
- Numbers show their unit (`theme::int_format` / `theme::input_int`): `100 ms`, `16 px`, `2×`, `1 frame`, and the status
  bar says `48×48 px`.
- Menu bar titles open when the mouse rests on them (`theme::begin_menu`: ImGui only opens a title on a click; submenus
  already open on hover).
- A double click on a docked panel's inner edge restores its default width (`Panel::defaultWidth`), like the timeline's
  edge; the tool bar has a fixed width so it has no edge to drag.
- The status bar is a row of items (`StatusItem`: tool, frame, layer, zoom, selection, cel; the size is in Properties) kept in
  `Editor::Overlays::status_items` (saved as `view.status_items`). A right click on an item offers "Hide <item>"; a
  right click anywhere on the bar opens a menu that switches every item and hides the bar (View > Status bar shows it
  again).
- Dialogs: the popup color is the panel color, not the control color, so a dialog's buttons and fields are visible (they
  had the same fill as the dialog and vanished). `theme::dialog_buttons` puts the buttons under a hairline,
  right-aligned, 88 px at least with a gap, the main one in the accent (red for Delete / Overwrite / Reset); six
  dialogs use it.
- Units stay while a number is typed: the unit is drawn over the right end of the field (before the - / + buttons), not
  made part of the text. The color picker's RGB / HSV / Hex row is replaced by `theme::hex_field` (a preview, "#" and
  RGB / RRGGBB / RRGGBBAA, checked on Enter).
- A round button (`icons::round_button`) for small secondary actions; the foreground / background swap uses it.
- Looked at and not taken: `PopupWindow` hot regions and `AnimatedWidget` easing (ImGui popups already close on an
  outside click, and the UI is immediate-mode), a separate "small" font (one more atlas size for little gain now).

- Motion, kept small: a toast fades in while rising 8 px (`Toasts::rise`, `ENTER_SECONDS`), and the tool-bar buttons and the
  layer eye / lock toggles ease between resting, hovered and selected (`theme::animate` keeps one 0..1 value per widget
  id in the window's state storage, `theme::ease_toward` / `theme::mix` are the pure parts). The app redraws every frame,
  so no wake-up logic is needed. Modal dialogs fade in (`theme::begin_modal` / `end_modal` push the popup's alpha; every
  dialog asks each frame, so the stored value is 0 again before it opens). Not animated on purpose: menus, panel docking,
  the timeline fold.
- File dialog: the path field holds the whole path (`~` is the home folder; it starts with the folder and the document's
  name). A folder button after it shows or hides the browser (`app/file_browser_ui.cpp`): a search field and a folder
  tree rooted at the path's folder (`file_browser.hpp`). A folder's name picks it as the place to save to (its path goes
  in front of the file name, which stays as typed; the tree keeps its root), its arrow opens it to show what is inside
  (listed once, cached while the dialog is open), `../` moves the root up, a file click puts it in the path field and a
  double click on a file in a dialog that reads opens it. The search looks in the root and up to 6 levels below (300
  hits, relative paths). Rows are taller (`App::TREE_ROW_PADDING`; a divider line between rows was tried and dropped); "Expand all" opens every
  folder (4 levels, 400 folders at most, cached listings) and "Collapse all" closes them and forgets their open state.
  The files shown are the action's kinds (`FileActionInfo::filter`); hidden names are left out.
- Tree arrows and section carets are unfilled chevrons (`theme::tree_node` hides ImGui's filled triangle and draws
  `theme::chevron` instead).
- Menus: a menu bar title opens on hover and closes when the mouse has been away from the bar and the open popups for
  0.25 s (`theme::begin_menu`; the bar rectangle is taken before `BeginMenu`, since after it the current window is the
  popup); a click on an open title closes it and the hover does not reopen it until the mouse
  leaves the title (it used to blink: ImGui toggled it closed, the hover opened it again).
- Narrow windows: the window opens inside the display's usable area with a minimum size (`window_size.hpp`), docked panels
  are drawn narrower (`Panels::fitWidths`, the chosen width is kept) so the canvas keeps `PANELS_CANVAS_MIN`, and the
  timeline scrolls sideways. The options bar has no scrollbar (it is cut off; Shift + wheel scrolls it).

## Document tabs and split view

Several documents open at once, as tabs, and two of them side by side. Done on `feat/doc-tabs`.

- Every document has its own `Editor` (history, selection, view, texture): `App::docs_` holds `Document { editor, path,
  serial }`, `App::active_` is the shown one and `App::editor_` (a non-owning pointer) is its editor, so the menus and
  shortcuts keep saying `editor_->`. The panels and the canvas hold an `Editor *` too and are pointed at another one by
  `setEditor` (which drops what they remember about the old document: thumbnails, a rename in progress, the timeline's
  filled-cel cache). New and Open make a tab (an untouched untitled tab is reused by Open, a file that is open is just
  shown); the unsaved-changes question moved from New / Open to closing a tab and to quitting (`App::quit` asks about
  every tab with changes, one after the other, Cancel stops it).
- What the user carries from tab to tab (tool, colors, brush and shape options, overlays, symmetry and the clipboard) is
  copied into the editor that is activated: `Editor::adoptPreferences` (built on `store` / `load`). That is also how
  a selection copied in one tab pastes into another.
- The tab bar is ImGui's (`app/tabs.cpp`): reorderable (ImGui reorders its own list and `syncTabOrder` brings `docs_`
  into the same order), a dot on a tab with unsaved changes, a close button, a middle click closes, a right click offers
  Close / Split right / Split down / Close others / Close to the right, and `+` opens the New dialog. Ctrl+W closes the
  tab, Ctrl+Tab / Ctrl+Shift+Tab (or Ctrl+PageDown / PageUp) switch. ImGui's selection is only believed when the app did
  not change the active tab in the same frame (`tabSelected_`).
- Split view (`app/split.cpp`): View > Split right / Split down / Close split (or the tab menu) show a second `Canvas`
  ("Canvas 2") beside or under the first, with a draggable handle (ratio 0.15 to 0.85, double click to reset). A click in
  a pane makes its document the active one (`Canvas::setOnPress`; the active pane has a line along its top), picking a
  tab fills the active pane, and a tab that is in the other pane just takes the focus there. Closing a tab that a pane
  shows ends the split. The timeline, options bar, status bar and the Inspector follow the active document.
- Autosave keeps one copy per document with unsaved changes (`recovery_<serial>.pxa` / `.path`, `Document::recovery`); a
  copy goes when its document is clean, saved or closed, and the recovery dialog offers all that were left (an old single
  `recovery.pxa` too). A recovered copy whose file is open in another tab becomes untitled, and Save As onto a file that
  another tab has is refused, so two tabs never share a file.
- Closing a tab destroys its editor (and the layer thumbnails) a frame later (`App::retired_`, `Editor::retiredSurface_`,
  `LayerThumbnails::graveyard_`): the frame being built may already have drawn their textures, which are rendered at its
  end. A tab cannot be closed or left while a tool is being dragged (quit cancels the drag first), and a middle-button
  pick ends when the tab changes.
- Known limits: the split is not saved between runs; playback runs only in the active document.

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
- [x] **20. Slices, pivots, hitboxes** — named rects with an optional pivot per sprite: `Sprite::Slice` and
      `slice_ops.hpp` (undoable add / set / remove), the optional `SLCE` chunk of `.pxa` (name, rectangle, flags, pivot;
      read back checked against the sprite size), a `slices` list in the sheet JSON (`name`, `bounds`, optional `pivot`,
      scaled by the export scale), and in the UI the Slice tool (K: drag to mark one, drag a slice to move it, a click on
      empty canvas deselects), outlines with names and pivots on the canvas (View > Slices), and a Slices section in
      the Inspector (list, New / Delete, name, X / Y / Width / Height, pivot; every change one undo step).
- [x] **21. Sheet packing** — `SheetLayout::Packed`: every frame is trimmed to its drawn pixels (an empty frame keeps a
      1 x 1 transparent cell) and the pieces are packed by `pack_rects` (skyline bottom-left, tall first, the sheet
      width tried from about the square root of the area upward for the smallest result; `spacing` is the gap between
      pieces). `SpriteSheet::trims` / `trimmed` and, in the JSON, `"trimmed": true`, `spriteSourceSize` and `sourceSize`
      per frame (scaled with the sheet; slices stay in whole-frame coordinates). `tinysprite export --layout packed`, a
      "Packed (trimmed)" entry in the sprite sheet dialog, and a checkbox there that also writes the `.json` beside the
      image (`with_replaced_extension`).
- [ ] **22. Filters** — outline, palette swap, hue / brightness, as undoable layer operations.
- [ ] **23. Tilemap editor** — tileset from a cel grid, tile layer, `.tmj`-like export.
- [ ] **24. Aseprite import** (optional).

Backlog, not scheduled (each is large or needs a design decision first): indexed color mode, several-rectangle
selection, a TTF font and HiDPI, the OS reason in failed-write messages, the refactoring
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
- Shortcuts live in `src/ui/app/shortcuts.cpp` (`App::plainShortcuts`): B E G I L R O M pick tools, X swaps colors, F fits,
  `+` / `-` and `[` / `]` brush size (1 px; `Editor::changeBrushSize`), the wheel zooms, holding the middle button = temporary eyedropper (`Editor::tempPick`; dragging it pans), Space + drag pans.
- Clipboard (`selection.hpp`): copy / cut / paste and selection flip / rotate. A rotated selection keeps its center and
  loses what leaves the sprite; undoing it restores the selection rectangle.
- `RecentColors` is not part of undo. Recent colors are recorded on press by every drawing tool except the eraser and
  the eyedropper.
- Brush (`PencilOptions.size` / `round`): square or round, 1..`PENCIL_BRUSH_MAX`; each stamp pixel is mirrored
  separately. Pixel-perfect is ignored above size 1. Shapes use it for their outline, the bucket does not.
- `layer_op_merge_down` snapshots the lower layer's cels before / after (whole-cel memcpy) instead of pixel
  deltas, so earlier pixel steps on either layer stay valid after undo. Both layers must be visible.
- UI layout (`src/ui/panels.cpp`): Tools (the tool bar) / Layers (titled Inspector) are docked left / right (resizable by the inner edge, 48-480 px) or
  floating windows; dragging a docked panel's title bar pulls it out (it follows the mouse until release), and
  dragging a floating title bar to an edge docks it. The layout is persisted (`Panels::store` / `load`). The canvas refits
  on resize until the user zooms or pans (`Editor.auto_fit`).
- Look (`src/ui/theme.cpp`): a neutral near-black theme (see "UI polish 2": surface ladder, controls told apart by
  their fill, 1 px hairline dividers, 6-8 px corners, no shadows). The accent `theme::ACCENT` marks the selected tool
  (`theme::choiceButton`, `icons::choice`), the checkmark and the layer drop line. Sliders (`theme::slider_int`) are a field filled up to the value with the value centered on it, combos (`theme::begin_combo`) have a drawn chevron. In the palette, Add is disabled for a color it already has, Replace gives the selected swatch the foreground color, and Reset is disabled for the default palette. Tools and shapes are icon buttons (the
  shortcut is in the tooltip), the other buttons are text, grouped into collapsible sections (`theme::section`) in the
  Tools and Layers panels; the icons are font glyphs
  (`src/ui/icons.cpp`; `tests/ui/ui_icons.cpp` checks the table and that every glyph is in the font). The
  The options bar shows only what the active tool uses. The font is an embedded Geist subset (UI polish 2); there is no mono font. `tinysprite --frames N --screenshot out.png` saves the last frame, for checking the look without a
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
  with the Up / Down buttons (`moveSelectedLayersBy`). While dragging, a 2 px accent line across the hovered row marks where the layer
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
- No native file chooser (a path dialog instead); one UI font (a Geist subset), no mono font and no CJK glyphs.
- The Windows (MinGW) job is not built locally.
- A GIF has no partial transparency and at most 256 colors; the sprite sheet is a plain grid / strip (no packing or JSON
  yet, that is phase 3).
