---
version: alpha
name: tinysprite-design
description: A quiet, dark, tool-first interface for a pixel art editor. The sprite is the only colorful thing on screen. The chrome is a neutral near-black with a short ladder of lifted surfaces, hairline dividers, borderless controls told apart by fill, 6-8 px corners, and one off-white accent that only marks what is selected or focused. Geist Regular for text, Material Symbols for icons, no ornament. Source of truth for every value below is `src/ui/theme.hpp` / `theme.cpp`; this file explains the intent and the rules, and the code wins when they disagree.

colors:
  canvas: "#0f1011"
  canvas-soft: "#1d1e21"
  hover: "#26282c"
  pressed: "#2f3136"
  hairline: "#2a2c31"
  workspace: "#08090a"
  accent: "#e8eaed"
  accent-strong: "#c4c8ce"
  accent-text: "#0f1011"
  slider-fill: "#3a3d44"
  slider-fill-active: "#4a4e57"
  danger: "#c0403d"
  danger-strong: "#a63836"
  ink: "#f7f8f8"
  body-strong: "#d0d6e0"
  body: "#b0b6c0"
  mute: "#8a8f98"

typography:
  ui:
    fontFamily: "Geist Regular (embedded subset), Material Symbols Outlined merged for icons"
    fontSize: 14px
    fontWeight: 400
    lineHeight: 1.0
    letterSpacing: 0
  section-title:
    fontFamily: "Geist Regular"
    fontSize: 14px
    fontWeight: 400
    letterSpacing: 0
  shortcut-hint:
    fontFamily: "Geist Regular"
    fontSize: 14px
    textColor: "{colors.mute}"
  icon-glyph:
    fontFamily: "Material Symbols Outlined"
    fontSize: 20px

rounded:
  control: 6px
  card: 8px
  window: 0px
  full: 9999px

spacing:
  hair: 1px
  xxs: 2px
  xs: 4px
  sm: 6px
  md: 8px
  lg: 10px
  xl: 16px

layout:
  window-default: 1100x720
  tool-bar-width: 52px
  options-bar-height: 40px
  inspector-width: 280px
  inspector-min: 250px
  inspector-max: 480px
  status-bar-height: 24px
  timeline-cell: 30x22
  timeline-name-column: 120px
  timeline-sash: 8px
  scrollbar: 12px
  ui-scale: 1.0-3.0

components:
  button:
    backgroundColor: "{colors.canvas-soft}"
    textColor: "{colors.ink}"
    rounded: "{rounded.control}"
    padding: 4px 8px
  button-hover:
    backgroundColor: "{colors.hover}"
  button-pressed:
    backgroundColor: "{colors.pressed}"
  button-selected:
    backgroundColor: "{colors.accent}"
    textColor: "{colors.accent-text}"
    rounded: "{rounded.control}"
  button-dialog-primary:
    backgroundColor: "{colors.accent}"
    textColor: "{colors.accent-text}"
    rounded: "{rounded.control}"
    minWidth: 88px
  button-dialog-danger:
    backgroundColor: "{colors.danger}"
    textColor: "{colors.ink}"
    rounded: "{rounded.control}"
    minWidth: 88px
  button-disabled:
    backgroundColor: "{colors.canvas-soft}"
    textColor: "{colors.mute}"
  input-field:
    backgroundColor: "{colors.canvas-soft}"
    textColor: "{colors.ink}"
    rounded: "{rounded.control}"
    padding: 4px 8px
  slider:
    backgroundColor: "{colors.canvas-soft}"
    fillColor: "{colors.slider-fill}"
    rounded: "{rounded.control}"
  tool-button:
    backgroundColor: "{colors.canvas-soft}"
    textColor: "{colors.body}"
    rounded: "{rounded.control}"
    size: frame height + 8px
  tool-button-active:
    backgroundColor: "{colors.accent}"
    textColor: "{colors.accent-text}"
  panel:
    backgroundColor: "{colors.canvas}"
    textColor: "{colors.ink}"
    rounded: "{rounded.window}"
    padding: 10px
  card:
    backgroundColor: "{colors.canvas}"
    rounded: "{rounded.card}"
  popup:
    backgroundColor: "{colors.canvas}"
    rounded: "{rounded.card}"
  modal:
    backgroundColor: "{colors.canvas}"
    rounded: "{rounded.card}"
  tab-selected:
    backgroundColor: "{colors.canvas-soft}"
    textColor: "{colors.ink}"
    overline: "{colors.accent}"
  tab:
    backgroundColor: "{colors.canvas}"
    textColor: "{colors.mute}"
  workspace:
    backgroundColor: "{colors.workspace}"
  status-bar:
    backgroundColor: "{colors.canvas}"
    textColor: "{colors.mute}"
    height: 24px
---

## Overview

This file is the **design contract** the `ui-audit` skill tests the running app against. Values come from `src/ui/theme.hpp`; behavior rules (Interaction principles, Do's and Don'ts, Responsive behavior) are targets: where the app does not meet one yet, that is a finding, not a reason to edit this file.

tinysprite is a drawing tool, so the interface steps back. The base atmosphere is a **neutral near-black** (`{colors.canvas}` #0f1011) with no hue, so that no panel color competes with the pixels being drawn. Depth comes from a short ladder of lifted greys, never from shadows or colored borders. The one place that is darker than the panels is the **workspace** behind the sprite (`{colors.workspace}` #08090a): the artwork sits in a well.

The interface has exactly one voice of emphasis: the **off-white accent** (`{colors.accent}` #e8eaed). It marks what is selected or focused (the active tool, the selected choice, checkmarks, the current tab's overline) and nothing else. Because the accent is a light fill with dark text, "selected" reads as an **inverted** control; the eye finds it without any hue.

**Key characteristics:**
- Neutral near-black chrome; the sprite is the only colorful thing on screen.
- Controls have **no borders**. A fill tells a control from the panel (`{colors.canvas-soft}`); hover and pressed lift one rung each.
- Dividers are 1 px hairlines. Few borders remain (windows, popups, child areas).
- Corners are 6 px on controls, 8 px on cards and popups, and square on docked windows (they meet edge to edge).
- One accent, one danger color. Red is only for actions that destroy something.
- Geist Regular at one size (14 px at scale 1), icons from Material Symbols merged into the same font so a glyph is drawn with text.
- Everything drawn in pixels goes through `theme::px()` so the whole UI follows the UI scale (1 to 3).
- Small, fast motion only: 0.12 s ease on hover and selection, a fade-in on dialogs. No bounce, no slide.

## Colors

### Surfaces (the ladder, darkest to lightest)
- **Workspace** (`{colors.workspace}` #08090a): behind the sprite. Darker than every panel.
- **Canvas** (`{colors.canvas}` #0f1011): the window, panels, popups, menus, title bars, scroll tracks. The default floor.
- **Canvas Soft** (`{colors.canvas-soft}` #1d1e21): cards, inputs, buttons, the selected document tab.
- **Hover** (`{colors.hover}` #26282c): any soft surface under the mouse; hovered tab and row.
- **Pressed** (`{colors.pressed}` #2f3136): pressed or selected soft surface, scrollbar grab.
- **Hairline** (`{colors.hairline}` #2a2c31): 1 px dividers, window and popup borders, table lines.
- **Slider Fill** (`{colors.slider-fill}` #3a3d44) and **Active** (#4a4e57): the filled part of a slider; a lifted grey so the digits on it stay readable.

### Accent and danger
- **Accent** (`{colors.accent}` #e8eaed): the active tool, the selected choice, checkmarks, the tab overline, the primary dialog button. Never decoration.
- **Accent Strong** (`{colors.accent-strong}` #c4c8ce): the pressed accent.
- **Accent Text** (`{colors.accent-text}` #0f1011): text and icons on top of the accent.
- **Danger** (`{colors.danger}` #c0403d) and **Danger Strong** (#a63836, pressed): the primary button of a dialog that destroys something (delete, discard, reset). Not an error color for text.

### Text
- **Ink** (`{colors.ink}` #f7f8f8): primary text, labels, values.
- **Body Strong** (`{colors.body-strong}` #d0d6e0): emphasized secondary text.
- **Body** (`{colors.body}` #b0b6c0): inactive icons, running text in dialogs.
- **Mute** (`{colors.mute}` #8a8f98): disabled text, shortcut hints, units, status bar, inactive tabs.

### Rules for color
- A new color needs a new row in `theme.hpp` first. Panels never inline a hex value.
- Text on any surface must keep a contrast ratio of at least 4.5:1 for text and 3:1 for icons. Control boundaries are told by fill alone and are exempt (canvas-soft on canvas is 1.14:1 by design); state is never carried by the boundary, a selected control is inverted. `{colors.mute}` on `{colors.canvas-soft}` is the tightest pair in the system; check it before using mute for anything that must be read.
- Tag bands (`theme::TAG_COLORS`) are user data and the only hue besides the artwork; the dock-drop highlight is `theme::PANEL_DROP`. Errors and warnings are plain ink text, never red or amber (red is for destroying data).
- Color never carries meaning alone. A selected control is also inverted, a disabled one also has the "not allowed" cursor and no tooltip behavior change.

## Typography

One family, one size. Hierarchy comes from color (ink, body, mute) and position, not from size or weight.

| Token | Size | Use |
|---|---|---|
| `ui` | 14 px x scale | everything: labels, values, menus, tabs, buttons, status bar |
| `shortcut-hint` | 14 px, `{colors.mute}` | the key at the right end of a choice button or menu row |
| `icon-glyph` | about 20 px x scale | layer and button icons (Material Symbols, range U+E000-U+F8FF); each glyph is drawn at the size that gives its ink a 15 px longer side (x scale, limited to 0.8-1.5 of 20 px), because the font pads its glyphs unevenly (`icons::fitted_glyph_size`) |

- Font rendering is tuned for crispness at 14 px: whole-pixel glyph positions, one sample per pixel (`PixelSnapH`, `OversampleH/V = 1`). Do not enable oversampling or fractional positions; letter spacing goes uneven.
- Numbers that count something show their unit (`100 ms`, `50%`), drawn inside the field so it stays while typing (`theme::input_int`, `theme::slider_int`).
- Section headers are a hairline above and a plain chevron and title, not a filled bar (`theme::section`).
- Strings are English, sentence case, no trailing period on labels; tooltips are short imperative phrases with the shortcut in parentheses ("Play the animation (Enter)").

## Layout

### Window structure
```
+-- menu bar ---------------------------------------------------+
| document tabs                                                 |
+------+-------------------------------------------+------------+
| tool | options bar (options of the active tool)  | Inspector  |
| bar  +-------------------------------------------+ Color      |
| 52   | canvas (workspace well, sprite, overlays) | Layers     |
|      +--- sash (drag / double click) ------------+ Properties |
|      | timeline                                  |  280       |
+------+-------------------------------------------+------------+
| status bar (24)                                               |
+---------------------------------------------------------------+
```
- Default window 1100 x 720. The tool bar is 52 px, the options bar 40 px, the inspector 280 px (resizable 250-480), the status bar 24 px, all at scale 1.
- The layer list takes the height the properties leave, so the properties stay at the bottom (`LayersPanel::list`, minimum 96 px).
- The timeline folds and resizes through the sash (8 px grab area). Ctrl+T toggles it; it never takes more than 70% of the canvas area.

### Spacing
- Base unit 2 px; common steps 4, 6, 8, 10, 16. Window padding 10, frame padding 8 x 4, item spacing 8 x 6, inner spacing 6 x 4. Scrollbar 12, slider grab 10. All multiplied by the UI scale.
- Dialog buttons: at least 88 px wide, 16 px padding at each side of the label, 8 px gap, right-aligned under a hairline, primary on the right.
- Related controls sit in the same row with 8 px gaps; groups of options in the options bar are separated by 16 px.

### Hit targets
- Tool buttons are square, frame height + 8 px (`ToolsPanel::iconRow`; they wrap at the panel width). Frame height is the minimum for any clickable row. A target smaller than the frame height needs a reason (the timeline cell is 30 x 22 because there are many).
- Grab areas for resizing (panel edge 4 px, sash 8 px) are larger than their visible line.

## Elevation and depth

| Level | Treatment | Use |
|---|---|---|
| Well | `{colors.workspace}` | behind the sprite |
| Flat | `{colors.canvas}`, no border | docked panels, bars, menus |
| Soft | `{colors.canvas-soft}` fill, no border | buttons, inputs, cards, selected tab |
| Hairline | 1 px `{colors.hairline}` | window edges, popups, child areas, dividers |
| Modal | `{colors.canvas}` over a 50% black dim | dialogs; fade in over 0.12 s |

No drop shadows. A popup is told from the panel behind it by its hairline border and its 8 px corners.

## Shapes

| Token | Value | Use |
|---|---|---|
| `{rounded.window}` | 0 | docked panels and windows (they meet edge to edge) |
| `{rounded.control}` | 6 px | buttons, inputs, sliders, grabs, scrollbars, tabs |
| `{rounded.card}` | 8 px | popups, modals, child areas |
| `{rounded.full}` | pill / circle | the swap-colors button, dots |

Icons are Material Symbols Outlined (unfilled), except the tool and shape icons, which are 16 x 16 pixel bitmaps (`icon_bitmaps.cpp`): shapes 12-14 px across with a 2 px stroke, tools a 1 px outline. Chevrons are drawn as two strokes (">" closed, "v" open), never as filled triangles.

## Components

### Buttons
- **button**: `{colors.canvas-soft}` fill, ink text, 6 px corners, no border. Hover `{colors.hover}`, pressed `{colors.pressed}`.
- **choice button** (`theme::choiceButton`): left-aligned label with its shortcut at the right end in mute. The selected one is **inverted** (accent fill, dark text).
- **action button** (`theme::actionButton`): plain button whose tooltip also shows while it is disabled. A disabled button greys its text to `{colors.mute}`.
- **dialog buttons** (`theme::dialog_buttons`): the primary is accent-filled, or danger-filled when the dialog destroys something. A dialog has at most one filled button.
- **tool button**: square icon button (frame height + 8 px), `{colors.body}` icon on soft fill; the active tool is inverted.

### Inputs
- **input field**: soft fill, no border, 6 px corners, 4 x 8 padding. Number fields show the unit.
- **slider** (`theme::slider_int`): rounded field filled with `{colors.slider-fill}` up to the value, value and unit centered on it. No grip over the digits. Drag, or arrow keys when focused. No Ctrl+click text entry.
- **combo** (`theme::begin_combo`): field with a drawn chevron at the right end.
- **hex field** (`theme::hex_field`): rounded color preview, a "#", the digits. Accepts #RGB, #RRGGBB, #RRGGBBAA.
- **focus**: ImGui's nav cursor in ink at 70% alpha. Keyboard focus must always be visible.

### Navigation
- **menu bar**: titles open on hover once the mouse rests on them (`theme::begin_menu`); submenus open on hover.
- **document tabs**: the selected tab sits on a soft fill with a thin accent overline; unselected tabs are canvas with mute text; an unsaved document should be marked on its tab.
- **section** (`theme::section`) and **tree node** (`theme::tree_node`): chevron + title, folding on click.

### Panels
- **Tool bar**: one column of icon buttons, then the shapes. **Options bar**: only the options of the active tool, so it changes with the tool.
- **Inspector**: Color, then Layers, then Slices and Tilemaps (both folded by default), then Properties. Panel settings keys are fixed (`panel.tools.*`, `panel.layers.*`), not derived from the title.
- **Timeline**: toolbar, onion skin row, tag row, grid of cels. The playhead and the selected cells use the accent family; a cel with pixels shows a dot and a cel that shares its pixels with another (linked) shows a bar.
- **Status bar**: 24 px, mute text: the active tool, cursor position, zoom and transient messages (check `statusbar.cpp` for the exact set).

### Canvas
- Nearest-neighbor sampling always; the pixel grid appears from zoom 4. Fit margin 16 px.
- Overlays (selection marching ants, slices, onion skin, symmetry axes, guides, loupe) are drawn above the sprite and never change its pixels. They must stay visible on both light and dark artwork. Their colors are the fixed `theme::overlay` values in `theme.hpp`, not the surface ladder.
- The cursor shows the tool's footprint. The loupe (14 px cells) follows the mouse with a 22 px gap.

### Dialogs
- Modal, auto-sized, fade in 0.12 s over a 50% dim. Title, content, hairline, buttons right-aligned. Esc cancels, Enter confirms, and neither is eaten by a focused text field.
- Destructive confirmations use the danger primary and name the thing destroyed ("Discard changes to cat.pxa?"). Overwriting a file always asks first.
- A dropped file is ignored while a dialog is open.

## Interaction principles

1. **One undo step per user gesture.** If the UI shows a change, Ctrl+Z reverts exactly that.
2. **No dead ends.** Every disabled control tells why (tooltip stays on a disabled `actionButton`) and every modal can be left with Esc.
3. **Never hide state in a mode the user cannot see.** The active tool, the active layer and frame, the selected tag, onion skin and playback must each be visible on screen at all times.
4. **Typing wins over shortcuts.** While a text field has focus, single-key shortcuts do nothing.
5. **The canvas owns the mouse inside it.** Panels never start a drag that ends over the canvas, and a stroke released outside the window still ends.
6. **Fast feedback.** Hover and selection react within one frame, ease in 0.12 s; nothing animates longer than a dialog fade.

## Do's and Don'ts

### Do
- Add a tooltip with the shortcut to every icon-only control (`theme::tip`, `theme::hint`).
- Use `theme::px()` for every pixel size and `theme::` helpers for every shared widget.
- Mark selection by inverting (accent fill, dark text), and disabled by mute text.
- Keep text one size; make emphasis with ink vs. mute.
- Use the danger primary only when the action destroys data.
- Keep all four edges of docked panels square; round only what floats.

### Don't
- Don't introduce a second accent or a hue in the chrome. The sprite is the color.
- Don't add borders to controls; use a fill.
- Don't add shadows, gradients, bounce or slide animations.
- Don't draw an ImGui filled triangle arrow; use the chevron helpers.
- Don't inline hex values or hard-coded pixel sizes in panel code.
- Don't use red for non-destructive warnings or errors in text.
- Don't make a click target smaller than the frame height without a reason written next to it.
- Don't let a label, a unit or a value be clipped at any UI scale from 1 to 3 or at the minimum window size.

## Responsive behavior

A desktop tool: there are no breakpoints, but the layout must hold across window size and UI scale.

| Condition | Required behavior |
|---|---|
| Window at default 1100 x 720 | the layout above, nothing clipped |
| Window narrower than default | the inspector keeps at least its 250 px minimum; the canvas shrinks first; the options bar scrolls (Shift + wheel) and marks the cut with a chevron at its right end, never overlaps |
| Window short | the timeline folds before the canvas disappears; the layer list keeps 96 px |
| UI scale 1.0 to 3.0 | every text, unit and icon fits its control; scale follows the display DPI (`scaleForDpi`, quarters). A scale whose minimum window (640 x 420 x scale) does not fit the display is not used: the saved or DPI scale is capped at start (`window_max_scale`) and the UI scale menu disables it with the reason. An explicit `--scale` is taken as given. |
| Many documents | tabs scroll or shrink; the active tab is always visible |
| Large sprite / extreme zoom | the viewport clamps; the grid draws only from zoom 4; fit margin 16 px |

## Iteration guide

1. Change one component at a time and name it by its token or helper (`theme::choiceButton`, `button-selected`).
2. A variant of a component (`-hover`, `-pressed`, `-selected`, `-disabled`) is a separate row in `components:`.
3. Reference tokens, never hex, in this file and never raw literals in `src/ui`.
4. If a rule here and `theme.hpp` disagree, the code is what ships: fix whichever is wrong in the same change.
5. A new widget goes into `theme.hpp` only if two panels need it.
6. When in doubt about emphasis: change the color step (mute, body, ink), not the size or weight.

## Known gaps

- Contrast of `{colors.mute}`: 5.86 on canvas, 5.13 on canvas-soft, 4.54 on hover, but 4.01 on pressed (below 4.5), so text that sits on the pressed fill uses `{colors.body}` (`theme::hint_text`, the selected hidden layer name).
- No light theme exists. Everything above is dark-only.
- The help window text is written by hand and is not covered by the theme helpers.
- Only English strings; font subset covers Latin text, so other scripts in layer names may not render.
- Touch input, screen readers and a high-contrast mode are not designed.
