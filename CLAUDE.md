# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

tinysprite is a pixel art / 2D sprite animation editor written in C++17 (core, I/O and UI; SDL2 + Dear ImGui for the frontend). The project is built incrementally by milestone; progress, remaining steps and known gotchas are tracked in `docs/MILESTONES.md` (update it when a milestone finishes). A map of the modules and data flow is in `docs/ARCHITECTURE.md`. Planned scope: drawing tools first, then animation (timeline, onion skin, GIF/spritesheet export), then game pipeline features (tilemaps, JSON export, headless CLI).

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

Format/lint (needs `clang-format` and `clang-tidy`; K&R style, 4-space indent, config in `.clang-format` / `.clang-tidy`):

```sh
scripts/format.sh           # apply formatting (--check to verify only)
scripts/lint.sh             # clang-tidy via build/compile_commands.json (covers src/ui when the UI is built)
scripts/lint.sh --changed   # only .cpp files that differ from main (and those including a changed header): fast
scripts/setup-hooks.sh      # once per clone: pre-commit = format check, pre-push = format + lint --changed
```

CI (`.github/workflows/ci.yml`) has three separate jobs: `lint` (shellcheck, format check, clang-tidy), `linux` (ASan/UBSan build and tests, UI included) and `windows` (MinGW build and tests). The clang tools are pinned to the version in `scripts/clang-version` (the one CI installs; `scripts/install-deps.sh` installs it locally, and the scripts refuse to run without it; the editor uses `clangd-18` / `clang-format-18` via `.vscode/settings.json` and `.devcontainer/`, kept in step by `scripts/check-clang-version.sh` in the `lint` job). Run format and lint before committing; `thirdparty/` is excluded from both.

## Architecture

- **Layering is enforced by CMake targets**: `tinysprite_core` (no SDL/ImGui, ever) → `tinysprite_io` (core + stb) → `tinysprite_ui` (core + io + SDL2 + ImGui) → executable. `tests/test_*.cpp` link only core/io; UI tests (`tests/ui/ui_*.cpp`, only when the UI is built) link `tinysprite_ui` and run under `SDL_VIDEODRIVER=dummy`. Never include SDL/ImGui headers from `src/core` or `src/io`.
- **Public headers** live in `include/tinysprite/` (core and io only); internal headers stay next to sources in `src/`.
- **Document model**: `Sprite → Layers × Frames → Cel` (pixel buffer), with a Palette, frame durations and tags. Cels are shared (`shared_ptr`) when frames are linked, and removed frames / layers live on inside history steps; do not flatten this structure.
- **Undo/Redo**: every edit goes through the history module. One undo step = one user stroke; storage is a pixel delta (index, old, new). Edits that are not pixels (layers, frames, tags, links, palette) are `History::pushAction` steps in `*_ops.h`. Do not mutate cels outside this path.
- **Rendering** is CPU compositing into one texture with nearest sampling, re-composited by dirty rect.
- File format `.pxa` is versioned and chunked (length-prefixed) for forward compatibility.

## Conventions

- **TDD**: write the failing test first (`tests/test_<feature>.cpp`, one file per feature, using `tests/minitest.h`: `TEST`, `RUN`, `ASSERT_*`), then implement. New `test_*.cpp` files are picked up automatically by CMake globbing (re-run cmake).
- Tool/raster behavior is tested by feeding point lists and comparing golden pixel arrays.
- C++17, no raw owning pointers: document types (`Cel`, `Layer`, `Sprite`, `Palette`, `History`, `Clip`) are RAII classes, live strokes (`PencilStroke`, `ShapeStroke`, `MoveStroke`) are created by a static `begin()` that returns a `std::unique_ptr` (null if the history already has an open stroke) and cancel themselves when destroyed unfinished. Invalid sizes throw `std::invalid_argument`; operations that can be refused (stroke already active, index out of range) return `bool` or null instead of throwing. Rasterizers report pixels through a `std::function`. Tests run under ASan/UBSan. UI code uses classes where state and behavior go together (`Editor`, `Drag` subclasses per tool, panels, `Canvas`, `App`).
- Commits: English only, following Conventional Commits (https://www.conventionalcommits.org/ko/v1.0.0/), e.g. `feat(core): add flood fill`.
- Record dependency name, pinned version and license in `thirdparty/README.md` when adding anything to `thirdparty/`.

## Workflow (mandatory)

These rules exist because they were broken repeatedly; follow them without being reminded.

**Lint and format**
- Run `scripts/format.sh --check` and `scripts/lint.sh` (or `--changed` while iterating) before every commit that touches C++ or scripts, and fix everything they report. Run the full `scripts/lint.sh` before opening or updating a PR.
- Judge a check by its real exit code. Never pipe it through `tail`/`head` and read `$?` (that is the pipe's status): redirect to a log file, then test `$?` and read the log.
- A local pass does not prove CI passes: CI uses a different clang-tidy version. CI is the source of truth. If CI lint fails and the local run is clean, change the code so the flagged construct is gone (do not assume the first fix worked), then watch CI again.
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
