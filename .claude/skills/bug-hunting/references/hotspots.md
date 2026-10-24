# Bug hotspots in tinysprite

Load this in Step 0. These come from the architecture rules in `.rules` and `docs/ARCHITECTURE.md`; check
`docs/MILESTONES.md` (known gotchas) for the ones found in practice.

## Document model and history

- **Undo/redo symmetry**: apply, undo, redo must give the same state as apply; undo to the start must give the original.
  Test with several steps, not one.
- **Mutation outside history**: any write to a `Cel` pixel buffer, layer list, frame list, tag or palette that is not a
  stroke or a `History::pushAction` / `perform` step.
- **Linked cels** (`shared_ptr<Cel>`): a stroke on one frame must show on linked frames; unlinking, removing a frame
  or merging must not leave a dangling or wrongly shared cel. Pixel deltas recorded earlier must stay valid after cel snapshots restore.
- **Removed frames / layers** live on inside history steps: check index shifts after insert/remove/move, and undo of a
  removal when other edits happened in between.
- **Open stroke**: `begin()` returns null when the history already has one; every caller must handle null.
  A stroke destroyed unfinished must cancel and restore pixels.
- **Tags**: ranges after frame insert, remove or move; a selected tag limiting playback when frames shrink.

## Raster and tools

- Off-by-one at the canvas edge, negative coordinates, shapes entirely outside the canvas, 1x1 sprite.
- Ellipse and polygon with degenerate input (zero width, collinear points, a single point).
- Flood fill on a large region (stack depth), on the same color as the target, with selection active.
- Symmetry and pixel-perfect mode: duplicated or missing pixels at the axis.
- Blend and alpha: premultiplied vs straight, rounding at 0 and 255, onion skin tint fade.

## I/O

- `.pxa` reader: truncated file, wrong magic, unknown version, unknown chunk (must be skipped using its length),
  declared length larger than the file (must not allocate before checking), zero frames, duplicated chunks.
- `.pxa` writer then reader round trip equals the original, including durations (`FRAM`), tags (`TAGS`), links (`LINK`).
- GIF encoder: LZW code size limits, table reset, 1-color palette, frame delay rounding (centiseconds), odd sizes.
- Spritesheet: layout math for 1 frame, many frames, non-square sizes.
- Palette files `.gpl` and `.hex`: comments, CRLF line endings, bad hex, more than 256 colors.
- Paths: Windows (`%APPDATA%`, backslashes), non-ASCII file names, unwritable directory, autosave recovery after a crash.

## UI (SDL2 + ImGui)

- **Stale canvas**: a mutating `Editor` method that forgets `markDirty` / `markAllDirty`.
- Coordinate transform (`Viewport`) at zoom extremes, negative pan, window resize, high DPI.
- Drag state: mouse released outside the window or over a panel, tool switched during a drag, shortcut pressed during a stroke
  (`if (stroking()) return;` guards).
- Playback: `tick()` with duration 0, ping-pong at 1 frame, tag edited while playing.
- Panels reading a frame or layer index that became invalid after undo.
- Text input focus eating shortcuts, or shortcuts firing while typing in a dialog.

## Memory and arithmetic

- `width * height * 4` overflow in `int`; signed/unsigned mixing in index math.
- References or iterators kept across `push_back` / `erase`.
- Lambdas stored in history capturing `this` or a reference that outlives its owner.
- Uninitialized members, which ASan does not catch but UBSan or valgrind-style runs might.

## Likely false alarms (do not report)

- Behavior documented in `docs/ARCHITECTURE.md` or `docs/MILESTONES.md` as intended (for example Cel is RGBA only, 256 colors max).
- Throwing `std::invalid_argument` for invalid sizes: the convention.
- Code in `thirdparty/`, and anything outside the target files in SKILL.md Step 0.
