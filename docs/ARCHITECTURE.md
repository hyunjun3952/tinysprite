# Architecture

How tinysprite is put together and why. `CLAUDE.md` has the rules to follow, `MILESTONES.md` the progress and
gotchas; this file is the map.

## Layers

```
tinysprite_core   src/core  +  include/tinysprite/   document model, rasterizer, tools, history, compositing
      ^
tinysprite_io     src/io                              .pxa, palette files, image import / export (stb)
      ^
tinysprite_ui     src/ui                              SDL2 + Dear ImGui frontend, linked into `tinysprite`
```

Each layer only knows the ones above it in the diagram, and CMake enforces that through target links. `core` and
`io` never include SDL or ImGui. `io` also has the animated GIF encoder, the sprite sheet layout, the Aseprite reader (`aseprite.cpp`, with its own zlib `inflate.cpp`) and the tilemap export. Their public headers live in `include/tinysprite/`; headers private to a module
(`src/core/tools/history_plot.hpp`, everything in `src/ui`) stay next to the sources.

`tests/test_*.cpp` link only `io` (and so `core`). The UI tests in `tests/ui/` link `tinysprite_ui` and run under
`SDL_VIDEODRIVER=dummy`. The UI is built whenever SDL2 is installed (`TINYSPRITE_BUILD_UI`).

## Document model

`Sprite` owns a `Palette`, an ordered list of `Layer`s, the frames' durations and the tags; a `Layer` holds one `Cel` (an
RGBA pixel buffer) per frame, as `shared_ptr<Cel>` so that frames can share a cel (a linked cel). A `Sprite::Frame` is a
frame taken out of the sprite (a cel per layer plus its duration), which is how a removed frame lives on inside a history
step. A cel is RGBA for now; indexed color is deferred.

Animation lives in a few small modules next to it: `frame_ops.hpp` / `tag_ops.hpp` / `cel_ops.hpp` / `palette_ops.hpp` / `filter_ops.hpp` (undoable
edits, same pattern as `layer_ops.hpp`), `tilemap.hpp` / `tilemap_ops.hpp` (tilesets and tilemaps next to the layers, `TileStroke` for painting; the UI is the Tilemaps
Inspector section and `tilemap_window.cpp`), `playback.hpp` (which frame to show as time passes, and `play_order`) and the onion
skin in `composite.hpp`.

Invalid sizes throw `std::invalid_argument`. Operations that can legitimately be refused (a stroke is active, an index
is out of range) return `bool` or null instead.

## Editing and undo

Every change to pixels goes through `History`: `beginStroke(cel)`, `setPixel`, `endStroke`. A stroke is one undo step
and is stored as pixel deltas `(index, old, new)`. Writing a pixel's current color records nothing, and a stroke that
changed nothing is dropped.

Changes that are not pixel deltas (adding, removing, moving, renaming layers, opacity, visibility) are recorded as
actions with their own undo and redo callables (`History::pushAction`, wrapped by `layer_ops.hpp`). A layer that is
removed, or added and then undone, is kept alive by the history step so that pixel steps on its cels stay valid.

A sprite also has at most `SPRITE_MAX_FRAMES` (500) frames, because the timeline is an ImGui table with a column per frame and
ImGui stops at 511 columns: `Sprite::insertFrame` and `fitsCels` refuse beyond it, the `.pxa` reader rejects a larger file and
the GIF import keeps the first 500 frames (`Editor::takeFramesFull()` makes the App say so).

Memory is bounded in two places so that running out of it is a refusal and not a crash. A sprite has a cel budget
(`SPRITE_MAX_CEL_BYTES`, 2 GiB of `width x height x 4` per layer and frame; `Sprite::fitsCels`): `layer_op_add`,
`layer_op_duplicate`, `frame_op_add` and `frame_op_duplicate` refuse beyond it. A `History` keeps at most
`HISTORY_MAX_BYTES` of steps (`setLimit`): a stroke weighs its deltas, an action what it declares in
`HistoryAction::bytes` (merge down declares its snapshots), and the oldest steps are dropped first, never the newest.
`Editor` reports a refusal through `takeMemoryShort()` (the App shows a toast) and catches `std::bad_alloc` around the
operations that allocate cels (`guarded`), closing any open history group so the history cannot freeze.

Tools sit on top of that:

- `raster.hpp` has pure rasterizers (line, rectangle, ellipse, polygon, flood fill). They report pixels through a
  callback and never touch a cel, so the same code serves a stroke, a preview and a test.
- `PencilStroke`, `ShapeStroke` and `MoveStroke` are live strokes created by a static `begin()`. They return null when
  the history already has an open stroke, and cancel themselves when destroyed unfinished. The batch functions
  (`pencil_stroke`, `shape_stroke`, `move_stroke`) feed point lists to the same machinery, which is how tools are tested.
- Symmetry (`symmetry.hpp`) mirrors plotted points across the cel's center for the pencil, shapes and bucket.

## Rendering

The visible layers are composited on the CPU (`composite_sprite`, straight-alpha "over", scaled by layer opacity) into
one buffer that the UI uploads to a single SDL texture and draws with nearest sampling. Only the dirty rectangle is
re-composited. `Editor::markDirty` / `markAllDirty` feed it, and also bump a revision counter that the layer
thumbnails use to know when to redraw.

## The UI

```
App            window, ImGui context, main loop, menus, shortcuts, file and confirmation dialogs, autosave
 |- Document[] one per open tab: an Editor, the file it came from, its autosave slot
 |   '- Editor  document + history + tool state + texture; the only thing panels edit (App::editor_ is the active one's)
 |- Canvas     draws the sprite and overlays, turns mouse input into zoom, pan and Editor::press / drag / release
 |             (two of them in a split view, one per pane)
 |- ToolsPanel, LayersPanel, Panels   side panels and their docking
```

The window is laid out as: a narrow **tool bar** on the left (`ToolsPanel::show`: the tools and the shapes as one
column of icon buttons), an **options bar** above the canvas (`ToolsPanel::contextBar`: the options of the active tool,
drawn as its own window over the canvas area), the **canvas** with the **timeline** under it (its top edge is a sash,
`App::timelineSash`: drag to resize or fold it, double click to reset; Ctrl+T toggles it), and the right panel titled
**Inspector** (`PanelId::Layers`: Color via `ToolsPanel::colorBlock`, then Layers, then Properties). The layer actions
are the items of `App::layerMenuItems`, shown in the Layer menu and in the right-click menu of the list
(`LayersPanel::setContextMenu`). The layer list is a scrolling child that takes the height the
properties leave (`LayersPanel::list`), so the properties stay at the bottom. Panel settings keys come from a fixed key
(`panel.tools.*`, `panel.layers.*`), not from the title.

`App` and `Editor` are big classes whose code is split over several files by what it is about, so the implementation files
stay a few hundred lines each (the class declarations remain in `editor.hpp` and `app.hpp` in `src/ui/`). `src/ui/app/`:
`app.cpp` (startup, main loop, autosave), `menus.cpp`, `commands.cpp`, `shortcuts.cpp`, `dialogs.cpp`, `files.cpp`,
`file_browser_ui.cpp` (the file dialog's folder tree), `documents.cpp` (the open documents), `tabs.cpp` (the tab bar),
`split.cpp` (the split view).
`src/ui/editor/`: `editor.cpp` (document, strokes, texture), `layers.cpp`, `frames.cpp`, `palette.cpp`, `selection.cpp`,
`settings.cpp`. Helpers shared between the files in `editor/` (and `layer_selection.cpp`) are declared in
`editor/internal.hpp` and defined in `editor/internal.cpp`.

`Editor` delegates two self-contained pieces: `LayerSelection` (`layer_selection.hpp`: the active layer plus Ctrl/Shift
picks, tracked by layer identity) and `CompositeSurface` (`composite_surface.hpp`: the CPU buffer, its texture and the
dirty rectangle). Both are unit-tested without an `Editor`; the editor keeps the same public methods and forwards.

Tools are a table, not switches: `tool.cpp` has one `ToolTraits` row per `ToolKind` (its `ToolGroup` and, for the shape
tools, which `ShapeKind` it draws), and the editor, canvas and tools panel ask it instead of comparing tool kinds. Shapes
are the same on the core side: `shape.cpp` has one row per `ShapeKind` (how to draw it, whether it can be filled).
`Editor::press` asks `beginDrag` (`drags.cpp`) for a `Drag`, which picks a factory by the tool's group: pencil, bucket,
eyedropper, shape, select (which also moves). The editor holds at most one. `Canvas` calls `update` every frame while the button is down, then `finish`
on release or `cancel` on Esc, which is how one drag becomes exactly one undo step.

The menu commands that have a Ctrl shortcut (file, edit, zoom) are one table in `commands.cpp`: label, keys, Shift
rule, key repeat. The menus (`app/menus.cpp`) and the shortcut handler (`app/shortcuts.cpp`) both read it, and
`app/commands.cpp` holds what each command does and when it is enabled. Tool letters and the single-key shortcuts
stay in `app/shortcuts.cpp`; the help window text is still written by hand.

The timeline (`timeline_panel.cpp`) is a strip under the canvas, not one of the dockable `Panels`: a toolbar, an onion
skin row, a tag row and a grid of cels. `Editor` holds the current frame, the selected tag and the playback
(`editor/frames.cpp`); `App` calls `Editor::tick` once per UI frame with the elapsed time.

Several documents can be open (tabs, `App::docs_`), each with its own `Editor`; the panels and canvases are re-pointed
at the active one with `setEditor`, and `Editor::adoptPreferences` carries the tool, colors and clipboard from tab to tab.
A frame that closes a tab must not free its textures before the frame is rendered: see `App::retired_`.

Settings (`settings.cpp`) are a sorted `key=value` file in the config directory. `recovery.cpp` writes an autosave copy
of each document with unsaved changes (one file per document); the next start offers to recover them. Neither is touched by `--frames` smoke runs
unless `--config-dir` is given.

## File format

`.pxa` is a magic number and a version, then chunks of a 4-byte tag, a 4-byte little-endian length and the payload.
A reader skips tags it does not know, so later versions can add chunks (the animation chunks above are such additions: the
version is still 1), and rejects other versions and truncated data.

| Chunk | Contents |
|-------|----------|
| `HEAD` | width, height, frame count (u32 each); must come first |
| `PALT` | color count (u16), then r, g, b, a per color |
| `FRAM` | optional: the duration of every frame in ms (u16 each); only written when one is not the default; before the layers |
| `LAYR` | name (u8 length + bytes), visible (u8), opacity (u8), then the pixels of every frame; one chunk per layer, bottom to top |
| `LINK` | optional, per layer with links: layer index, count, then (frame, earlier frame it shares its cel with) pairs |
| `BLND` | optional, after the layers: count, then (layer index, blend mode byte) for each layer that is not Normal; an unknown mode reads as Normal |
| `TAGS` | optional: count (u16), then per tag a name, first and last frame and a direction |
| `SLCE` | optional, after `TAGS`: count (u16), then per slice a name (u8 length + bytes), x, y, width, height (u32 each), a flags byte (bit 0: a pivot follows as two u32, in cels from the slice's top-left corner). A slice that does not fit the sprite is dropped on reading; the pixels still load |
| `TSET` | optional, after `SLCE`: count (u16), then per tileset a name (u8 length + bytes), tile width and height, atlas width and height (u32 each) and the atlas pixels (RGBA) |
| `TMAP` | optional, after `TSET`: count (u16), then per map a name, tileset index, columns, rows (u32 each), a visible byte and one u32 per cell (0 empty, else tile + 1). The reader keeps the tile data only if every tileset and map is valid together; otherwise it drops it and the pixels still load |

Every writer goes through `file_write_atomic` (temporary file plus rename), so a failed write never destroys an
existing file. The authoritative description is the comment in `include/tinysprite/io.hpp`.

## Testing

Test first, one file per feature. Raster and tool behavior is checked by feeding point lists and comparing golden pixel
arrays. Core tests run under ASan and UBSan by default. UI behavior is tested through the real `Editor` and ImGui with
synthetic mouse input; the ImGui widgets themselves are only partly covered.
