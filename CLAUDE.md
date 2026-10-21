# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

tinysprite is a pixel art / 2D sprite animation editor written in C11 (SDL2 + Dear ImGui via cimgui for the UI). The project is built incrementally by milestone; progress, remaining steps and known gotchas are tracked in `docs/MILESTONES.md` (update it when a milestone finishes). Planned scope: drawing tools first, then animation (timeline, onion skin, GIF/spritesheet export), then game pipeline features (tilemaps, JSON export, headless CLI).

## Commands

```sh
cmake -S . -B build            # ASan/UBSan are ON by default (-DTINYSPRITE_SANITIZE=OFF to disable)
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -R history          # one test (ctest name = file name minus "test_")
./build/tests/test_history                 # or run a test binary directly
cmake -S . -B build -DTINYSPRITE_BUILD_UI=ON   # SDL2/cimgui frontend (off by default; needs libsdl2-dev)
```

Format/lint (needs `clang-format` and `clang-tidy`; K&R style, 4-space indent, config in `.clang-format` / `.clang-tidy`):

```sh
scripts/format.sh           # apply formatting (--check to verify only)
scripts/lint.sh             # clang-tidy via build/compile_commands.json
scripts/setup-hooks.sh      # once per clone: pre-commit = format check, pre-push = format + lint
```

CI (`.github/workflows/ci.yml`) runs format check, lint, build and tests. Run format and lint before committing; `third_party/` is excluded from both.

## Architecture

- **Layering is enforced by CMake targets**: `tinysprite_core` (no SDL/ImGui, ever) → `tinysprite_io` (core + stb) → `tinysprite_ui` (core + io + SDL2 + cimgui) → executable. Tests link only core/io. Never include SDL/ImGui headers from `src/core` or `src/io`.
- **Public headers** live in `include/tinysprite/` (core and io only); internal headers stay next to sources in `src/`.
- **Document model** is animation-ready from the start: `Sprite → Layers × Frames → Cel` (pixel buffer), with a Palette. Phase 1 uses a single frame; do not flatten this structure.
- **Undo/Redo**: every edit goes through the history module. One undo step = one user stroke; storage is a pixel delta (index, old, new). Do not mutate cels outside this path.
- **Rendering** is CPU compositing into one texture with nearest sampling, re-composited by dirty rect.
- File format `.pxa` is versioned and chunked (length-prefixed) for forward compatibility.

## Conventions

- **TDD**: write the failing test first (`tests/test_<feature>.c`, one file per feature, using `third_party/minitest.h`: `TEST`, `RUN`, `ASSERT_*`), then implement. New `test_*.c` files are picked up automatically by CMake globbing (re-run cmake).
- Tool/raster behavior is tested by feeding point lists and comparing golden pixel arrays.
- Every struct has a `*_create` / `*_destroy` pair; tests run under ASan/UBSan.
- Commits: English only, following Conventional Commits (https://www.conventionalcommits.org/ko/v1.0.0/), e.g. `feat(core): add flood fill`.
- Record dependency name, pinned version and license in `THIRD_PARTY.md` when adding anything to `third_party/`.
