# Skill changelog

One line per change, newest first. Versions are in each skill's front matter.

- refactoring-audit 1.6.2: the safety net is checked by breaking the code on purpose, not by "the tests pass"; a change without a test is not committed (`.rules`, section 2).

- refactoring-audit 1.6.1: lessons from a run over `src` (estimates of "merge parallel code" were far off and must be measured, `RUN(...)` registration,
  no cmake reconfigure for a new header, expand the panel before comparing screens); Troubleshooting bullets moved back out of Lessons;
  shared notes on the compile database, `-replace` case, and hover-before-click.

- refactoring-audit 1.6.0, bug-hunting 1.5.0, pr-review 1.4.0, ui-audit 0.2.0: shared Windows tools, exit-code rule, audited-commit line and
  "Not checked" section in `audit-conventions.md`; stale SDL2, WSL, shellcheck and "three groups" text fixed; ui-audit drives the app with `--script`;
  scenarios and hotspots cover masks, tilemaps, rulers, resource limits, UTF-8 paths and Korean text; pr-review checks commit hygiene and stray files.
- refactoring-audit 1.5.0: evidence rule, no editing of the user's uncommitted files, estimates marked "(est.)", re-measure after every commit.
