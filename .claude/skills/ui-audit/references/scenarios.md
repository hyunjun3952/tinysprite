# Scenario catalog

One scenario = one fresh app instance (`--config-dir` per scenario). `core` marks the smoke set.
Fixtures are made in the scratchpad (never committed): `empty16` (16x16, 1 layer, 1 frame), `anim64` (64x64, 3 layers, 4 frames,
2 tags, one linked cel), `tiny1` (1x1), `big512` (512x512), `longnames` (layer and tag names of 60+ characters, non-ASCII),
`slices` (3 slices, one with a pivot), `many` (40 layers, 60 frames), `korean` (layer, tag and slice names in Hangul, one 60+ characters long),
`tilemap` (a tileset and a map placed on the sprite, one tile turned), `masks` (art with flat color areas and a hole for the wand and lasso).
Start state "default" means: no saved settings (fresh config dir), window 1100x720, scale 1.

For every scenario also run the general checks in `heuristics.md`. "Edges" are the probes of Step 3.4.

## A. Startup and chrome
| id | goal | fixture | do | check | edges |
|---|---|---|---|---|---|
| S01 core | start the app with no file | none (new sprite) | launch, look | layout matches the DESIGN.md diagram, nothing clipped, active tool and layer visible, status bar filled | scale 2 and 3, window 800x500 |
| S02 core | open a file from the command line | anim64 | launch with file | tab title, timeline shows 4 frames and tags, layer list correct | `longnames` |
| S03 | recovery prompt after an unclean exit | anim64 | edit, kill the process, relaunch with same cfg | prompt wording, what each button does, nothing lost | decline then relaunch |
| S04 | settings persist | any | change scale, dock layout, quit, relaunch | same layout and scale | corrupt settings file |

## B. Menus and shortcuts
| id | goal | fixture | do | check | edges |
|---|---|---|---|---|---|
| S05 core | find every command through the menus only | anim64 | open each menu, hover each item | every item has a label, shortcut shown, disabled ones explain why, hover-open works, no overlap | keyboard-only navigation |
| S06 | shortcuts match the menus and the help window | anim64 | press each listed shortcut | same effect as menu, help text is correct | shortcut while typing in a field, while a dialog is open, during a stroke |

## C. Drawing and canvas
| id | goal | fixture | do | check | edges |
|---|---|---|---|---|---|
| S07 core | draw a line of pixels and undo it | empty16 | pencil, drag, Ctrl+Z, Ctrl+Y | canvas updates at once, one undo step per stroke, cursor outline matches footprint | stroke leaves the window, Esc mid-drag, tool switch mid-drag |
| S08 | zoom and pan | big512 | wheel, Ctrl+wheel, middle drag, fit | grid appears from zoom 4, fit margin, nearest sampling, cursor-anchored zoom | min/max zoom, window resize, pan far away |
| S09 | every tool once | empty16 | each tool in the tool bar, its options bar | options bar shows only that tool's options, tooltip with shortcut, active tool inverted, status bar names the tool | mirror axes, fill option, 1x1 sprite (`tiny1`) |
| S10 | selection: select, move, copy, paste | anim64 | select tool, drag, move, copy/paste | marching ants visible on light and dark art, one undo step, selection survives frame change as designed | selection at the edge, paste larger than sprite |
| S11 | color: pick, swap, edit hex, palette | empty16 | color block, hex field, palette add/replace/reset | foreground/background clear, hex field accepts #RGB/#RRGGBB/#RRGGBBAA, invalid input does not corrupt, palette buttons enable/disable correctly | 256 colors, translucent color |
| S12 | eyedropper and middle-button pick | anim64 | pick from a layer, from a hidden layer | which layer is picked is clear; tab left alone | pick on empty pixel |

## D. Layers
| id | goal | fixture | do | check | edges |
|---|---|---|---|---|---|
| S13 core | add, rename, hide, lock, reorder, delete a layer | anim64 | layer menu and list | thumbnails update, eye/lock reachable, drag drop line visible, rename field, every action undoable | 40 layers (`many`), long names, delete last layer |
| S14 | multi-select layers (Ctrl/Shift) and act on them | anim64 | select two, merge down, delete | highlight of active vs selected distinguishable, disabled actions explain | selection after undo |
| S15 | opacity and blend mode | anim64 | slider, combo | slider shows value and unit, canvas updates live, one undo step | 0 and 100, typing a value |

## E. Animation
| id | goal | fixture | do | check | edges |
|---|---|---|---|---|---|
| S16 core | play the animation | anim64 | Play, Enter, stop by editing | current frame highlighted on timeline and canvas, play state obvious, edit stops it | duration 0, 1 frame, ping-pong |
| S17 | frames: add, duplicate, delete, reorder, link cels | anim64 | timeline buttons and menu | cell highlight, linked-cel marker, undo of each | 60 frames (`many`), delete last frame |
| S18 | tags | anim64 | add, rename, change range and direction, select a tag | tag row readable, range shown, selected tag clear | tag edited while playing, overlapping tags |
| S19 | onion skin | anim64 | toggle, change range | ghost frames distinguishable from the current, controls labeled | first/last frame |
| S20 | timeline sash | anim64 | drag, fold, double-click, Ctrl+T | grab area found without hint, minimum height, fold remembers height | window short, 70% cap |

## F. Documents, files, dialogs
| id | goal | fixture | do | check | edges |
|---|---|---|---|---|---|
| S21 core | new, save, close with unsaved changes | none | New dialog, draw, Close, each answer | dialog text names the file, danger primary only when destroying, Esc cancels, Enter = primary | cancel then continue editing |
| S22 | open and save as with the file dialog | scratch dir | navigate, type a path, `../`, overwrite | existing file asks before overwriting, errors shown, no way to leave the scratch dir by accident | invalid name, read-only dir, long path |
| S23 | export: PNG, sprite sheet + json, GIF | anim64 | each export dialog | options labeled with units, preview or size shown, success/failure feedback, overwrite prompt | 1x1, 512x512, zero-length name |
| S24 | tabs: open several, reorder, close others, split view | anim64, empty16 | open both, drag tab, split | active tab visible, unsaved mark, per-tab undo, tool/colors carried | 10 tabs in a narrow window |
| S25 | drag a file onto the window | anim64 | drop with and without a dialog open | opens a tab; ignored while a dialog is open | non-.pxa file |
| S26 | slices | slices | slice tool, list, details, pivot | overlay labels readable, list scrolls after 5 rows, edits undoable | label wider than slice |

## G. Panels and layout
| id | goal | fixture | do | check | edges |
|---|---|---|---|---|---|
| S27 core | resize, float, dock, reset panels | anim64 | drag inspector edge, pull a panel out, dock it, Window > Reset | min/max widths, nothing unreachable, reset restores | float off-screen, window shrink |
| S28 | collapse sections | anim64 | Window > panel > sections | folded state persists, menu stays open while toggling | all sections hidden |
| S29 | status bar and View toggles | anim64 | View menu items | each toggle visibly does what it says, state shown by check marks | |

## G2. Selections, tilemaps, guides (features added after S26)
| id | goal | fixture | do | check | edges |
|---|---|---|---|---|---|
| S30 | magic wand and lasso | masks | wand by color and by outline, lasso, add and subtract with the modifier | the shape of the selection is visible on light and dark art, status bar names the tool and mode, copy/move/clear act on the shape only, one undo step each | tolerance 0 and max, lasso with 2 points, selection at the edge |
| S31 | bucket: contiguous / everywhere, tolerance | masks | switch the mode, change the tolerance, fill | the switch and the tolerance are labeled with units, preview or result matches the mode, one undo step | tolerance 0, fill with the same color, selection active |
| S32 | tilemap window and placement | tilemap | open the tilemap window, preview, place, turn a tile, undo | preview matches the placed result, window can be closed and reopened, the grid is readable, every action undoable | 1x1 tile, map origin at the limit, resize the grid |
| S33 | rulers and guides | anim64 | toggle the rulers, drag a guide out, delete it, save and reopen | the sprite stays in place when rulers are toggled, labels do not overlap and far ticks stay readable, a guide can be hovered and removed, guides survive the file | zoom extremes, a guide outside the sprite after resize |
| S34 | New sprite dialog and export names | none | open New sprite, change the size, open the Export menu | the dialog shows the chosen size and the shape it makes, Export items say what they make, the dialog opens in this tab as designed | 1x1, 4096x4096, width and height not numbers |
| S35 | Recent colors and color block | empty16 | before and after the first color is used | the row is in the same place before the first color, the color block reads fore/background clearly | 256 colors |
| S36 core | Korean and other non-Latin text | korean | look at the layer list, timeline tags, slices and status bar; rename a layer by typing Hangul | glyphs render with the fallback font (no boxes), names are ellipsized with a tooltip and not cut mid-character, the text field accepts and keeps Hangul, the file dialog lists non-ASCII names | 60+ characters, a name mixing Latin and Hangul, scale 2 and 3 |

## H. Cross-cutting sweeps (run on 3 or more of the scenarios above)
| id | goal | do | check |
|---|---|---|---|
| X01 core | UI scale 1, 1.5, 2, 3 | `--scale S` on S01, S13, S16, S21 | no clipped text or unit, icons sharp, hit targets scale, dialogs fit the window |
| X02 core | small window (800x500, then the smallest the OS allows) | resize on S01, S16, S27 | canvas shrinks first, inspector keeps min width, options bar does not overlap, timeline folds |
| X03 | keyboard only | S05, S21, S22 | every control reachable by Tab/arrows, focus ring always visible, Esc/Enter consistent |
| X04 | rapid and repeated input | double-click, hold a shortcut, click during a fade | no double actions, no stuck hover, no dropped input |
| X05 | contrast and legibility | screenshots of S01, S13, S18 | measure mute-on-soft and every text/icon pair against `heuristics.md` thresholds |
| X06 | first-time user walkthrough | new sprite; "draw a 4-frame blink and export a GIF" using only what the UI shows | where do you stop and wonder; count dead ends and guesses |
| X07 | icon consistency | zoom the tool bar, layer rows, timeline toolbar and palette buttons at scale 1 and 2 | the longer side of every icon's ink is about the same (15 px at scale 1); no icon looks heavier or lighter than its neighbours (the plus, the refresh arrow and the bucket were the outliers) |
| X08 | the same task with a script | re-run S07, S13 and S21 with `--script` (`../../_shared/audit-conventions.md`) | the script reaches the same state as the manual run and its screenshots match; a difference is the driver, not the app |
