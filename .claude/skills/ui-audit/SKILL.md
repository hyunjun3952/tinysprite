---
name: ui-audit
description: Audits the tinysprite UI/UX by driving the real app with computer-use, one isolated scenario at a time, comparing what is on screen with DESIGN.md, then reports bugs and design flaws with screenshots, root cause and a smallest-change fix, and applies the approved ones test-first. Use when the user says "UI 점검", "UI 감사", "ui audit", "UX 문제점", "화면이 이상해", "디자인 결함", "개선점 찾아줘", "컴퓨터 유즈로 테스트", or asks whether the app follows DESIGN.md. Do NOT use for core/io logic bugs (use bug-hunting), for reviewing a pull request (use pr-review), or for a pure code-structure cleanup (use refactoring-audit).
argument-hint: "[area | scenario id | all]"
metadata:
  author: tinysprite
  version: 0.1.0
---

# UI audit

## Arguments: `[area]`

`/ui-audit canvas`, `/ui-audit dialogs`, `/ui-audit S07`, `/ui-audit all`, `/ui-audit` all work.
The argument picks areas or scenario ids from `references/scenarios.md`. Missing: ask once which areas, default to a
smoke pass over every area (the scenarios marked `core`). A big scope is split by area and done one scenario at a time.

The goal is to find where the running UI is **wrong** (bug), **against the design** (DESIGN.md violation) or **hard to use**
(design flaw), prove each with a screenshot and steps, and propose the smallest change that fixes it.
Reports follow `../_shared/audit-conventions.md`: language, where the report goes, the severity scale, headless screenshots.

## Important

- The contract is `DESIGN.md` (tokens, components, interaction principles, responsive rules). Intended behavior is in
  `docs/ARCHITECTURE.md` and the gotchas in `docs/MILESTONES.md`. A finding states which rule it breaks, or says
  "usability, no written rule" (a design flaw); never invent a rule.
- **Report only by default.** No change to `src/`, `tests/` or `DESIGN.md` until the user picks which findings to fix.
- **Only describe what you saw.** Every finding cites a screenshot you took in this run. If computer-use is not available
  (Step 0), say so, use the headless fallback, and mark those findings `From code` or `Headless`, never `Confirmed on screen`.
- **One scenario = one fresh app instance** with its own scratch `--config-dir`, so a finding is never an artifact of
  leftover state from the previous scenario. Close the app after each (see Step 3).
- Never touch the user's real files or settings: scratch config dir, scratch `.pxa` files, nothing under `~`
  except the scratchpad. The file dialogs are tested only inside the scratch directory.
- Computer-use rules from its skill hold: request access first and wait; never click links; terminals and IDEs are
  click-only (use Bash for commands, not typing into a terminal window); no financial actions (not relevant here).
- Report a check by its real exit code, as `.rules` (section 7) says.

## Step 0: Preflight

1. Say the scope back in one line (areas, number of scenarios).
2. Read `DESIGN.md` fully, then `references/heuristics.md`. Skim `docs/MILESTONES.md` for known gotchas in the area
   (a documented limit is not a finding).
3. Build the UI (`CLAUDE.md`, Commands) and a scratch workspace.
   Scratchpad layout: `ui-audit/fixtures/` (small `.pxa` files per scenario), `ui-audit/shots/` (`S<nn>-<step>.png`),
   `ui-audit/cfg-S<nn>/` (one config dir per scenario).
4. Make fixtures with a small program or test using the `io` API (not committed): empty 16x16, 64x64 with 3 layers and 4
   frames and tags, a 1x1, a 512x512, a long layer name, a file with a slice. Each scenario in `references/scenarios.md`
   names the fixture it needs.
5. Pick the driver (the first that is available):
   - **computer-use** (preferred): load its tools (see the `computer-use` skill: one ToolSearch `computer`, then request
     access to the app; the window is a native SDL2 window, tier "full"). Launch the app with Bash in the
     background: `./build/src/ui/tinysprite FIXTURE --config-dir CFG [--scale S]`. If the session runs on WSL, the window
     appears through WSLg; check with a first screenshot that the window is visible and not scaled by the host.
   - **headless fallback** (`../_shared/audit-conventions.md`, Running the app headless; `FIXTURE` is the file, `CFG` the scenario's config dir)
     gives the state right after start only (no input). Anything needing input is then tested with a synthetic-input
     test in `tests/ui/` (see `tests/ui/ui_*.cpp` for the pattern) and marked `Headless`, or listed under "not tested".
   Say which driver is used at the top of the report.
6. Record the environment once: window size, UI scale, display scale, build type (sanitizers make the app slower; note
   frame-time complaints as such).

## Step 1: Static contract sweep (no app yet)

Cheap checks of the code against DESIGN.md. Each hit is a suspicion for Step 3 to confirm on screen:

- Hex or `ImVec4`/`IM_COL32` literals in `src/ui/**` outside `theme.cpp` (design: tokens only).
- Pixel literals not wrapped in `theme::px()` (breaks at scale > 1).
- Icon-only buttons without `theme::tip` / `theme::hint`.
- Click targets smaller than the frame height.
- Dialogs without Esc/Enter handling; destructive dialogs not using the danger primary.
- Actions with no disabled reason, or state changes with no `markDirty`.
- Strings: inconsistent casing, trailing periods, a different word for the same thing (Frame vs Cel vs Cell).

```sh
grep -rnE '0x[0-9a-fA-F]{6}|IM_COL32|ImVec4\(' src/ui --include=*.cpp | grep -v theme.cpp
```

Write each as a suspicion: `file:line`, rule, what the screen should show. Do not report them yet.

## Step 2: Scenario plan

Take the scenarios in scope from `references/scenarios.md`. Each has an id, a fixture, a start state, the user goal, the
actions, and what to check. Add scenarios for suspicions from Step 1 and for anything the user mentioned.
Order: smoke first (does it start, is anything clipped), then by area, then the cross-cutting sweeps (scale, window size,
keyboard-only). Show the plan as a table (id, area, goal, driver) before running when more than 8 scenarios.

## Step 3: Run each scenario in isolation

For every scenario, in this order:

1. **Fresh start**: new `CFG` dir, launch with the fixture, wait for the first frame, screenshot `S<nn>-00-start.png`.
2. **Do the user's task** with the real controls (mouse, keyboard, menu), as a first-time user would: no shortcuts the UI
   does not show, no knowledge of internals. After every action that changes the screen, screenshot
   `S<nn>-<step>-<what>.png`. Keep a numbered list of the exact actions, it becomes the Steps.
3. **Check** against the lists in `references/heuristics.md`: design rules (DESIGN.md), the 10 usability heuristics,
   the state/feedback/undo checks, and the state-specific ones from the scenario. Look at the whole screenshot, not only
   the part you acted on: stale canvas, clipped text, wrong highlight, overlapping panels, missing hover, unreadable mute text.
4. **Probe the edges** the scenario lists (minimum window, scale 2 and 3, 1x1 sprite, long names, 100+ items, rapid
   repeated input, Esc during a drag, undo/redo after the action, switching tab or tool mid-action).
5. **Measure when it matters**: contrast of two colors from the screenshot or from `DESIGN.md` hex values (formula in
   `references/heuristics.md`), hit target size in screenshot pixels divided by the display scale, frames to react.
6. **Close the app** (quit through its menu or Ctrl+Q, and check that the unsaved-changes dialog behaves). Do not kill it
   while a dialog is open unless the dialog itself is the bug. Then the next scenario starts clean.

Keep every screenshot that shows a finding; delete the others from the report (not from disk). If the app crashes, hangs
or shows a sanitizer report in the launching terminal, that is a 심각 finding on its own: capture the last screenshot and the terminal output.

### Step 3b: Reproduce and try to disprove

Before a finding enters the report:

1. **Reproduce once more from a fresh start** with only the listed steps. A finding that needs the leftover state of an earlier scenario is dropped or rewritten with that state as a step.
2. Is it intended? Check `docs/MILESTONES.md`, tooltips and `DESIGN.md` (Known gaps are not findings).
3. Is it the driver (computer-use click offset, WSLg scaling, a screenshot taken mid-fade) and not the app? Retake after the 0.12 s animations settle.
4. For a bug with a code cause: can a headless test (`tests/ui/`) show it? If yes, write it in the scratchpad and run it to confirm (exit code).

Say how many suspicions were dropped.

## Step 4: Classify

Every finding gets one **kind** and one **severity**:

| Kind | Meaning |
|---|---|
| Bug | does something other than what the UI says or the docs intend (stale canvas, wrong state, crash, lost undo) |
| Design violation | works, but breaks a written rule in DESIGN.md (wrong token, no tooltip, clipped at scale) |
| Design flaw | works and follows the rules, but a first-time user is blocked, misled or slowed (no written rule; cite the heuristic) |
| Improvement | not a problem; a clear gain for small cost |

Severity is the shared scale (`../_shared/audit-conventions.md`). On screen, 보통 also covers a stale display, a misleading state and a DESIGN.md rule broken in a main view.

## Step 5: Fix proposal for each finding

For each, in the user's view first, then the code:

1. **What the user is trying to do** and what goes wrong.
2. **Smallest change** that fixes it, in terms of the screen; one alternative when there is a real one (and why not).
3. **Where in the code** (`file:line`, the `theme::` helper to use or add) and the root cause if it is a bug.
4. **Cost**: files and lines, tests to add, risk to other panels; and **how to verify** (the scenario id to rerun, the headless test).
5. Whether it needs a DESIGN.md change (a new token or rule) and which section.

Group findings that share one cause (one missing helper, one wrong token) into one fix.

## Step 6: Report

Fill in `references/report-template.md`. Attach the absolute path of every screenshot used (send them with SendUserFile
when the user follows remotely). Order: summary table, findings by severity, fix groups with the proposed order,
dropped suspicions, scenarios run / not run and why.
End by asking which findings to fix.

## After the report: fixing the chosen findings

Tests, code, lint, branches and commits follow `.rules` (sections 2, 3, 5, 6 and 7). Per fix group, what is specific to a UI fix:

1. The failing test is a headless one when it can be (`tests/ui/ui_<feature>.cpp` with synthetic input, or a golden
   screenshot comparison only if the repo already does that); otherwise state the manual re-check scenario.
2. Fix with the smallest change.
3. **Re-run the same scenario** from a fresh start and compare the new screenshot with the old (`-after.png`). Also re-run one
   neighbouring scenario to catch regressions.
4. If the fix adds or changes a token or rule, update `DESIGN.md` in the same change.
5. Add a line to `references/scenarios.md` for any new bug class worth a permanent scenario, and update
   `docs/MILESTONES.md` if a milestone's gotcha list changed.
6. When all chosen fixes are done, lint, test and commit as `.rules` says.

## Troubleshooting

- No computer-use tools and no enable tool: say so plainly, run the headless fallback, and offer to resume with computer-use
  when the user turns it on (Claude desktop app, Settings, This computer, Computer use).
- The window does not appear (WSL without WSLg, no display): check `echo $DISPLAY $WAYLAND_DISPLAY`; otherwise headless only.
- Clicks land in the wrong place: the display scale differs from the screenshot scale; click a known control first
  (a menu title) to calibrate, and note the offset.
- Screenshots show half-faded dialogs or hover tints: wait about 0.2 s and retake.
- The app is slow: sanitizer build. Rebuild with `-DTINYSPRITE_SANITIZE=OFF` in a separate build dir for timing questions only.
