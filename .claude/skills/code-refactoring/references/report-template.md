# Refactoring report template

Drop sections that are empty. Findings are sorted by priority (높음, 보통, 낮음), 보류 last.

```markdown
# Refactoring: SCOPE

Scope: files / functions. Mode: soft | hard. Baseline: tests pass (exit=0), N tests. Candidates were ranked by pain x change frequency.

## Findings
| # | Priority | Where | Lens | Problem and cost it causes | Refactoring | Complexity before -> after | Change cost | Cheaper alternative |
|---|---|---|---|---|---|---|---|---|
| R1 | 높음 | src/x.cpp:120 | complexity | cognitive 14, edited 9 times in 6 months | Extract Function by decision | 14 -> 6 and 7 | 1 file, ~40 lines, 1 test | none |

Lens: smell (name) | anti-pattern | magic number | complexity | performance | SOLID (principle).

## Performance measurements (only if a performance lens was used)
| Scenario | Input | Build | Runs | Median / worst | Budget | Verdict |
|---|---|---|---|---|---|---|

## Applied
| # | Commit | Before -> after (measured) | Tests run (exit codes) | format / lint |
|---|---|---|---|---|

## Left out (보류)
- R4: reason (no measurable gain / waits for a condition / taste only)

## Found on the way (not refactoring)
- Suspected bug at file:line, handed to bug-hunting.

## Docs updated
ARCHITECTURE.md / MILESTONES.md changes, or "none needed".

## Next
Proposed order for the remaining findings.
```
