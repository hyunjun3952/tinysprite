# UI audit report template

Drop empty sections. Findings sorted 심각 -> 보통 -> 낮음 -> 주의.

```markdown
# UI audit: SCOPE

Driver: computer-use | headless fallback. Build: type, sanitizer on/off. Window: WxH, scale S, display scale D.
Scenarios run: ids (n of m). Not run: ids and why.

## Summary
| Severity | Bug | Design violation | Design flaw | Improvement |
|---|---|---|---|---|
| 심각 | | | | |
| 보통 | | | | |
| 낮음 | | | | |
| 주의 | | | | |

## Findings

### F1 [보통] [Bug | Design violation | Design flaw] Short title  (Confirmed on screen | Headless | From code)
- Scenario: S13 (fixture anim64, scale 1)
- Rule: DESIGN.md section / heuristic number, or "usability, no written rule"
- Steps: 1. ... 2. ... 3. ...   (fresh start, only these steps)
- Actual: what the screen shows
- Screenshot: /abs/path/ui-audit/shots/S13-03-....png
- Expected: what it should show
- User impact: what the user was trying to do and what it costs them
- Proposal: smallest change on screen; alternative and why not
- Cause in code: file:line, root cause (bugs)
- Fix cost / verify: files and lines, tests to add, scenario id to rerun
- Disproof tried: the argument against and why it failed

## Fix groups
| Group | Findings | Shared cause | Change | Cost | DESIGN.md change? |
|---|---|---|---|---|---|

## Improvements (not problems)
| Where | Idea | Gain | Cost |
|---|---|---|---|

## Dropped on review
How many suspicions did not survive Step 3b, one line each.

## Not tested
What could not be reached with this driver, and why.

## Proposed fix order
1. Group A (reason) ...

Which findings should I fix?
```
