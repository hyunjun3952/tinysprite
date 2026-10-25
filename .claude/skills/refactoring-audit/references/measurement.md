# Measuring complexity and performance

Numbers come before opinions. Record the command, the build type and the result.

## Cognitive complexity (limit 15; look at 12 and above)

CI already fails above 15 (`readability-function-cognitive-complexity`, config in `.clang-tidy`, `src/` only). To list functions that are close, run
clang-tidy with a lower threshold; this reports without failing the build. The check warns only when the score is **above** the
threshold, so threshold 11 lists every function scoring 12 or more:

```sh
tidy=$(ls -d ~/.cache/pre-commit/*/py_env-*/bin/clang-tidy | head -1)
cmake -S . -B build >/dev/null            # makes compile_commands.json
$tidy -p build --quiet \
  --checks='-*,readability-function-cognitive-complexity' \
  --config='{CheckOptions: {readability-function-cognitive-complexity.Threshold: "11"}}' \
  --header-filter='^$' $(git ls-files 'src/*.cpp') > LOG 2>&1; echo "exit=$?"
grep 'cognitive complexity' LOG
```

`git ls-files 'src/*.cpp'` lists every source file at any depth (a hand-written `src/ui/*/*.cpp` misses `src/core/tools/`).

PowerShell (tools and exit codes: `../../_shared/audit-conventions.md`, "Tools on Windows"):

```powershell
$files = git ls-files 'src/*.cpp'
clang-tidy -p build --quiet --checks='-*,readability-function-cognitive-complexity' `
  --config='{CheckOptions: {readability-function-cognitive-complexity.Threshold: "11"}}' `
  --header-filter='^$' $files > $log 2>&1; $LASTEXITCODE
Select-String $log -Pattern 'cognitive complexity of'
```

The scan over all of `src` takes several minutes: run it in the background with the log in the scratchpad. A clang-tidy exit 0 here only means it ran;
the check reports without failing.

Each warning prints the exact score ("has cognitive complexity of 13 (threshold 12)"). Re-run after the change for the "after" number.
src/ui files need the UI build in `build/`. If the pinned clang-tidy is missing, run `pre-commit run clang-tidy-full --hook-stage manual --files src/core/sprite.cpp` once to create it; if the config override is rejected, edit nothing in the repo, copy the file to the scratchpad and pass a temporary `--config-file`.
Lines per function and parameter counts are a second signal; compare them with `wc -l` on the function range or by reading.

Hot spots to prioritize: complexity 12 or more AND edited often (`git log` count, see SKILL.md Step 1).

Tools on Windows (pre-commit on `PATH`, the pinned clang tools, the exit-code rule, the UI exe): `../../_shared/audit-conventions.md`.

## Performance

Judge only after measuring. A guess ("this loop looks slow") is a hypothesis to test, not a finding.

1. **Use a Release build in a separate build dir.** The default build has ASan/UBSan on and is several times slower and distorts hot spots.
   ```sh
   cmake -S . -B SCRATCH/build-rel -DCMAKE_BUILD_TYPE=Release -DTINYSPRITE_SANITIZE=OFF
   cmake --build SCRATCH/build-rel
   ```
   `SCRATCH` is the session scratchpad directory. Do not reconfigure the repo's `build/` for this.
2. **Define the budget.** Interactive paths (a stroke segment, composite of the dirty rect, playback tick, tool preview): must fit a 60 fps frame, 16 ms, with room for ImGui and the GPU upload; give it a few ms. Batch paths (open `.pxa`, export GIF / sheet): feel slow past about 1 s on realistic sizes. If the user has a budget, use theirs.
3. **Use realistic inputs**: sprite sizes the app is used with (for example 64x64, 256x256, 1024x1024), 1 to 100+ frames, several layers, linked cels, a large flood-fill region, a 256-color GIF.
4. **Measure** with a small program in the scratchpad linked against `tinysprite_core` / `tinysprite_io` (not committed unless the user wants a benchmark):
   - `std::chrono::steady_clock`, a warm-up run, then at least 10 runs; report median and worst, not one number.
   - Prevent the call being optimized away (use the result).
   - Profile only after a number exceeds the budget: `perf record -g` / `perf report` if available, or `valgrind --tool=callgrind`; sampling shows where, the timer shows how much.
   - The real app: `build/.../tinysprite --frames N` with `SDL_VIDEODRIVER=dummy` runs headless and gives frame timing without a screen. A GPU/driver stall cannot be seen headless; say so.
5. **Record**: scenario, input size, build type, runs, median / worst, budget, verdict (within budget / over / unknown).

### What counts as a finding

- Over budget on a realistic input: priority 높음.
- Within budget now but the cost grows with an input that will grow (O(n^2) over frames x layers, full re-composite where a dirty rect exists, copying a cel per stroke point): 보통 with the growth measured at two sizes.
- Faster-looking code with no measured gain: drop it. Readability wins ties.

### Rules for performance refactorings

- Keep the optimization separate from structural refactorings (own commit, own before/after numbers).
- Correctness first: the same tests must pass; the dirty-rect, shared-cel and history invariants stay intact.
- Prefer algorithmic fixes (avoid the copy, skip the unchanged region, reserve capacity) over micro-tuning. Do not trade a clear design for a few percent.
- Remove the optimization if re-measuring shows no gain.
