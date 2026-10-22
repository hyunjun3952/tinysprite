# Design patterns

Which patterns the code uses, which ones it could still use, and why. `ARCHITECTURE.md` is the map;
this file is the reasoning behind the shapes in it.

## In use

| Pattern | Where | Why it fits |
|---|---|---|
| Command | `HistoryAction`, `History::pushAction` / `perform`, every `*_ops.h` | An edit is an undo / redo pair of callables, so one history serves layers, frames, tags, cels and palettes alike. `perform` is "record, then run redo", which six ops used to spell out by hand. |
| Memento | `Merge` (cel snapshots), `TagSnapshots`, `palette_op_set` (whole palette) | Undo of an edit that is not a pixel delta restores a saved state; restoring whole cels keeps the pixel steps recorded earlier valid. |
| Factory method + RAII | `PencilStroke::begin`, `ShapeStroke::begin`, `MoveStroke::begin`, `Editor::create` | A stroke can be refused (the history has one open), so construction returns null; the destructor cancels an unfinished stroke. |
| Template method | `LiveStroke` (`restart`, `end`, `cancel`) | Pencil, shape and move share the open / commit / revert life cycle and differ in what they plot. |
| Strategy | `Drag` subclasses per tool, `ToolTraits` tables | The mouse handling of a tool is chosen at press time; a table replaces a switch on the tool kind. |
| Data-driven dispatch | `Command` table, `FileActionInfo` rows, `Decoder::CHUNKS` in `pxa.cpp`, `PaletteFormat` | Adding a menu command, a file action or a `.pxa` chunk is one row, not a new branch in a long function. |
| Facade | `Editor` (panels only talk to it) | The document, history, selection, tool state and texture are kept consistent in one place. |
| Flyweight | `shared_ptr<Cel>` in linked cels | Frames that show the same pixels share one buffer, so a stroke on one shows on all. |
| Parameter object | `OnionPlan`, `LitBounds`, `Spread`, `Grid`, `FrameMove`, `PencilOptions` | Several values that travel together become one named argument instead of a long parameter list. |

## Candidates, not done

- **Observer** for `Editor::markDirty` / `markAllDirty` / `revision`: today every mutating method calls them by hand, and a
  forgotten call is a stale canvas. A change signal the texture, thumbnails and timeline subscribe to would remove the calls.
  Worth it when a fourth consumer appears.
- **Guard helper** for `if (stroking()) return;`, which appears about forty times in the `Editor` methods. A small `Editor::whenIdle(fn)`
  would name the rule once. Low value until the rule changes.
- **One holder for `Slot` and `Membership`**: both keep an item attached to or detached from its sprite (frame, layer). A
  template over the owned type would merge them; they differ enough in what attach does that it has not paid off.
- **State** for `Playback` (Forward / Reverse / PingPong) if a fourth mode is added; `nextFrame` is the single place that
  knows the three today.

## Limits that keep the code readable

`clang-tidy` enforces a cognitive complexity of at most 15 per function in `src/` (`readability-function-cognitive-complexity`
in `.clang-tidy`; `tests/.clang-tidy` turns it off, since `ASSERT_*` and `RUN` expand to branches). When a function
reaches it, split by what the branches decide (see `pxa.cpp`'s `Decoder`, `timeline_panel.cpp`'s `grid`) rather than
by line count.
