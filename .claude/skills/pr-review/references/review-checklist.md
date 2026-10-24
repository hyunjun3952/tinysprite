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

## Project rules (`.rules`)

Check the diff against `.rules`: section 1 (layering, document model, undo, file format, directory layout), 2 (tests),
3 (conventions, comments), 5 (what clang-tidy rejects) and 7 (commits, branches). What a violation costs in a review:

- A direct cel mutation outside the history path is a 심각 finding.
- Branch policy: mention it only if the PR is clearly the wrong size for its risk.

## Correctness and risk

Look for what `../../bug-hunting/references/hotspots.md` lists (document model and history, raster and tools, I/O, UI, memory and arithmetic),
and for what it marks as likely false alarms.

## Tests

- New behavior has a test; the test fails on the old code.
- Tool and raster behavior is tested with point lists and golden pixel arrays.
- Test names and files follow `tests/test_<feature>.cpp`.
- UI tests run under `SDL_VIDEODRIVER=dummy`.

## Things that are not review findings

- Style that `clang-format` fixes by itself.
- Personal preference with no cost difference.
- Candidates listed under "Left from the pattern survey" in `docs/MILESTONES.md`, unless the PR is the case that makes them worth it.
