# AGENTS.md

## Rules
- Make the smallest correct change.
- Do not modify unrelated code.
- Do not introduce new abstractions or dependencies without clear need.
- Preserve existing behavior unless explicitly changing it.

## Style
- Be concise. No fluff or narration.
- Output code by default. Explain only if necessary or requested.

## Code
- Native hot/cold units: keep `_unit_cold` in lockstep with `_units` (same push/clear pairs); use `_uc(u)` only when `u` references an element inside `_units`, never a local `UnitState` copy.
- No magic numbers
- Use reusable effect classes
- Prefer composition over complex logic.
- Keep definitions declarative and strongly typed.
- Comment only when required for non-obvious context.

## Testing
- Update tests only if behavior changes.
- Reuse existing test helpers.
- Run relevant checks.
- For Godot runs, use `run_godot.ps1` for headless/startup checks so file logging always has an explicit target.
- If a Godot command crashes before scene load, verify the launch path first before changing gameplay code.
- Before any test or smoke run, check every edited GDScript with `--check-only` first (`scripts/tools/check_gdscript_preload.gd`; preload/compile only). Dashboard loader + scene smoke is `--check-stats-dashboard`.
- Do not start long smoke scenes until all edited scripts preload cleanly.
- If `--check-only` hangs, stop and remove load-time work from edited scripts before running smoke tests.
- Prefer `_init()` for object creation; avoid top-level `new()` or other heavy work in script members.
- Use a hard timeout on `run_godot.ps1` and kill hung Godot processes instead of waiting indefinitely.
- Run `--check-only` before long smoke scenes when editing GDScript helpers.
- Keep explicit types in helpers that parse `Variant` data; this project treats those warnings as errors.
- For native TeamfightGodot changes, use the canonical validation sequence: `cmake --build native/build --config Release`, `.\run_godot.ps1 -- --check-only`, `.\run_godot.ps1 -- --check-native-load`, `.\run_godot.ps1 -- --check-match-telemetry`, `.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json`, and `.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=1`.
- Call `clear()` on batch match engines after each match so unit refs do not leak across runs.
- Before declaring work finished after any Godot run, verify all Godot processes are terminated

## Done
- Minimal change.
- Correct behavior.
- Checks pass.
