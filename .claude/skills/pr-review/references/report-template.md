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

## Findings (most severe first)
### [심각] Short title - path/file.cpp:LINE
- Problem: what is wrong, and the case that triggers it
- Impact: what breaks
- Fix: concrete suggestion

### [보통] ...
### [낮음] ...
### [주의] ...

## Complexity and cost
| Approach | Complexity | Risk | Cost |
|---|---|---|---|
| PR as written | | | |
| Alternative | | | |

Recommendation: keep as is | switch to the alternative, because ...

## Not reviewed
Files or aspects that were skimmed or skipped, and why.

## Next step
What the author must do, or "ready to merge".
```
