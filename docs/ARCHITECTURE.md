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
`io` never include SDL or ImGui. `io` also has the animated GIF encoder and the sprite sheet layout. Their public headers live in `include/tinysprite/`; headers private to a module
(`src/core/tools/history_plot.h`, everything in `src/ui`) stay next to the sources.

`tests/test_*.cpp` link only `io` (and so `core`). The UI tests in `tests/ui/` link `tinysprite_ui` and run under
`SDL_VIDEODRIVER=dummy`. The UI is built whenever SDL2 is installed (`TINYSPRITE_BUILD_UI`).

## Document model

`Sprite` owns a `Palette`, an ordered list of `Layer`s, the frames' durations and the tags; a `Layer` holds one `Cel` (an
RGBA pixel buffer) per frame, as `shared_ptr<Cel>` so that frames can share a cel (a linked cel). A `Sprite::Frame` is a
frame taken out of the sprite (a cel per layer plus its duration), which is how a removed frame lives on inside a history
step. A cel is RGBA for now; indexed color is deferred.

Animation lives in a few small modules next to it: `frame_ops.h` / `tag_ops.h` / `cel_ops.h` / `palette_ops.h` (undoable
edits, same pattern as `layer_ops.h`), `playback.h` (which frame to show as time passes, and `play_order`) and the onion
skin in `composite.h`.

Invalid sizes throw `std::invalid_argument`. Operations that can legitimately be refused (a stroke is active, an index
is out of range) return `bool` or null instead.

## Editing and undo

Every change to pixels goes through `History`: `beginStroke(cel)`, `setPixel`, `endStroke`. A stroke is one undo step
and is stored as pixel deltas `(index, old, new)`. Writing a pixel's current color records nothing, and a stroke that
changed nothing is dropped.

Changes that are not pixel deltas (adding, removing, moving, renaming layers, opacity, visibility) are recorded as
actions with their own undo and redo callables (`History::pushAction`, wrapped by `layer_ops.h`). A layer that is
removed, or added and then undone, is kept alive by the history step so that pixel steps on its cels stay valid.

Tools sit on top of that:

- `raster.h` has pure rasterizers (line, rectangle, ellipse, polygon, flood fill). They report pixels through a
  callback and never touch a cel, so the same code serves a stroke, a preview and a test.
- `PencilStroke`, `ShapeStroke` and `MoveStroke` are live strokes created by a static `begin()`. They return null when
  the history already has an open stroke, and cancel themselves when destroyed unfinished. The batch functions
  (`pencil_stroke`, `shape_stroke`, `move_stroke`) feed point lists to the same machinery, which is how tools are tested.
- Symmetry (`symmetry.h`) mirrors plotted points across the cel's center for the pencil, shapes and bucket.

## Rendering

The visible layers are composited on the CPU (`composite_sprite`, straight-alpha "over", scaled by layer opacity) into
one buffer that the UI uploads to a single SDL texture and draws with nearest sampling. Only the dirty rectangle is
re-composited. `Editor::markDirty` / `markAllDirty` feed it, and also bump a revision counter that the layer
thumbnails use to know when to redraw.

## The UI

```
App            window, ImGui context, main loop, menus, shortcuts, file and confirmation dialogs, autosave
 |- Editor     document + history + tool state + texture; the only thing panels edit
 |- Canvas     draws the sprite and overlays, turns mouse input into zoom, pan and Editor::press / drag / release
 |- ToolsPanel, LayersPanel, Panels   side panels and their docking
```

`App` and `Editor` are big classes whose code is split over several files by what it is about, so the implementation files
stay a few hundred lines each (the class declarations remain in `editor.h` and `app.h`): `app.cpp` (startup, main loop, autosave), `app_menus.cpp`, `app_shortcuts.cpp`, `app_dialogs.cpp`,
`app_files.cpp`; `editor.cpp` (document, strokes, texture), `editor_layers.cpp`, `editor_palette.cpp`,
`editor_selection.cpp`, `editor_settings.cpp`. Helpers shared between the `editor_*.cpp` files are declared in
`editor_internal.h` and defined in `editor_internal.cpp`.

`Editor` delegates two self-contained pieces: `LayerSelection` (`layer_selection.h`: the active layer plus Ctrl/Shift
picks, tracked by layer identity) and `CompositeSurface` (`composite_surface.h`: the CPU buffer, its texture and the
dirty rectangle). Both are unit-tested without an `Editor`; the editor keeps the same public methods and forwards.

Tools are a table, not switches: `tool.cpp` has one `ToolTraits` row per `ToolKind` (its `ToolGroup` and, for the shape
tools, which `ShapeKind` it draws), and the editor, canvas and tools panel ask it instead of comparing tool kinds. Shapes
are the same on the core side: `shape.cpp` has one row per `ShapeKind` (how to draw it, whether it can be filled).
`Editor::press` asks `beginDrag` (`drags.cpp`) for a `Drag`, which picks a factory by the tool's group: pencil, bucket,
eyedropper, shape, select (which also moves). The editor holds at most one. `Canvas` calls `update` every frame while the button is down, then `finish`
on release or `cancel` on Esc, which is how one drag becomes exactly one undo step.

The menu commands that have a Ctrl shortcut (file, edit, zoom) are one table in `commands.cpp`: label, keys, Shift
rule, key repeat. The menus (`app_menus.cpp`) and the shortcut handler (`app_shortcuts.cpp`) both read it, and
`app_commands.cpp` holds what each command does and when it is enabled. Tool letters and the single-key shortcuts
stay in `app_shortcuts.cpp`; the help window text is still written by hand.

The timeline (`timeline_panel.cpp`) is a strip under the canvas, not one of the dockable `Panels`: a toolbar, an onion
skin row, a tag row and a grid of cels. `Editor` holds the current frame, the selected tag and the playback
(`editor_frames.cpp`); `App` calls `Editor::tick` once per UI frame with the elapsed time.

Settings (`settings.cpp`) are a sorted `key=value` file in the config directory. `recovery.cpp` writes an autosave copy
of a document with unsaved changes; the next start offers to recover it. Neither is touched by `--frames` smoke runs
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
| `TAGS` | optional: count, then per tag a name, first and last frame and a direction |

Every writer goes through `file_write_atomic` (temporary file plus rename), so a failed write never destroys an
existing file. The authoritative description is the comment in `include/tinysprite/io.h`.

## Testing

Test first, one file per feature. Raster and tool behavior is checked by feeding point lists and comparing golden pixel
arrays. Core tests run under ASan and UBSan by default. UI behavior is tested through the real `Editor` and ImGui with
synthetic mouse input; the ImGui widgets themselves are only partly covered.
