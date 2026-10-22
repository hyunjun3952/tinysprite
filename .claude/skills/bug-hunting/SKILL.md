---
name: bug-hunting
description: Hunts bugs in this repo's code by logic review first, then confirms each suspicion with a test, and reports findings sorted by severity (심각/보통/낮음/주의), with UI defects described as steps, actual screen and expected screen. Use when the user says "find bugs", "bug hunt", "버그 찾아줘", "버그 헌팅", "이 코드 검토해줘", "why does this crash", "audit this module", or asks to check a function, module or recent change for defects. Do NOT use for reviewing a pull request (use pr-review) or for implementing a new feature.
argument-hint: "[target]"
metadata:
  author: tinysprite
  version: 1.4.0
---

# Bug hunting

## Arguments: `[target]`

`/bug-hunting src/core/history.cpp`, `/bug-hunting src/io`, `/bug-hunting HEAD~3..HEAD`, `/bug-hunting` all work.
The whole argument is the **target**: the scope of Step 0 (function, file, module, commit range, or a symptom in words).
Missing: follow Step 0 (ask only if unclear). There is one process; a big target is split by module and done one at a time.

Find real defects by reading first and running second. Every reported bug must say how it was confirmed.
Write the report in the user's language (Korean if they wrote Korean). Code and identifiers stay as they are.

## Important

- Default is report only. Do not change source or tests in the repo until the user picks which findings to fix.
  Temporary reproduction tests go in the scratchpad or an uncommitted file, and are removed or kept only as agreed.
- Report a failing test only. A test that passes is not written into the report (but a suspicion it disproved is dropped, not kept as a bug).
- Never report "tests passed" without the exit code of that run. Do not pipe a test or lint run through `tail`/`head`
  and read `$?`; redirect to a log file, test `$?`, then read the log.
- Every finding carries a **Reachability** line: the user action or API call sequence that gets the program into the buggy state.
  A defect nobody can reach is 주의 at most, however ugly the code.
- Mark every finding **Confirmed** (a test or run reproduces it) or **Suspected** (logic only, could not be reproduced), and say why.
- Never invent how the UI looks. Without a screenshot you took (Step 4, UI defects only), describe what the code draws, say that it is derived from code,
  and ask the user for a screenshot when the exact visual matters.

## Step 0: Scope

Decide what to hunt before reading. Ask only if it is not clear from the request:
a function, a module (`src/core`, `src/io`, `src/ui`), the last N commits (`git diff main...HEAD`), or a symptom the user saw.
Say the scope back in one line. A big scope is split by module and done one at a time.

**Target files are fixed to three groups; nothing else is read for bugs, even if the request names a wider area:**

- Test code: `tests/*.cpp`, `tests/ui/*.cpp`, and the test helpers they include (`tests/minitest.hpp`, `tests/ui/*.hpp`).
- Functional code: C++ sources and headers that take part in the program's behavior: `src/**/*.{cpp,hpp}` and the public headers in `include/tinysprite/*.hpp`.

Out of the target: `thirdparty/`, `build/`, `docs/`, `.github/`, `CMakeLists.txt` / `*.cmake` (including `tests/check_file.cmake`), and any other non-C++, non-script file.
Open those for context only (how to build or run, known gotchas and intended behavior in `docs/`), never to report a finding in them.
If a suspicion points outside the target (for example a CI workflow), mention it in one line under "not checked" in the report instead of hunting it.
Findings in test code are about the test being wrong: asserts that cannot fail, golden data that encodes a bug, missing cases, order dependence, leaks between tests.

Read `references/hotspots.md` for the places where this codebase has gone wrong or is easy to get wrong.

## Step 1: Logic review (first pass)

Read the code in scope without running anything. For each function ask:

1. What are the inputs, including empty, minimal (1x1, 1 frame, 1 layer), maximal, negative and out of range?
2. What state does it assume, and what if that state is stale or shared (linked cels, open stroke, undo in between)?
3. Do all exits leave the document, history and UI consistent (including early returns and exceptions)?
4. Are the callers' assumptions the same as the callee's (null, ownership, who calls `markDirty`)?

Write each suspicion down as: location (`file:line`), claim, the input or sequence that should trigger it.
Use the tests that already exist as the map of what is covered; look at what they do not cover.

### Step 1b: History check and class sweep

- Before trusting a suspicion, look at what already happened there: `git log -S'symbol' --oneline -- PATH` and `git blame -L` on the lines.
  A line that an earlier `fix:` commit shaped is a regression risk; read that commit's test. A "gotcha" in `docs/MILESTONES.md` may already explain the code.
- When a suspicion holds, **sweep its class**: grep the repo for the same pattern (same API misuse, same missing `markDirty`, same unchecked `begin()`)
  and list every sibling site as its own suspicion. One bug found is evidence the pattern exists elsewhere.

## Step 2: Triage small issues

For minor problems and improvements found during the read (not bugs), record in a table, do not fix:

| Where | Problem | Proposed change | Why (evidence) | Change cost | Complexity before -> after |
|---|---|---|---|---|---|

Change cost = files and lines touched, tests to add or change, risk to other callers.
Complexity = cognitive complexity or function length before and after, as numbers when you can measure them,
otherwise a clear estimate marked as such. If a change does not lower complexity or risk, say "no gain" and drop it.

## Step 3: Test to confirm

For every suspicion that can be tested, try to make it fail. Follow the repo's TDD style (`tests/minitest.hpp`, one file per feature):

```sh
cmake -S . -B build && cmake --build build
ctest --test-dir build -R NAME --output-on-failure > LOG 2>&1; echo "exit=$?"
```

Order: run the existing suite for the scope first (baseline, so you know a failure is new), then add a minimal reproduction test.
A suspicion that does not fail gets one more attempt with a different input or sequence before it stays Suspected.
The sanitizers are on by default; an ASan/UBSan report counts as a failure even when the assertion passes.

Record only the failures, with:
- **Case**: exact input or sequence.
- **Why it fails**: the root cause in the code, not the symptom.
- **What it causes**: the user-visible or data effect (wrong pixels, crash, lost undo, corrupted file).

If a test fails for a reason unrelated to the suspicion (build error, environment), fix the test harness first or report it as a blocker;
do not count it as a confirmed bug.

When the test itself is changed (existing test edited or new test added), write down:
- **Range**: what the test now covers and what it still does not (inputs, modules, platforms).
- **Regression procedure**: the commands that show existing behavior still works after the fix
  (affected test files by name, then the full `ctest --test-dir build --output-on-failure`, then `pre-commit run --all-files --hook-stage manual` for C++ changes), and their exit codes.
Never weaken or delete an existing assertion to make a test pass; that hides the bug.

### Step 3b: Try to disprove each finding

Before a finding enters the report, argue against it once, in writing (one line each):

1. Is the state reachable, or does a caller, a UI guard or an earlier check already prevent it?
2. Does the test assert the **intended** behavior (`docs/ARCHITECTURE.md`, existing tests), or only what I assumed?
3. Does it fail without my scratchpad changes and with sanitizers off too (not an artifact of the harness)?

A finding that does not survive is dropped or downgraded (Confirmed -> Suspected, or 주의). Say how many were dropped; it shows the pass was honest.

## Step 4: UI defects (screenshots only here)

Only when the symptom is on screen. Screenshots are taken **only for UI defects**: never for core/io/script/test findings,
and never "to be thorough" when the code-level evidence is already enough. If Step 1-3 found no UI defect, skip this step entirely.

The app has a screenshot option that writes a PNG; use it first (no extra tool needed, works headless; Read opens the PNG):

```sh
SDL_VIDEODRIVER=dummy ./build/src/ui/tinysprite [file.pxa | W H] --frames 3 --screenshot OUT.png --config-dir TMPDIR
```

1. Build with the UI first (`cmake -S . -B build && cmake --build build`). Use a scratchpad `--config-dir` so the user's settings and autosave are untouched.
2. `--screenshot` saves the last of the `--frames` frames; the app gets no input, so the screen is the state right after start.
   To show another state, make a `.pxa` in that state (write it from a test or a small program with the `io` API, in the scratchpad) and pass it as `file`.
   Scale with `--scale S` when the defect is about zoom or density.
3. Save images in the scratchpad as `ui-U<n>-actual.png` (and `-expected.png` only if a reference exists, for example the same file before the change).
   Open each with Read and describe only what is visible, not what the code should draw.
4. If the defect needs live input (drag, hover, menu open, animation playing) and the option cannot reach it, use the computer-use or browser skill if the session has one
   (a whole UI/UX pass, not one defect, is the `ui-audit` skill);
   otherwise say "From code", describe the screen from the code, and ask the user for a screenshot. Never describe pixels you did not see.

Do not commit screenshots or temporary `.pxa` files. Give the user the absolute path of each image (send it with SendUserFile when they follow remotely).

Use `references/report-template.md` UI block:

1. **Steps to reproduce**: situation and exact actions (tool, menu, shortcut, zoom level, frame/layer state).
2. **Actual**: what the screen shows (with the screenshot path).
3. **Expected**: what it should show.
4. **Proposal from the user's view**: how the UI should change, and why (what the user is trying to do, what confuses or blocks them).
   Prefer the smallest change that fixes it; compare with the alternative if there is one.

Where possible also give the code-level cause (for example a missing `markDirty`) and, if headless-testable, a test.

## Step 5: Report

Fill in `references/report-template.md`. Findings are sorted by severity, most severe first, then by confidence:

- **심각 (Critical)**: crash, undefined behavior, data loss or corrupted file, broken undo/redo of the document.
- **보통 (Major)**: wrong result in a realistic case, stale or wrong display, error not handled.
- **낮음 (Minor)**: edge case unlikely in practice, cosmetic, small robustness gap.
- **주의 (Note)**: not a bug now; fragile design or a trap for the next change.

End with a short summary count per severity, what was not checked, and the proposed fix order.
The step 2 table goes after the bug list. Do not write the report into the repo unless the user asks.

## After the report

When the user chooses findings to fix: write the failing test first, fix, run the regression procedure above,
re-check the sibling sites of the class sweep (fix them in the same change or list them as follow-ups),
add the pattern to `references/hotspots.md` if it is new (one line: what went wrong, where, how to spot it), then `pre-commit run --all-files --hook-stage manual` and read its exit code. Commit rules and the branch policy are in CLAUDE.md
(small fixes go straight to `main`; risky ones get a `fix/...` branch and PR).

## Troubleshooting

- Build fails before any test runs: `cmake -S . -B build` again (new test files are found by globbing); `-DTINYSPRITE_BUILD_UI=OFF` if SDL2 is missing.
- A test fails only sometimes: look for uninitialized memory or order dependence (ASan/UBSan output); run it 10 times before calling it flaky.
- Cannot reproduce a suspicion: keep it as **Suspected** with the reasoning, or drop it if the code path is shown unreachable.
