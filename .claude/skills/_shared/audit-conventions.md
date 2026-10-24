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
- The app gets no input, so the screen is the state right after start. To show another state, write a `.pxa` in that state (from a test or a
  small program with the `io` API, in the scratchpad) and pass it as `file`.
- `--scale` is a percent from 100 to 300 and is taken as given. A saved or DPI scale is capped to the display; the dummy driver's display is small.
- Save images in the scratchpad, never in the repo, and give the user the absolute path of each (SendUserFile when they follow remotely).
- Describe only what is visible. A state that needs live input (drag, hover, a menu open, animation) cannot be reached this way: use the
  computer-use or browser skill if the session has one, else say "From code" and ask for a screenshot. Never describe pixels you did not see.
