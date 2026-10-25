---
name: refactoring-audit
description: Finds and plans behavior-preserving refactorings in this C++ repo - code smells (refactoring.guru catalog), C++ anti-patterns, magic numbers, SOLID/OOP violations, functions at or near the cognitive complexity limit of 15, and measured performance hot spots - then applies the approved ones in small verified steps. Use when the user says "refactor", "리팩토링", "code smell", "clean up this code", "코드 냄새", "복잡도 줄여줘", "책임이 너무 많아", "too complex", "느려", "버벅여", or asks to improve structure without changing behavior. A quick cleanup of only the current diff is the built-in simplify skill. Do NOT use to fix a bug (use bug-hunting) or to review a pull request (use pr-review).
argument-hint: "[target]"
metadata:
  author: tinysprite
  version: 1.6.2
---

# Refactoring audit

## Arguments: `[target]`

`/refactoring-audit src/ui/editor/editor.cpp`, `/refactoring-audit src/core`, `/refactoring-audit` all work.
The whole argument is the **target**: the scope of Step 0 (function, file, module, files changed in the last N commits).
Missing: rank candidates and propose a few, as Step 0 says. There is one process.

Improve structure without changing behavior, and only where the gain is worth the change.
Reports follow `../_shared/audit-conventions.md`: language, where the report goes, the severity scale, headless screenshots.

## Important

- **Behavior must not change.** A refactoring that needs a test edited to pass is not a refactoring. Found a bug on the way?
  Record it and hand it to the bug-hunting skill; do not fix it inside a refactoring commit.
- **Safety net first.** Run the tests of the scope before touching anything and note the exit code. No test covers the code?
  Write a characterization test that pins today's behavior first (golden pixels, round trip), then refactor.
- **No measurement, no performance claim.** A performance finding without numbers from a Release build is dropped. See `references/measurement.md`.
- **Default is report first.** Do not edit source until the user picks which findings to apply. Then apply one finding per commit.
- **Not every smell deserves a fix.** Each finding needs a cost/benefit line; if the gain is "no gain" or "taste", mark it 보류 and move on.
  Prefer the cheapest change that removes the problem; a new class or pattern is the last option, not the first.
- **Revert when the gain is not there.** After each step re-measure the number the finding promised. If it did not drop, the step is reverted, not kept "because it is cleaner".
- **Moves are separate from edits.** Moving code (file, class, function) and changing it never share a commit, so `git diff -M --stat` shows the move as a pure rename.
- **Public surface stays fixed unless approved.** `git diff main -- include/` must be empty after a refactoring, and no function signature, header name or file format byte changes without the user's yes.
- **A claim needs evidence you have read.** Before a finding, a "found on the way" suspicion or a code comment states a fact
  (a limit, a size, a lifetime, who calls what), open the declaring header or grep the use. A name, a cast or memory is not evidence;
  an unread claim goes into the report as "unchecked", not as a finding.
- **Never edit or commit the user's uncommitted work.** A file with changes in `git status` is reported, not refactored, unless the user says to.
  Commit only your own files with `git commit --only PATH...` so staged files of the user stay out of your commit.
- Layering, the undo path and the document model in `.rules` are fixed. A refactoring must not move SDL/ImGui into `src/core` or `src/io`,
  bypass `History`, flatten `Sprite -> Layers x Frames -> Cel`, or change the `.pxa` format.

## Step 0: Scope and baseline

Pick the scope (a function, a file, a module, the files changed in the last N commits). If the user gave none, rank candidates (Step 1) and propose the top few; do not sweep the whole repo.
A scope larger than a module (`src`, a whole layer): run the complexity scan over all of it once, rank by pain x change count, and read and propose only the top
10; say which modules were only scanned (`../_shared/audit-conventions.md`, "Scope too big for one pass").

Run `git status --short` and note the files that already have changes: they are the user's work in progress (see Important).
Set up the tools once (`../_shared/audit-conventions.md`, "Tools on Windows"): `pre-commit` on `PATH`, the pinned `clang-format` and `clang-tidy`, the exit-code rule, how to run the UI exe.
The report's first line gives the HEAD SHA and the files with uncommitted changes (same file).
Build and run the tests of the scope as `CLAUDE.md` (Commands) says and read the exit code (`.rules`, section 7). Baseline red means stop and tell the user.

## Step 1: Find candidates

Run the five lenses on the scope, but keep only candidates with evidence and report the top 10 by rank. The reference file of each lens has the detail.

1. **Smells** from the refactoring.guru catalog (Bloaters, Object-Orientation Abusers, Change Preventers, Dispensables, Couplers): `references/smell-catalog.md`.
2. **C++ anti-patterns**, magic numbers and unclear constants, god objects, spaghetti control flow: `references/cpp-antipatterns.md`.
3. **Complexity**: functions over the limit of 15 (CI already blocks those) and **near the limit, 12 or more** (clang-tidy `Threshold: 11`): `references/measurement.md`.
4. **Performance**: only code with a measured problem or a plausible hot path (per-pixel loops, per-frame work, composite, flood fill, GIF encode, `.pxa` load). Measure before judging: `references/measurement.md`.
5. **OOP / SOLID / design principles** (SRP, OCP, LSP, ISP, DIP, DRY, KISS, YAGNI, Law of Demeter), applied pragmatically: `references/cpp-antipatterns.md`.

Rank by **pain x change frequency**: complexity or smell score of the code, times how often it changes
(`git log --format= --name-only -- PATH | sort | uniq -c | sort -rn | head`). A messy file nobody touches ranks below a mildly messy file edited every week.
A shape that `docs/ARCHITECTURE.md` or the "Left from the pattern survey" list in `docs/MILESTONES.md` describes is intended or deliberately postponed; do not report it unless the change makes it worth doing.

## Step 2: Evaluate each finding

For every candidate, fill one row (see `references/report-template.md`):

- Where (`file:line`), which lens and which smell / principle / measurement.
- Why it hurts (a concrete cost: harder to change X, a bug it invites, N ms per frame), not "it is ugly".
- Proposed refactoring by name (Extract Function, Replace Conditional with table, Introduce Parameter Object, and so on) and how.
- **Complexity before -> after** (cognitive complexity, lines, parameters). "Before" is a measured number. "After" is an estimate until Step 3
  re-measures it: write it as `~8 (est.)` in the report, and replace it with the measured number once applied.
- **Cost**: files and lines touched, tests to add, risk to callers, public header impact.
- Add a **cheaper alternative** when there is one, and why the proposal is still better or not.
- **Priority**: 높음 (blocks change or measured problem), 보통, 낮음, 보류 (not worth doing, or waiting for a condition).

A finding whose after-state is not clearly simpler, faster (measured) or safer is 보류.

## Step 3: Apply (only what the user approved)

One finding at a time, in a loop:

1. Confirm the safety net covers the code (Step 0). Add a characterization test if it does not. "A test names the function" is not coverage:
   break the code on purpose (flip a condition, drop a guard, change a constant) and run the tests. A survivor is a gap: write the test first, run it
   red against the break and green against the code (`.rules`, section 2). A break with no observable effect is an equivalent mutant; say so.
   A `TEST(...)` that is not listed with `RUN(...)` in `main` never runs.
   UI code: take the "before" screenshot now, with the binary built before your edit, and keep that exe or image in the scratchpad.
2. Make the smallest structural change; keep the build green after every step, not only at the end.
   Write to the rules while typing, not after the hooks complain: lines of 100 columns at most, `return {a, b};` instead of repeating the
   return type, `static_cast` for size comparisons, braces everywhere (`.rules`, sections 4 and 5). A comment you add is checked against
   the code first (grep the lifetime, the caller, the limit it names).
3. Run the affected tests after each step and the full `ctest -j8` once before the commit (a full run is 20 to 100 s). Same results as the baseline, no test edited to fit.
   UI code: also prove the screen did not change. Render the same file before and after
   (a headless screenshot, `../_shared/audit-conventions.md`) and compare with `cmp` or a file hash; a difference is a behavior change.
   A start-up screenshot only proves code that runs at start-up. When the code you changed runs on input (tabs, panes, dialogs, menus),
   drive it with a `--script` that reaches it and screenshot that state, or name the test that covers it and write
   "screen not compared: <why>" in the report. Do not present a skipped comparison as done.
   A script that reaches a state, e.g. for a tab or pane change (coordinates from a first screenshot; `tests/ui/smoke.script` is the model):
   `click 120 40` / `key ctrl+T` / `wait 3` / `shot out.png`; run it on the exe built before and the exe built after, and compare the two images by hash.
   Deleting code: show it is dead first (grep every use including `tests/` and `tests/ui/`, then build and run the tests without it); "looks unused" is not proof.
4. Format and lint the changed files before the commit, with the exit code of each: `clang-format -i` then `--dry-run -Werror`, then the
   pinned `clang-tidy -p build FILE` (`.rules`, sections 6 and 7; commands in `references/measurement.md`). The commit hook is the last check, not the first.
5. Re-measure what the finding claimed (complexity number, benchmark) on the changed file(s) after **every** commit, including a
   prep step such as a shared helper, and record before/after. A step with no measured number is reported as "not measured".
6. Sweep the class: grep the repo for the same smell. List the other sites in the report; do not fix them in this commit.
7. Commit alone, as `refactor(scope): what changed` (`.rules`, section 7), with `git commit --only PATH...`. Do not mix formatting sweeps or behavior changes in.
   Pushing is a separate step the user asks for. Before it, `git fetch` shows whether the remote answers; if the key or login is refused, stop and tell the user.

Branch policy is `.rules` (section 7): a small refactoring inside one module is a direct commit to `main`; one across layers, in public headers or hard to revert gets a branch and a PR.
When a refactoring changes structure other docs describe, update `docs/ARCHITECTURE.md` in the same commit.
A finding the user declines or defers goes into "Left from the pattern survey" in `docs/MILESTONES.md` (one line, with the condition that would make it worth doing), so it is not found and argued again next time.
A finding that grows past about 200 changed lines is split into steps that each leave the build green, or re-planned as a branch.

## Step 4: Report

Fill in `references/report-template.md`: findings by priority, then what was applied with before/after numbers, tests run with exit codes,
and what was left out and why. Keep it short; a finding that was fixed needs two lines, not a section.
End with "Not checked": every proof the skill asks for that you skipped or could only partly do (screen not compared, no Release build,
lint not run, a claim not read in the source). If the user asks you to review your own commits, list your own mistakes there, not only the code's.

## Troubleshooting

- Lint passes locally but fails in CI: `.rules` (section 7).
- `pre-commit not found` when committing, an exe that prints nothing, `git push` refused, `STL1000` from clang-tidy: `../_shared/audit-conventions.md`, "Tools on Windows".
- A refactoring grows past the planned size: stop, commit the green part, and re-plan the rest; do not leave the tree half-converted.
- Cannot show the gain with a number: say so, mark the finding 낮음 or 보류.

## Lessons (add a line when a run goes wrong; `../_shared/audit-conventions.md`, "Keeping the skills current")

- Style left to the hooks cost a rewrap and a tidy round; write to `.rules` sections 4 and 5 while typing (Step 3.2).
- An "after" complexity written as a number but never measured was an estimate; mark it `(est.)` until re-measured.
- A start-up screenshot did not reach a tab or pane change; use a script (Step 3.3).
- A comment written from memory ("until the frame ends") was wrong; grep the lifetime first.
- Line-count estimates of "merge the parallel code" findings were off by a factor of 5 or more (one finding promised -40 lines and measured +7
  after a template helper). Try the change, count the lines of the touched files, and revert if the number did not drop (Important, "Revert when
  the gain is not there"). A helper with five parameters or a template over two types rarely pays for itself.
- A new header or file does not need `cmake -S . -B build`; a reconfigure drops the STL workaround of `compile_commands.json` and every tidy run then
  fails with `STL1000`. Re-add the two defines (`../_shared/audit-conventions.md`) before blaming the code.
- A test that is defined with `TEST(...)` but not listed with `RUN(...)` in `main` never runs, and a green run then proves nothing: check that the
  "N run" count rose by the number of tests added.
- 5 of 19 changes of one run survived a deliberate break (a guard, a clamp, a cancel button, two with no visible effect): "all tests pass" was true
  and meant little. Break the code first (Step 3.1). Never commit someone else's change that has no test; the test found a real defect in it.
- A screenshot that does not open the panel you changed proves nothing about it: expand the panel with a script (hover first, then click) and
  compare the state the changed code draws.
