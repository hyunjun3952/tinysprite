---
name: code-refactoring
description: Finds and plans behavior-preserving refactorings in this C++ repo - code smells (refactoring.guru catalog), C++ anti-patterns, magic numbers, SOLID/OOP violations, functions at or near the cognitive complexity limit of 15, and measured performance hot spots - then applies the approved ones in small verified steps. Use when the user says "refactor", "리팩토링", "code smell", "clean up this code", "코드 냄새", "복잡도 줄여줘", "책임이 너무 많아", "too complex", "느려", "버벅여", or asks to improve structure without changing behavior. Do NOT use to fix a bug (use bug-hunting) or to review a pull request (use pr-review).
metadata:
  author: tinysprite
  version: 1.0.0
---

# Code refactoring

Improve structure without changing behavior, and only where the gain is worth the change.
Write the report in the user's language (Korean if they wrote Korean). Code and identifiers stay as they are.

## Important

- **Behavior must not change.** A refactoring that needs a test edited to pass is not a refactoring. Found a bug on the way?
  Record it and hand it to the bug-hunting skill; do not fix it inside a refactoring commit.
- **Safety net first.** Run the tests of the scope before touching anything and note the exit code. No test covers the code?
  Write a characterization test that pins today's behavior first (golden pixels, round trip), then refactor.
- **No measurement, no performance claim.** A performance finding without numbers from a Release build is dropped. See `references/measurement.md`.
- **Default is report first.** Do not edit source until the user picks which findings to apply. Then apply one finding per commit.
- **Not every smell deserves a fix.** Each finding needs a cost/benefit line; if the gain is "no gain" or "taste", mark it 보류 and move on.
  Prefer the cheapest change that removes the problem; a new class or pattern is the last option, not the first.
- Layering, the undo path and the document model in CLAUDE.md are fixed. A refactoring must not move SDL/ImGui into `src/core` or `src/io`,
  bypass `History`, flatten `Sprite -> Layers x Frames -> Cel`, or change the `.pxa` format.

## Step 0: Scope and baseline

Pick the scope (a function, a file, a module, the files changed in the last N commits). If the user gave none, rank candidates (Step 1) and propose the top few; do not sweep the whole repo.

```sh
cmake -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure > LOG 2>&1; echo "exit=$?"
```

Never read a run through a pipe's status; redirect to a log, test `$?`, then read the log. Baseline red means stop and tell the user.

## Step 1: Find candidates

Run these five lenses on the scope. The reference file of each lens has the detail.

1. **Smells** from the refactoring.guru catalog (Bloaters, Object-Orientation Abusers, Change Preventers, Dispensables, Couplers): `references/smell-catalog.md`.
2. **C++ anti-patterns**, magic numbers and unclear constants, god objects, spaghetti control flow: `references/cpp-antipatterns.md`.
3. **Complexity**: functions over the limit of 15 (CI already blocks those) and **near the limit, 12 or more**, found with a lower clang-tidy threshold: `references/measurement.md`.
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
- **Complexity before -> after** (cognitive complexity, lines, parameters, as measured numbers; estimates marked as estimates).
- **Cost**: files and lines touched, tests to add, risk to callers, public header impact.
- **Cheaper alternative** if there is one, and why the proposal is still better or not.
- **Priority**: 높음 (blocks change or measured problem), 보통, 낮음, 보류 (not worth doing, or waiting for a condition).

A finding whose after-state is not clearly simpler, faster (measured) or safer is 보류.

## Step 3: Apply (only what the user approved)

One finding at a time, in a loop:

1. Confirm the safety net covers the code (Step 0). Add a characterization test if it does not.
2. Make the smallest structural change; keep the build green after every step, not only at the end.
3. Run the affected tests, then the full `ctest`. Same results as the baseline, no test edited to fit.
4. `scripts/format.sh --check` and `scripts/lint.sh --changed`, each judged by its own exit code. Run the full `scripts/lint.sh` before opening or updating a PR.
5. Re-measure what the finding claimed (complexity number, benchmark) and record before/after.
6. Commit alone: `refactor(scope): what changed`, English, Conventional Commits. Do not mix formatting sweeps or behavior changes in.

Branch policy from CLAUDE.md: a small refactoring inside one module goes straight to `main`. Changes across layers, public headers, or anything hard to revert get a `refactor/...` branch and a PR.
When a refactoring changes structure other docs describe, update `docs/ARCHITECTURE.md` in the same commit.

## Step 4: Report

Fill in `references/report-template.md`: findings by priority, then what was applied with before/after numbers, tests run with exit codes,
and what was left out and why. Keep it short; a finding that was fixed needs two lines, not a section.

## Troubleshooting

- Tests need a rebuild after adding files: `cmake -S . -B build` again (new files are found by globbing).
- Lint passes locally but fails in CI: CI uses a different clang-tidy version. Change the flagged construct so it is gone (do not assume the first fix worked) and watch CI.
- A refactoring grows past the planned size: stop, commit the green part, and re-plan the rest; do not leave the tree half-converted.
- Cannot show the gain with a number: say so, mark the finding 낮음 or 보류.
