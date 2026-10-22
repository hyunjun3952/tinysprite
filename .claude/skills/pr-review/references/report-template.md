# PR review report template

Keep it short; drop a section when it has nothing to say.

```markdown
# PR #N: TITLE

**Verdict: Approve | Request changes | Comment**  (one-sentence reason)

## Overview
- Author: NAME (bot / first-time / regular)
- Kind: bug fix | feature | refactor | dependency | docs | CI/tooling | test-only | file format
- Purpose: what problem it solves, linked issue
- Size: N files, +A / -D, M commits
- Base <- head: main <- feat/xyz   Mergeable: yes/no

## CI
| Job | State | Note |
|---|---|---|
| lint | pass/fail/pending/not run | |
| linux | | |
| windows | | |

## Commits
| SHA | Message | One logical change? | Test included? |
|---|---|---|---|

Atomic: yes/no. Reviewed per commit: yes/no.

## Test integrity and scope
Removed or weakened assertions, tests that cannot fail, test-first or not, files the description does not explain. "Clean" if none.

## Findings (most severe first)
### [심각] Short title - path/file.cpp:LINE  (Confirmed | Suspected)
- Problem: what is wrong, and the input or sequence that triggers it
- Impact: what breaks
- Fix: concrete suggestion

### [보통] ...
### [낮음] ...
### [주의] ...

## Run results
| Step | Command | Exit code |
|---|---|---|
| build / ctest / hooks / test-gap mutation (or "not checked": fork without a yes) | | |

Changed functions: cognitive complexity measured, before -> after.

## Existing review threads
Unresolved comments from others, or "none".

## Complexity and cost
| Approach | Complexity | Risk | Cost |
|---|---|---|---|
| PR as written | | | |
| Alternative | | | |

Recommendation: keep as is | switch to the alternative, because ...

## Merge gate
| Check | yes / no / not checked |
|---|---|
| 3 CI jobs ran and passed | |
| No unresolved thread | |
| Tests for new behavior | |
| Docs in sync (ARCHITECTURE / MILESTONES / thirdparty README) | |
| Commit messages valid | |
| Size fits risk | |

## Not reviewed
Files or aspects that were skimmed or skipped, and why.

## Next step
What the author must do, or "ready to merge".
```
