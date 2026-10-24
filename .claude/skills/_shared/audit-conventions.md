# Shared conventions of the audit skills

Used by `bug-hunting`, `ui-audit`, `refactoring-audit` and `pr-review`. The rules of the repo (tests, lint, commits, branches, merging,
exit codes) are in `.rules`, and the build and test commands in `CLAUDE.md`; this file holds only what the skills share with each other.

## Reports

- Write the report in the user's language (Korean if they wrote Korean). Code, commands, identifiers and UI strings stay as they are.
- The report goes in the chat. Do not write it into the repo unless the user asks.
- Report a result by its real exit code (`.rules`, section 7), and say what was not checked.

## Severity

Four levels, most severe first. A report sorts by severity, then by how likely a user meets the finding.

- **심각 (Critical)**: crash, data loss, broken undo, a user blocked with no way out.
- **보통 (Major)**: a wrong result or display in a realistic case, a common task needs a workaround.
- **낮음 (Minor)**: cosmetic, a rare edge, a small inconsistency.
- **주의 (Note)**: not a defect now; fragile, a trade-off, a trap for the next change.

Each skill adds the examples of its own domain after this list (code defects, screen defects, review findings).

## Running the app headless

`--screenshot` writes the last of the `--frames` frames as a PNG; no display and no extra tool are needed, and Read opens the image:

```sh
SDL_VIDEODRIVER=dummy ./build/src/ui/tinysprite [file.pxa | W H] --frames 3 --screenshot OUT.png --config-dir TMPDIR [--scale PERCENT]
```

- Build with the UI first (`CLAUDE.md`, Commands). Use a scratchpad `--config-dir` so the user's settings and autosave are untouched.
- Without `--script` the app gets no input, so the screen is the state right after start. To show another state, write a `.pxa` in that state
  (from a test or a small program with the `io` API, in the scratchpad) and pass it as `file`, or drive the app with a script (next section).
- `--scale` is a percent from 100 to 300 and is taken as given. A saved or DPI scale is capped to the display; the dummy driver's display is small.
- Save images in the scratchpad, never in the repo, and give the user the absolute path of each (SendUserFile when they follow remotely).
- Describe only what is visible. Never describe pixels you did not see.

## Driving the app headless with a script

`--script FILE` feeds input to the app, one command per frame, and the run ends with the script. No display, no computer-use, and nothing else
on the screen can end up in an image:

```sh
SDL_VIDEODRIVER=dummy ./build/src/ui/tinysprite [file.pxa | W H] --script SCRIPT --config-dir TMPDIR [--scale PERCENT]
```

```
click 25 105          # the left button at a point of the window (rclick: the right one)
drag 300 200 360 260  # press, move in 8 steps, release
key ctrl+shift+Z      # ImGui key names: Z, Enter, Escape, Delete, LeftArrow, F1 ...
type Layer 2          # the rest of the line, into a focused text field
wait 3                # frames with no input; a click shows its result a frame later
sleep 300             # real milliseconds, for a 0.12 s hover or dialog fade to finish
shot out.png          # the frame just rendered (the rest of the line is the path)
```

- Coordinates are window pixels, as in a screenshot. Take the first screenshot to read them off, and click a known control to calibrate.
- A hover or a click result shows after a frame or two, and a tooltip only after the mouse has rested: `move X Y`, `wait 3`, `sleep 900`, `wait 2`, then `shot`. The
  sleep lets the clock pass and the frames after it draw what the clock allows (a fade, a tooltip); without them the image shows the state before.
- A bad line stops the run with `FILE: line N: why` and exit code 2. An unknown key name is reported on stderr and skipped.
- Like `--frames`, a script run neither reads nor writes the user's settings unless `--config-dir` is given. Use a scratchpad one.
- Still out of reach: native file dialogs of the OS (the app has its own, which works), and anything that needs real time input (holding a key).
- The parser is `src/ui/script.cpp`; the feed to ImGui is `feedScript` in `src/ui/app/app.cpp`; `tests/ui/smoke.script` is the ctest example.
