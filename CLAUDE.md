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

## Architecture

Module map, data flow, document model, undo, rendering, UI and the `.pxa` format: `docs/ARCHITECTURE.md`. Hard rules:

- **Layering is enforced by CMake targets**: `tinysprite_core` (no SDL/ImGui, ever) → `tinysprite_io` (core + stb) → `tinysprite_ui` (core + io + SDL2 + ImGui) → executable. Never include SDL/ImGui headers from `src/core` or `src/io`. Public headers live in `include/tinysprite/` (core and io only), internal ones next to sources.
- **Document model**: `Sprite → Layers × Frames → Cel`; cels are shared (`shared_ptr`) when frames are linked, and removed frames / layers live on inside history steps. Do not flatten this structure.
- **Undo/Redo**: every edit goes through the history module, one undo step per user stroke; non-pixel edits are `History::pushAction` steps in `*_ops.h`. Do not mutate cels outside this path.
- File format `.pxa` is versioned and chunked (length-prefixed): see `docs/ARCHITECTURE.md#file-format`.

## Conventions

- **TDD**: write the failing test first (`tests/test_<feature>.cpp`, one file per feature, using `tests/minitest.hpp`: `TEST`, `RUN`, `ASSERT_*`), then implement. New `test_*.cpp` files are picked up by CMake globbing (re-run cmake). Test layout (core/io tests, `tests/ui/ui_*.cpp` under `SDL_VIDEODRIVER=dummy`, golden pixel arrays): `docs/ARCHITECTURE.md#testing`.
- Keep every function in `src/` at a cognitive complexity of 15 or less (checked by clang-tidy; not applied to `tests/`). Split by what the branches decide, not by line count (examples: `pxa.cpp`'s `Decoder`, `timeline_panel.cpp`'s `grid`).
- C++17, no raw owning pointers: document types are RAII classes; live strokes are created by a static `begin()` returning a `std::unique_ptr` (null if the history already has an open stroke) and cancel themselves when destroyed unfinished. Invalid sizes throw `std::invalid_argument`; refusable operations return `bool` or null. Details: `docs/ARCHITECTURE.md`. Tests run under ASan/UBSan.
- Commits: English only, following Conventional Commits (https://www.conventionalcommits.org/ko/v1.0.0/), e.g. `feat(core): add flood fill`.
- Record dependency name, pinned version and license in `thirdparty/README.md` when adding anything to `thirdparty/`.

## Workflow (mandatory)

These rules exist because they were broken repeatedly; follow them without being reminded.

**Lint and format**
- Run the hooks (`pre-commit run --all-files --hook-stage manual`, or the clang-tidy hook on the changed files while iterating) before every commit that touches C++, and fix everything they report. Run the full set before opening or updating a PR.
- Judge a check by its real exit code. Never pipe it through `tail`/`head` and read `$?` (that is the pipe's status): redirect to a log file, then test `$?` and read the log.
- CI runs the same pinned hooks, but it is the source of truth (it lints with the UI build, a stale local `build/` may not). If CI lint fails and the local run is clean, change the code so the flagged construct is gone (do not assume the first fix worked), then watch CI again.
- Never report "lint passed" or "tests passed" without having checked the exit code of that run.

**Branches and PRs**
- Small, low-risk changes (docs, typos, comments, scripts and tooling, small fixes, test-only changes, chores) are committed directly to `main` and pushed. No branch, no PR.
- Use a feature branch plus PR only for large features or risky changes (new modules or milestones, file format changes, behavior changes across layers, anything hard to revert). Name it `feat/...` or `fix/...`.
- Do not create a branch "by default" for every change.

**Merging a PR** (in this order, never skip a step)
1. Review the diff (read it; run `/code-review` for non-trivial changes) and fix findings.
2. Wait for CI: `gh pr checks <n> --watch`. The watch can end with "no checks reported" before CI registers: re-check until the jobs really ran. Merge only when every job passed.
3. Merge with `gh pr merge <n> --merge --delete-branch`, then `git checkout main && git pull --ff-only` and `git branch -d <branch>` for the local copy. A merged branch is never left behind, locally or on the remote.
4. If CI on `main` turns red after a merge, fixing it comes before any new work.
