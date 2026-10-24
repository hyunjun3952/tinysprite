# Checklists for Step 3

Use as prompts while looking at each screenshot. A check that fails becomes a finding only after Step 3b.

## 1. DESIGN.md rules
The contract is `DESIGN.md` itself: Colors, Typography, Layout (widths, hit targets), Shapes, Components, Interaction principles, Do's and Don'ts,
Responsive behavior. Check each screenshot against them and cite the rule that is broken; this file does not restate them.

## 2. Usability heuristics (Nielsen) applied to this app
1. **Visibility of status**: active tool, layer, frame, tag, zoom, playback and unsaved state are visible without opening anything.
2. **Match with the real world**: the words are the ones pixel artists use (layer, frame, cel, tag, palette); one word per thing.
3. **User control**: every action is undoable; Esc leaves every mode, dialog and drag; no modal traps.
4. **Consistency**: same control looks and behaves the same in every panel; shortcuts follow Aseprite/Photoshop habits when the menu says so.
5. **Error prevention**: destructive and overwrite actions ask; invalid input is refused before it is applied; disabled beats failing.
6. **Recognition over recall**: options are visible near the tool; shortcuts are shown in menus and tooltips.
7. **Flexibility**: a shortcut exists for frequent actions; the same task works by menu, button and key.
8. **Minimalism**: no panel shows what the current task does not use (options bar shows only the active tool's).
9. **Error recovery**: error messages say what happened and what to do; a failed save never destroys the old file.
10. **Help**: the help window and tooltips are correct for the current build.

## 3. State, feedback, undo
- After every action: did the canvas, thumbnails, timeline, title and status bar all update? (stale canvas = missing `markDirty`)
- Does Ctrl+Z revert exactly what the user saw change, in one step? Does Ctrl+Y restore it? Does the dirty mark follow?
- Is there feedback within one frame (hover, press) and for slow work (export, large fill)?
- After a dialog closes, is focus back where the user was? After Esc during a drag, is the document unchanged?
- Does switching tab, tool, frame or layer mid-action leave anything half-applied?

## 4. Layout and legibility
- Clipped, overlapping or truncated text, units, icons, tooltips going off-screen, popups opening outside the window.
- Alignment: labels in a column share one left edge; values align; gaps follow 2/4/6/8/10/16.
- Scroll: scrolling areas have a visible scrollbar and do not trap the wheel; nothing important hides below the fold.
- Empty and extreme states: no layers selected, 1x1 sprite, 60 frames, 40 layers, very long names, non-ASCII names.

## 5. Input
- Targets: at least the frame height (about 26 px at scale 1); resize grabs bigger than their lines.
- Keyboard: Tab order follows reading order; focus ring always visible; typing in a field never triggers a shortcut.
- Mouse: click, double click, right click, drag and wheel each do one predictable thing; the cursor shape says what will happen.

## 6. Contrast (WCAG 2.x)
Linear channel `c = v/255`, then `c <= 0.04045 ? c/12.92 : ((c+0.055)/1.055)^2.4`; `L = 0.2126 R + 0.7152 G + 0.0722 B`;
ratio `(L1 + 0.05) / (L2 + 0.05)` with the lighter on top. Text needs 4.5, large text and icons and control boundaries 3.
Quick script (scratchpad, `python3 -I`): compute for every pair in DESIGN.md `colors:`; first to check: mute on canvas-soft,
mute on hover, body on pressed, accent-text on accent, ink on danger, hairline against canvas (decorative, exempt).
Measure from the screenshot, not only the token, when the thing sits on the sprite (overlays).
