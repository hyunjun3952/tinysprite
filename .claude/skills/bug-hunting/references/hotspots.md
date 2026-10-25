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

## Selections, masks and tilemaps

- `Mask` (`src/core/mask.cpp`): an empty mask, a mask whose bounds touch the cel edge, `subtract` / add of shapes, magic wand with tolerance 0 and 255,
  lasso with fewer than three points; masked copy, move and clear must be one undo step each and leave pixels outside the mask alone.
- Tilemaps (`src/core/tilemap*.cpp`): a cell that refers to a missing tile, resizing a grid, a tileset atlas whose size is not a multiple of the tile,
  a map origin at the limits (`TILEMAP_MAX_DIMENSION`, `CEL_MAX_DIMENSION`), tile flips and turns, undo of placement, `.pxa` TSET/TMAP/TPOS round trip.
- Guides and rulers: a guide outside the sprite after a resize or crop (saved only when it lies on the sprite), more than 65535 guides (u16 count).

## Resource limits and allocation failure

- Limits are named in the headers: `SPRITE_MAX_FRAMES` (500), `CEL_MAX_DIMENSION` (4096), `HISTORY_MAX_BYTES`, the cel budget, `TILEMAP_MAX_DIMENSION`.
  Check each entry point that grows something (insert frame, import, paste, resize, tilemap grid, GIF/Aseprite import) refuses at the limit and
  leaves the document unchanged; a refused `insertFrame` keeps the frame (earlier fix).
- `std::bad_alloc` must not leave a half-built document or an open stroke; history trims old steps when over its byte budget.
- A file that declares more than it contains must be refused before it allocates (`.pxa`, GIF, Aseprite, images).

## Text and paths

- Windows paths: non-ASCII names, backslashes, `%APPDATA%`, roots (`C:\`), one tab per file for the same path spelled two ways. The same bug was fixed in
  several places (image, palette, sheet, dialogs): grep every `fopen` / `ifstream` / `std::filesystem` use for the UTF-8 route, not only the one reported.
- Korean and other non-Latin text: layer and tag names, text fields, the fallback font, clipped and ellipsized names, a name cut in the middle of a
  multi-byte character (names are cut at TAG/SLICE/TILE_NAME_MAX bytes).
- JSON output must stay valid UTF-8 (sheet export).

## Platform differences (CI is Linux/GCC with sanitizers, and Windows/MinGW)

- A `switch` over an enum without every enumerator (`-Werror=switch`), a lambda or callback that keeps a reference to a local (stack-use-after-return),
  a stream still open when its folder is removed, `long` being 32 bits on Windows (index and size math), MSVC vs GCC STL differences.

## UI (SDL3 + ImGui)

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
