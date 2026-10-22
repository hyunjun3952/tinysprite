# Smell catalog (refactoring.guru) applied to tinysprite

Source: https://refactoring.guru/refactoring/smells (five groups). A smell is a hint, not a verdict: confirm it costs something here.
Technique names are from the refactoring.guru catalog.

## Bloaters

| Smell | In this codebase look for | Usual technique |
|---|---|---|
| Long Method | Function over about 40 lines or cognitive complexity 12+ | Extract Function; split by what the branches decide (see `Decoder` in `pxa.cpp`) |
| Large Class | Class with unrelated field groups or many unrelated methods. `Editor` is a deliberate Facade (see DESIGN_PATTERNS.md): flag it only when a method does work instead of delegating | Extract Class, Move Method |
| Primitive Obsession | Several `int`s meaning width/height/x/y, color as `uint32_t` passed around raw, magic ints for modes | Introduce a small struct or enum class (like `Spread`, `Grid`) |
| Long Parameter List | 4+ parameters, especially same-typed neighbours | Introduce Parameter Object (like `OnionPlan`, `PencilOptions`) |
| Data Clumps | The same 3-4 values passed together in many signatures | Extract Class / struct |

## Object-Orientation Abusers

| Smell | Look for | Technique |
|---|---|---|
| Switch Statements | `switch` / `if` chains on a tool, mode or chunk kind repeated in several functions | Data-driven table (`ToolTraits`, `Decoder::CHUNKS`) or Strategy (`Drag` subclasses). A single switch on an enum is fine |
| Temporary Field | Member set only in some states, meaningless otherwise | Extract Class or make it a local / `std::optional` |
| Refused Bequest | Subclass that stubs or ignores base behavior | Replace Inheritance with Delegation |
| Alternative Classes with Different Interfaces | Two classes doing the same job under different method names | Rename, Extract Superclass / common interface |

## Change Preventers

| Smell | Look for | Technique |
|---|---|---|
| Divergent Change | One class edited for unrelated reasons (a UI tweak and a file-format change in the same class) | Extract Class per reason to change |
| Shotgun Surgery | One small change needs edits in many files (adding a tool, command or chunk touches 5 places) | Move Method / Field, Inline Class; or a table so it is one row |
| Parallel Inheritance Hierarchies | Adding a subclass here forces one there | Move behavior so one hierarchy references the other |

## Dispensables

| Smell | Look for | Technique |
|---|---|---|
| Comments | A comment that explains what confusing code does (not why) | Extract Function / Rename so the code says it. Keep comments that give the reason |
| Duplicate Code | Same logic in 2+ places (the `perform` helper replaced six copies) | Extract Function, Pull Up, Form Template Method. Rule of three: tolerate two |
| Data Class | Struct with only getters/setters whose logic lives elsewhere | Move Method into it. Plain data structs by design are fine |
| Dead Code | Unused function, parameter, branch, include, constant (`grep -rn` the symbol; check tests and UI) | Delete it; git keeps history |
| Lazy Class | Class or wrapper doing too little to justify its file | Inline Class |
| Speculative Generality | Unused hook, parameter, template, or abstract class "for later" | Collapse, Remove Parameter. YAGNI |

## Couplers

| Smell | Look for | Technique |
|---|---|---|
| Feature Envy | Method uses another object's data more than its own (many `other.x()` calls) | Move Method |
| Inappropriate Intimacy | Classes reaching into each other's internals, `friend` abuse | Move Method / Field, Hide Delegate, Extract Class |
| Message Chains | `a.b().c().d()` | Hide Delegate or Extract Function. Do not wrap a long chain just to hide it |
| Middle Man | Class that only forwards calls | Remove Middle Man. A Facade that coordinates (like `Editor`) is not one |
| Incomplete Library Class | A third-party gap patched with scattered helpers (stb, SDL, ImGui) | Wrapper in one place (the `io` / `ui` layer), never in `core` |

## Do not report

- The Facade, table-driven, Command and Memento shapes already listed under "In use" in `docs/DESIGN_PATTERNS.md`.
- `thirdparty/` code.
- Test code for complexity (`ASSERT_*` expands to branches), but duplicated setup in tests can still be extracted.
