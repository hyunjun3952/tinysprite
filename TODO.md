# TODO

## Migrate the whole project to C++ (branch `cpp-migration`)
Not started; the branch exists only as a place to do it. Today only `src/ui` is C++17 and the core / io stay C11
behind `extern "C"` header guards, so the project has two languages, two lint setups and C-style handles
(`*_create` / `*_destroy`) under the C++ RAII wrappers. Moving everything to C++ would remove that seam.
- Convert `src/core`, `src/io` and `tests/*.c` to C++17 (classes / RAII instead of create / destroy pairs, drop the
  `extern "C"` guards and the C-only compound-literal macros `RGBA_TRANSPARENT` / `SYMMETRY_NONE`).
- Replace the `Editor` deleters and raw stroke pointers with value types; revisit `History` ownership of cels.
- `project(tinysprite C)` -> `C CXX`, `CMAKE_C_*` -> `CMAKE_CXX_*`; one clang-tidy config (the `src/ui/.clang-tidy`
  overrides can then be folded into the root one); `format.sh` / `lint.sh` globs; CI.
- Keep the layering (core has no SDL / ImGui) and the TDD layout; port tests file by file with the module they cover.
- Update CLAUDE.md (language line, conventions) when done.
