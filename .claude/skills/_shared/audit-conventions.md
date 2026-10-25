# Shared conventions of the audit skills

Used by `bug-hunting`, `ui-audit`, `refactoring-audit` and `pr-review`. The rules of the repo (tests, lint, commits, branches, merging,
exit codes) are in `.rules`, and the build and test commands in `CLAUDE.md`; this file holds only what the skills share with each other.

## Reports

- Write the report in the user's language (Korean if they wrote Korean). Code, commands, identifiers and UI strings stay as they are.
- The report goes in the chat. Do not write it into the repo unless the user asks.
- Report a result by its real exit code (`.rules`, section 7), and say what was not checked.
- **First line of every report: what was audited.** `git rev-parse --short HEAD`, the branch, and the files `git status --short` shows as changed.
  Those files are the user's work in progress: a report that built or ran them says so, and an audit of `HEAD` alone uses a clean worktree
  (`git worktree add`). Without this line nobody can reproduce a finding.
- **Last section of every report: "Not checked".** Every proof the skill asks for that was skipped or only partly done (screen not compared and
  why, no Release build, lint not run, a claim not read in the source, a platform not built).
- A finding, a suspicion or a comment that states a fact (a limit, a size, a lifetime, who calls what) is written only after the declaration or the
  use was opened. A name, a cast or memory is not evidence; an unread claim is reported as "unchecked".

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
- A start-up screenshot proves only what runs at start-up. To show code that runs on input (tabs, panes, dialogs, menus, a drag), write a script
  that reaches that state and take `shot` there; compare the same script before and after a change.

## Tools on Windows (PowerShell)

The commands in the skills are written for sh. On Windows 11 with PowerShell use these. Set them up once per session.

- **pre-commit** must be on `PATH` or `git commit` stops with "`pre-commit` not found" (the hook shim, see `docs/MILESTONES.md`). Find it with
  `python -m pre_commit --version`, then `$env:PATH += ";$env:APPDATA\Python\Python3xx\Scripts"` (the folder of `pre-commit.exe`).
- **Pinned clang tools** live in the pre-commit cache, in different `repo*` folders:
  `Get-ChildItem "$env:USERPROFILE\.cache\pre-commit\*\py_env-*\Scripts\clang-*.exe"`. Do not search the profile recursively (minutes).
  Use them directly while iterating: `clang-format -i FILE`, `clang-format --dry-run -Werror FILE`, `clang-tidy -p build --quiet FILE`.
- **clang-tidy needs the STL workaround** in `build/compile_commands.json` (`docs/MILESTONES.md`, the `STL1000` note). `cmake` regenerates the
  file and drops it: after a reconfigure, expect `STL1000` errors and add the two defines again before blaming the code.
- **Exit codes**: `.rules` (section 7) forbids reading a status after a pipe. Redirect to a log, then read the status of that command:
  `cmd > LOG 2>&1; $LASTEXITCODE`, and read the log with `Select-String` (not `grep`). Do not write `cmd | Select-Object -Last 3; $LASTEXITCODE`.
  `ctest` ends with "100% tests passed" or a failure list; quote it together with its exit code.
- **The UI exe is a GUI program**: `& exe args` returns at once, with no exit code and no output. Run it with
  `$env:SDL_VIDEODRIVER="dummy"; $p = Start-Process exe -ArgumentList ... -Wait -PassThru; $p.ExitCode`.
  Compare two screenshots with `(Get-FileHash a).Hash -eq (Get-FileHash b).Hash`.
- **Run long commands in the background** (a full `ctest` is 20 to 100 s, a scan over `src` several minutes) and write their logs to the scratchpad.
  Use `ctest -j8`; run the tests of the scope first and the full set once before the commit.
- **Push**: `git fetch` first. `Permission denied (publickey)` means the SSH key is not loaded: report it, never change the remote.

## Scope too big for one pass

A target larger than a module (`src`, a layer, "everything") is split by module and done one at a time (core, io, ui). Run the cheap, mechanical
scans over the whole target once (complexity, grep of a pattern), then read only the top candidates by rank. Say in the report which modules were
only scanned and which were read.

## Keeping the skills current

When a run of a skill goes wrong, or a rule in `.rules` changes, update the skill in the same session: add one line to its "Lessons" list (what
went wrong, how to spot it) or fix the step. Bump `version` in its front matter and add a line to `_shared/CHANGELOG.md`.
