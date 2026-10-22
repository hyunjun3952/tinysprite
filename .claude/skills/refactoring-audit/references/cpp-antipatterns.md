# C++ anti-patterns, magic numbers and design principles

Project conventions come first (CLAUDE.md): C++17, RAII, no raw owning pointers, `std::invalid_argument` for invalid sizes,
`bool` or null for refusable operations, rasterizers report pixels through a `std::function`.

## C++ anti-patterns

| Anti-pattern | Look for | Better |
|---|---|---|
| Raw owning pointer, `new` / `delete` | `new` outside a smart-pointer factory | `std::unique_ptr` / value member |
| Spaghetti control flow | Deep nesting (3+), flags that steer later branches, early exits scattered, `goto` | Guard clauses, extract by decision, table dispatch |
| God object / too many responsibilities | A class whose methods group into unrelated clusters; header includes from many layers | Extract Class (SRP). Facade delegation is fine |
| Magic numbers | Bare literals other than 0, 1, -1 whose meaning is not obvious (`255`, `4`, `0x47`, `16`, `1000`, `100` for centiseconds) | Named `constexpr` where the name adds meaning (see below) |
| Unclear or overused constants | `constexpr int K1 = 3;`, one constant reused for two meanings, a constant for every literal, constants far from their only use | Rename by meaning, split by meaning, inline a constant used once if the literal is clearer |
| Macros | `#define` for constants or functions | `constexpr`, inline function, template |
| C-style cast / `reinterpret_cast` without need | `(int)x`, `(uint8_t*)p` | `static_cast`; byte access through `std::byte` / `unsigned char` |
| Output parameters | `void f(T& out)` returning results through references | Return a value or struct (guaranteed move/elision) |
| Pass-by-value of heavy objects | `Sprite`, `std::vector`, `std::string` by value in a hot or frequently called function | `const&`; sink parameter by value only to move from it |
| Copy in range-for | `for (auto x : vec_of_heavy)` | `const auto&` |
| Out-of-bounds-prone indexing | Hand-written `y * w + x` repeated | One accessor (`Cel::at`-style) that checks or documents the range |
| Signed/unsigned mix | `int` index against `size_t` size | One type per index; `static_cast` at the boundary |
| `using namespace` in a header | | Qualify names |
| Header bloat | Heavy includes in a public header, no forward declaration | Forward declare, move includes to the `.cpp` |
| Missing `const` / `explicit` / `[[nodiscard]]` | Query methods not const; single-arg constructors; ignorable `bool` result of a refusable op | Add them. Cheap, safe, local |
| Exceptions as flow control | try/catch around expected outcomes | `bool` / null / `std::optional` per project convention |
| Output via `std::endl` in loops, `printf` debugging left behind | | Remove or `'\n'` |
| Global mutable state, singletons | | Pass the dependency; keep state in the owning object |

### Magic numbers: when to name, when to leave

Name it when the literal (a) appears more than once, (b) carries a domain meaning a reader cannot guess
(GIF block ids, `.pxa` chunk tags and sizes, centisecond conversion, palette limit 256, bytes per pixel 4), or (c) is a tunable (zoom limits, autosave interval).
Leave it when it is obviously structural (`0`, `1`, `2` as in "half", array index of a fixed tuple, `255` in an alpha-math line next to a comment-free obvious blend) or when a name would only restate the number (`kTwo`).
A good name says the meaning (`kBytesPerPixel`), not the value (`kFour`). Put it next to its only user; share it in a header only when two files need the same fact.
Replacing every literal with a constant is itself the anti-pattern; judge each one.

## Design principles (pragmatic)

Apply a principle only when its violation costs something here (a change that is hard, a bug that keeps coming back, code that cannot be tested). Never add an abstraction "to follow SOLID".

| Principle | Violation signal | Typical fix | Over-application to avoid |
|---|---|---|---|
| SRP | A class changes for two unrelated reasons; a function does I/O and logic and formatting | Split by reason to change | One class per function |
| OCP | Adding a tool / command / chunk means editing a growing `switch` in many places | Table row, Strategy | An interface with one implementation |
| LSP | Subclass overrides to do nothing or throw; caller checks the dynamic type | Composition, narrower base | Deep hierarchies |
| ISP | A class forced to implement or depend on members it never uses; a panel taking the whole `Editor` for one call | Smaller interface / pass only what is needed | One-method interfaces everywhere |
| DIP | `core` depends on a concrete UI or I/O type; logic hard-wired to a global | Depend on a callback or interface owned by the lower layer (the rasterizers' `std::function` is the pattern) | A factory for every object |
| DRY | The same rule written in 3+ places | Extract once | Merging code that merely looks alike but changes for different reasons |
| KISS / YAGNI | Generality nobody uses; a framework for one case | Delete, inline | |
| Law of Demeter | `editor.sprite().layer(i).cel(f).pixels()` in UI code | A method on the object that owns the knowledge | Pass-through wrappers (Middle Man) |
| Composition over inheritance | Inheritance only to reuse code | Member + delegate | |
| Encapsulation / immutability | Public mutable members; mutation of a cel outside the history path | Private state, mutate only through the history API | |

OOP checks for this repo: document types stay RAII value-like classes; strokes stay created by `begin()`; UI state and behavior that belong together are one class.
