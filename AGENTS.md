# AGENTS.md

## Change Discipline

- Make the smallest complete change that resolves the request.
- Do not modify unrelated code.
- Do not introduce abstractions, refactors, or dependencies unless required for correctness.
- Preserve existing behavior unless explicitly requested or approved.
- Do not implement speculative changes; ask questions when implementation details are unclear.

## Communication/Output

- Be concise. Provide explanations only when required or requested.
- When proposing changes, prefer minimal diffs over prose.
- Do not use emojis.
- Prioritize accuracy over agreement.
- Be direct and candid even when contradicting user.

## Code

- Native hot/cold units: keep `_unit_cold` in lockstep with `_units` (same push/clear pairs); use `_uc(u)` only when `u` references an element inside `_units`, never a local `UnitState` copy.
- No magic numbers.
- Use reusable effect classes.
- Prefer composition over complex logic.
- Keep definitions declarative and strongly typed.
- Comment only when required for non-obvious context.
- Prefer `_init()` for object creation; avoid top-level `new()` or other heavy work in script members.
- Keep explicit types in helpers that parse `Variant` data; this project treats those warnings as errors.

## Wiki

- Code is the source of truth. If code conflicts with wiki, prefer code and flag the inconsistency.
- Consult the wiki when relevant context is needed.
  - `wiki/README.md`
  - `wiki/notes/native_agent_guide.md`
  - `wiki/notes/simulation_module_map.md`

## Testing

- Update tests only if behavior changes.
- Reuse existing test helpers.
- Run relevant checks.
- For Godot runs, use `run_godot.ps1` for headless/startup checks so file logging always has an explicit target.
- If a Godot command crashes before scene load, verify the launch path first before changing gameplay code.
- Before any test or smoke run, check every edited GDScript with `--check-only` first (`scripts/tools/check_gdscript_preload.gd`; preload/compile only). Dashboard loader + scene smoke is `--check-stats-dashboard`.
- Do not start long smoke scenes until all edited scripts preload cleanly.
- If `--check-only` hangs, stop and remove load-time work from edited scripts before running smoke tests.
- Use a hard timeout on `run_godot.ps1` and kill hung Godot processes instead of waiting indefinitely.
- Run `--check-only` before long smoke scenes when editing GDScript helpers.
- For native TeamfightGodot changes, run the canonical validation gate in [README.md#validation-gate](README.md#validation-gate).

- Call `clear()` on batch match engines after each match so unit refs do not leak across runs.
- Before declaring work finished after any Godot run, verify all Godot processes are terminated.

## Done

- Change is minimal and complete.
- Behavior changes explicitly listed.
- Checks pass.
- Wiki updated if stable concepts were introduced, removed, or invalidated.
