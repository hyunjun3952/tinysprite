# Architecture

How tinysprite is put together and why. `.rules` has the rules to follow and `MILESTONES.md` the progress and the
gotchas; this file is the map.

## Layers

```
tinysprite_core   src/core  +  include/tinysprite/   document model, rasterizer, tools, history, compositing
      ^
tinysprite_io     src/io                              .pxa, palette files, image import / export (stb)
      ^
tinysprite_ui     src/ui                              SDL3 + Dear ImGui frontend, linked into `tinysprite`
```

Each layer only knows the ones above it in the diagram, and CMake enforces that through target links. `core` and `io`
never include SDL or ImGui. `io` also has the animated GIF encoder, the sprite sheet layout and rectangle packing
(`rect_pack`), the Aseprite reader (`aseprite.cpp`, with its own zlib `inflate.cpp`), the tilemap export and the headless
`export` command (`cli.cpp`). Public headers live in `include/tinysprite/`; headers private to a module
(`src/core/tools/history_plot.hpp`, everything in `src/ui`) stay next to the sources.

SDL3, Dear ImGui and stb are not in the repository: `vcpkg.json` declares them and pins their versions (see
`thirdparty/README.md`). Only the fonts the UI embeds (`thirdparty/`, reduced to the glyphs it uses) are kept here.

## Document model

`Sprite` owns a `Palette`, an ordered list of `Layer`s, the frames' durations, the tags, the slices and the tilesets and
tilemaps. A `Layer` holds one `Cel` (an RGBA pixel buffer) per frame, as `shared_ptr<Cel>` so that frames can share a cel
(a linked cel). A `Sprite::Frame` is a frame taken out of the sprite (a cel per layer plus its duration), which is how a
removed frame lives on inside a history step. A cel is RGBA; indexed color is not supported.

Most of the model is small modules next to `sprite.hpp`:

- `*_ops.hpp` (`layer`, `frame`, `cel`, `tag`, `slice`, `palette`, `tilemap`, `filter`): undoable edits, all the same
  pattern: apply the change and record one history step.
- `tilemap.hpp`: tilesets and tilemaps next to the layers, and `TileStroke` for painting.
- `playback.hpp`: which frame to show as time passes (`play_order` gives the order of one loop).
- `composite.hpp`: flattening the layers, blend modes and the onion skin.

Invalid sizes throw `std::invalid_argument`. Operations that can legitimately be refused (a stroke is active, an index is
out of range) return `bool` or null instead.

## Editing and undo

Every change to pixels goes through `History`: `beginStroke(cel)`, `setPixel`, `endStroke`. A stroke is one undo step and
is stored as pixel deltas `(index, old, new)`. Writing a pixel's current color records nothing, and a stroke that changed
nothing is dropped.

Changes that are not pixel deltas (adding, removing, moving, renaming layers, opacity, visibility, frames, tags, ...) are
recorded as actions with their own undo and redo callables (`History::pushAction`, wrapped by the `*_ops.hpp` modules).
A layer or frame that is removed, or added and then undone, is kept alive by the history step so that pixel steps on its
cels stay valid. Several steps can be folded into one with `beginGroup` / `endGroup`; a group cannot hold strokes.

Memory is bounded in three places, so that running out of it is a refusal and not a crash:

- A sprite has at most `SPRITE_MAX_FRAMES` (500) frames: the timeline is an ImGui table with a column per frame plus one
  for the layer names, and a table has at most 512 columns. `Sprite::insertFrame` and `fitsCels` refuse beyond it, the
  `.pxa` reader rejects a larger file and the GIF import keeps the first 500 frames.
- A sprite has a cel budget (`SPRITE_MAX_CEL_BYTES`, 2 GiB of `width x height x 4` per layer and frame; `fitsCels`):
  adding or duplicating a layer or frame refuses beyond it, and so does importing an animated GIF whose frames alone would
  not fit.
- A `History` keeps at most `HISTORY_MAX_BYTES` of steps (`setLimit`): a stroke weighs its deltas, an action what it
  declares in `HistoryAction::bytes` (the pixels of a removed layer or frame, the snapshots of a merge), and the oldest
  steps are dropped first, never the newest.

`Editor` reports a refusal through `takeMemoryShort()` / `takeFramesFull()` (the app shows a toast) and catches
`std::bad_alloc` around the operations that allocate cels (`guarded`), closing any open history group so the history
cannot freeze.

Tools sit on top of that:

- `raster.hpp` has pure rasterizers (line, rectangle, ellipse, polygon, flood fill). They report pixels through a
  callback and never touch a cel, so the same code serves a stroke, a preview and a test.
- `PencilStroke`, `ShapeStroke` and `MoveStroke` are live strokes created by a static `begin()`. They return null when the
  history already has an open stroke, and cancel themselves when destroyed unfinished. The batch functions
  (`pencil_stroke`, `shape_stroke`, `move_stroke`) feed point lists to the same machinery, which is how tools are tested.
- `Mask` (`mask.hpp`) is a set of pixels of any shape over its trimmed bounding rectangle: a selection that is not a
  rectangle. It is built from a rasterizer (`Mask::fromPlot`; `mask_of_region` is the magic wand, `mask_of_polygon` the
  lasso) and moved, flipped, turned, clipped, added and subtracted as a value. `History::setClipMask` limits `setPixel`
  to it, which is how every tool and filter stays inside a selection; `Clip::copy`, `clip_replace`, `clear_mask` and
  `MoveStroke::begin` have a mask form that takes only its pixels.
- Symmetry (`symmetry.hpp`) mirrors every plotted pixel across the cel's center for the pencil, shapes and bucket.
- Shapes are a table (`shape.cpp`: one row per `ShapeKind`: how to draw it, whether it can be filled).

## Rendering

The visible layers are composited on the CPU (`composite_sprite`, straight-alpha "over" with the layer's blend mode,
scaled by its opacity) into one buffer that the UI uploads to a single SDL texture and draws with nearest sampling. Only
the dirty rectangle is re-composited. `Editor::markDirty` / `markAllDirty` feed it, and also bump a revision counter that
the layer thumbnails use to know when to redraw.

## The UI

```
App            window, ImGui context, main loop, menus, shortcuts, file and confirmation dialogs, autosave
 |- Document[] one per open tab: an Editor, the file it came from, its autosave slot
 |   '- Editor  document + history + tool state + texture; the only thing panels edit (App::editor_ is the active one's)
 |- Canvas     draws the sprite and overlays, turns mouse input into zoom, pan and Editor::press / drag / release
 |             (two of them in a split view, one per pane)
 |- ToolsPanel, LayersPanel, TimelinePanel, Panels   the bars and panels and their docking
```

The window is laid out as: a narrow **tool bar** on the left (`ToolsPanel::show`: the tools and the shapes as one column
of icon buttons), an **options bar** above the canvas (`ToolsPanel::contextBar`: the options of the active tool), the
**canvas** with the **timeline** under it (its top edge is a sash, `App::timelineSash`: drag to resize or fold it, double
click to reset; Ctrl+T toggles it), and the right panel titled **Inspector** (`PanelId::Layers`: Color, Layers, Slices,
Tilemaps and Properties). The layer actions are the items of `App::layerMenuItems`, shown in the Layer menu and in the
right-click menu of the list (`LayersPanel::setContextMenu`). Panel settings keys come from a fixed key
(`panel.tools.*`, `panel.layers.*`), not from the title. `DESIGN.md` is the contract for how all of it looks.

`App` and `Editor` are big classes whose code is split over files by what it is about; the declarations stay in
`src/ui/app.hpp` and `src/ui/editor.hpp`.

- `src/ui/app/`: `app.cpp` (startup, main loop, autosave), `menus.cpp`, `commands.cpp`, `shortcuts.cpp`, `dialogs.cpp`,
  `files.cpp`, `file_browser_ui.cpp` (the file dialog's folder tree), `filter_dialog.cpp`, `documents.cpp` (the open
  documents), `tabs.cpp` (the tab bar), `split.cpp` (the split view).
- `src/ui/editor/`: `editor.cpp` (document, strokes, texture), `layers.cpp`, `frames.cpp`, `colors.cpp` (colors and the
  palette), `selection.cpp`, `slices.cpp`, `tilemaps.cpp`, `filters.cpp`, `preferences.cpp` (what is saved between runs).
  Helpers they share are in `detail.hpp` / `detail.cpp`.
- `src/ui/file/` (file lists and path strings), `src/ui/layer/` (layer selection and thumbnails), `src/ui/tilemap/` (the
  Tilemaps section and the tilemap window): groups of four files or more with one prefix.

`Editor` delegates two self-contained pieces: `LayerSelection` (the active layer plus Ctrl/Shift picks, tracked by layer
identity) and `CompositeSurface` (the CPU buffer, its texture and the dirty rectangle). Both are unit-tested without an
`Editor`.

The rulers and guides are view state of an `Editor` (`ViewState::guides`), drawn and dragged by `Canvas`; `ruler.hpp` holds
the pure parts (tick spacing, snapping a guide to a cel edge, picking one under the mouse).

Tools are a table, not switches: `tool.cpp` has one `ToolTraits` row per `ToolKind` (its `ToolGroup` and, for the shape
tools, which `ShapeKind` it draws), and the editor, canvas and tools panel ask it instead of comparing tool kinds.
`Editor::press` asks `beginDrag` (`drag.cpp`) for a `Drag`, which picks a factory by the tool's group: pencil, bucket,
eyedropper, shape, select (which also moves), slice. The editor holds at most one. `Canvas` calls `update` every frame
while the button is down, then `finish` on release or `cancel` on Esc, which is how one drag becomes exactly one undo
step.

The menu commands that have a Ctrl shortcut are one table in `command_table.cpp`: label, keys, Shift rule, key repeat.
The menus (`app/menus.cpp`) and the shortcut handler (`app/shortcuts.cpp`) both read it, and `app/commands.cpp` holds what
each command does and when it is enabled. Tool letters and the single-key shortcuts stay in `app/shortcuts.cpp`; the help
window text is written by hand.

The timeline (`timeline_panel.cpp`) is a strip under the canvas, not one of the dockable `Panels`: a toolbar, an onion
skin row, a tag row and a grid of cels. `Editor` holds the current frame, the selected tag and the playback
(`editor/frames.cpp`); `App` ticks every document a pane shows once per UI frame (`Editor::tick`).

Several documents can be open (tabs, `App::docs_`), each with its own `Editor`; the panels and canvases are re-pointed at
the active one with `setEditor`, and `Editor::adoptPreferences` carries the tool, colors and clipboard from tab to tab. A
frame that closes a tab must not free its textures before the frame is rendered: see `App::retired_`.

Icons are Material Symbols glyphs merged into the UI font (`icons.cpp`), each drawn at the size that gives its ink the
same length; the 14 tool and shape icons are instead 16 x 16 bitmaps (`icon_bitmaps.cpp`) drawn one square per cell at a
whole-pixel scale, so they are the same size whatever the font does. Settings (`settings.cpp`) are a sorted `key=value` file in the config directory. `recovery.cpp` writes an
autosave copy of each document with unsaved changes (one file per document); the next start offers to recover them.
Neither is touched by `--frames` smoke runs unless `--config-dir` is given.

## File format

`.pxa` is a magic number and a version, then chunks of a 4-byte tag, a 4-byte little-endian length and the payload. A
reader skips tags it does not know, so later versions can add chunks (the version is still 1), and rejects other versions
and truncated data.

The chunk list (`HEAD`, `FRAM`, `PALT`, `LAYR`, `LINK`, `BLND`, `TAGS`, `SLCE`, `TSET`, `TMAP`), what each holds and in
which order the writer emits them are described once, in the comment in `include/tinysprite/io.hpp`.

Every writer goes through `file_write_atomic` (temporary file plus rename), so a failed write never destroys an existing
file. Paths cross the API as UTF-8 and are turned into `std::filesystem::path` with `path_from_utf8` before they are
opened.

## Testing

Test first, one file per feature. Raster and tool behavior is checked by feeding point lists and comparing golden pixel
arrays. Core tests (`tests/test_*.cpp`) link only `io` (and so `core`) and run under ASan and UBSan by default.

UI tests (`tests/ui/ui_*.cpp`) link `tinysprite_ui` and run under `SDL_VIDEODRIVER=dummy`: they drive the real `Editor`,
`App` and ImGui with synthetic input, on a software renderer. The harness counts every frame as 1/60 s, so a test does not
depend on how fast the machine is; the ImGui widgets themselves are only partly covered. The UI is built whenever SDL3 is
found (`TINYSPRITE_BUILD_UI`).
