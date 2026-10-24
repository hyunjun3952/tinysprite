# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

tinysprite is a pixel art / 2D sprite animation editor written in C++17 (core, I/O and UI; SDL2 + Dear ImGui for the frontend). The project is built incrementally by milestone; progress, remaining steps and known gotchas are tracked in `docs/MILESTONES.md` (update it when a milestone finishes). A map of the modules and data flow is in `docs/ARCHITECTURE.md`. Phases 1 (drawing tools) and 2 (animation) are done; the game pipeline (tilemaps, JSON export, headless CLI) is phase 3.

## Commands

```sh
cmake -S . -B build            # ASan/UBSan are ON by default (-DTINYSPRITE_SANITIZE=OFF to disable)
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -R history          # one test (ctest name = file name minus "test_")
./build/tests/test_history                 # or run a test binary directly
# One build/ dir: the SDL2/ImGui frontend is built whenever libsdl2-dev is installed
cmake -S . -B build -DTINYSPRITE_BUILD_UI=OFF    # core + io + tests only (no SDL2 needed)
```

Format/lint (K&R style, 4-space indent, config in `.clang-format` / `.clang-tidy`). Both tools are pinned once, in `.pre-commit-config.yaml` (pip wheels, so local and CI run the same version); there are no helper scripts:

```sh
pre-commit install                                # once per clone: clang-format on commit, a fast clang-tidy on push
pre-commit run --all-files --hook-stage manual    # format + lint everything, exactly what CI runs
pre-commit run clang-tidy-full --hook-stage manual --files src/core/sprite.cpp   # one file (needs build/compile_commands.json)
```

CI (`.github/workflows/ci.yml`) runs only when `src/`, `include/`, `tests/`, the root `CMakeLists.txt`, `.pre-commit-config.yaml`, `.clang-format` or `.clang-tidy` change (a docs or skills change gets no checks, which is expected). It has three separate jobs: `lint` (the pre-commit hooks above; a push or PR lints only the changed files, everything when a header or lint config changed, and weekly or by hand on `schedule` / `workflow_dispatch`), `linux` (ASan/UBSan build and tests, UI included) and `windows` (MinGW build and tests). The editor uses the clangd extension with `build/compile_commands.json`. `thirdparty/` is excluded from both tools.

## Rules

**Before writing or changing C++, and before committing, branching or merging, read `.rules` and follow it.** It holds every rule, sorted by kind: layering and structure (CMake targets, document model, undo, `.pxa`, a folder for a group of four or more files with one prefix in a `src/` directory, a shallow directory tree), tests (TDD, file layout), code conventions (C++17 and RAII, cognitive complexity of 15 or less, `thirdparty/README.md`), what `clang-format` and `clang-tidy` reject, and the workflow (lint and format, branches and PRs, merging, commit messages). Lint is slow, so write to those rules instead of leaving style to the hooks. Module map, data flow, document model, undo, rendering, UI and the `.pxa` format: `docs/ARCHITECTURE.md`.
