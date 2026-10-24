# PR review checklist

Load this in Step 4. Skip rows that do not apply to the PR kind.

## By kind of change

| Kind | Look at first |
|---|---|
| Bug fix | Is there a regression test that fails without the fix? Is the root cause fixed or only the symptom? Same bug elsewhere (grep the pattern)? |
| Feature | Tests first (`tests/test_<feature>.cpp`), public API minimal, undo path, `.pxa` impact, `docs/MILESTONES.md` updated when a milestone finishes. |
| Refactor | Behavior unchanged: same tests pass untouched, no test edited to fit. Diff should shrink complexity, not move it. |
| Dependency | `thirdparty/README.md` has name, pinned version, license. License compatible. Version pinned, not floating. Changelog read for breaking changes. dependabot PRs: check CI only plus the changelog. |
| Docs | Matches the code as it is now. Links work. No claim the code does not back. |
| CI / tooling | Third-party actions pinned to a version. `permissions` not widened. Scripts pass shellcheck. Does not weaken a check to get green. |
| Test-only | Tests assert something real (not just "does not crash"). No flaky timing or order dependence. |
| File format | Version bump, chunk is length-prefixed, old readers skip it, old files still load, test with an old-format fixture. Needs a feature branch + PR (risky change). |

## Project rules (from `.rules`)

- Layering: no SDL or ImGui include in `src/core` or `src/io`. `tests/test_*.cpp` link only core and io.
- Public headers only for core and io, in `include/tinysprite/`; internal headers next to the source.
- Every edit goes through history: pixel strokes through a stroke class, other edits through `History::pushAction` / `perform` in `*_ops.h`.
  A direct cel mutation outside this path is a 심각 finding.
- Document structure kept: Sprite, Layers x Frames, Cel; `shared_ptr` cels for linked frames; removed frames and layers live on in history steps.
- No raw owning pointers. Invalid size throws `std::invalid_argument`; refusable operations return `bool` or null.
- Cognitive complexity of every function in `src/` is 15 or less.
- Commit messages: English, Conventional Commits.
- C++17 only; K&R style, 4 spaces (clang-format hook).
- Branch policy: a risky change (new module, format change, cross-layer behavior) should be a PR; a small low-risk change would have gone straight to `main`.
  Mention it only if the PR is clearly the wrong size for its risk.

## Correctness and risk

- Off-by-one and bounds on pixel, frame and layer indexes; empty sprite, 1x1 sprite, 0 frames.
- Integer overflow in width * height * 4, and in index math with `int` vs `size_t`.
- Lifetime: references to vector elements kept across a push_back; lambdas capturing `this` stored in history.
- Undo/redo symmetry: redo after undo gives the same state; undo of the last step leaves nothing behind.
- Stroke `begin()` returning null is handled by the caller.
- UI: `markDirty` / `markAllDirty` called after every mutation, otherwise the canvas is stale.
- Error paths: file I/O failure, truncated or hostile `.pxa`, huge declared lengths (no allocation before validation).
- Behavior on Windows (MinGW): paths, `%APPDATA%`, line endings.

## Tests

- New behavior has a test; the test fails on the old code.
- Tool and raster behavior is tested with point lists and golden pixel arrays.
- Test names and files follow `tests/test_<feature>.cpp`.
- UI tests run under `SDL_VIDEODRIVER=dummy`.

## Things that are not review findings

- Style that `clang-format` fixes by itself.
- Personal preference with no cost difference.
- Candidates listed under "Left from the pattern survey" in `docs/MILESTONES.md`, unless the PR is the case that makes them worth it.
