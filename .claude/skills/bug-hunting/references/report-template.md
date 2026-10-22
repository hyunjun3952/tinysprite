# Bug hunting report template

Drop sections that are empty. Findings are sorted 심각 -> 보통 -> 낮음 -> 주의.

```markdown
# Bug hunt: SCOPE

Scope: what was read (files, commit range). Mode: soft | hard. Baseline: existing tests pass/fail (exit code).

## Summary
| Severity | Count | Confirmed | Suspected |
|---|---|---|---|
| 심각 | | | |
| 보통 | | | |
| 낮음 | | | |
| 주의 | | | |

## Bugs

### B1 [심각] Short title - path/file.cpp:LINE  (Confirmed | Suspected)
- Case: exact input or sequence that triggers it
- Why: root cause in the code
- Effect: what goes wrong for the user or the data
- Evidence: failing test name and its output (first failing assertion or sanitizer line), or the reasoning if Suspected
- Suggested fix: what to change, in one or two lines; cost (files / lines / tests)

### B2 [보통] ...

## UI defects
### U1 [보통] Short title  (Confirmed | From code)
- Steps: 1. ... 2. ... 3. ...
- Actual: what the screen shows
- Screenshot: absolute path of the captured image (UI defects only), or "none: derived from code"
- Expected: what it should show
- Proposal (user's view): how to change the UI and why
- Cause in code / test: if known

## Test changes
- Added or changed: test file and cases
- Range: what is now covered, what is not
- Regression procedure: commands run and their exit codes (affected tests, full ctest, format check, lint)

## Improvements (not bugs)
| Where | Problem | Proposed change | Why | Change cost | Complexity before -> after |
|---|---|---|---|---|---|

## Coverage (hard only)
| Function | Inputs and sequences tried | Result |
|---|---|---|

## Not checked
What was out of scope or could not be tested, and why.

## Proposed fix order
1. B1 (reason) ...
```
