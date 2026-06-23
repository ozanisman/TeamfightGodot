# Native simulation — agent guide

**Use this note first** for any task touching `native/src/` simulation or `TeamfightSimulationCore`. Gameplay semantics live in `wiki/concepts/`; **file ownership and edit routing** live here and in [simulation_module_map.md](simulation_module_map.md).

## Read order

| Step | Doc | Use for |
|------|-----|---------|
| 1 | This file | Routing, invariants, denylists |
| 2 | [simulation_module_map.md](simulation_module_map.md) | Full module table, “adding features” |
| 3 | Relevant `wiki/concepts/*.md` | Opcode behavior, targeting rules, etc. |
| 4 | [performance_optimization_status.md](performance_optimization_status.md) | Validation commands and benchmark numbers |

## Canonical paths (repo root)

| Area | Path |
|------|------|
| Godot coordinator | `native/src/teamfight_simulation_core.{hpp,cpp}` |
| Host callbacks / unit access | `native/src/simulation/sim_coordinator_host.{hpp,cpp}` |
| Simulation modules | `native/src/simulation/` |
| Types / opcodes | `native/src/simulation/simulation_types.hpp` |
| Stat getters (X-macro) | `native/src/simulation/sim_stats.inl.hpp` (include via `sim_stats.hpp`) |
| Effect kind strings | `native/src/simulation/sim_effect_kinds.inl.hpp` |
| Draft AI (off hot path) | `sim_draft_ai_*` (pick/ban), `sim_draft_recommender.*` (winner prediction); see [native_draft_ai.md](native_draft_ai.md) |
| Build list | `native/CMakeLists.txt` → `TEAMFIGHT_SIMULATION_SOURCES` |

## Task → where to edit

| Task | Primary files |
|------|----------------|
| Match / batch API, `run_match`, `advance_one_tick` | `sim_coordinator_match.cpp`, `sim_match_loop.cpp` |
| Reset / roster / loop state builders | `sim_coordinator_state.cpp` |
| Catalog load, balance patches | `sim_catalog.cpp`, `sim_coordinator_catalog.cpp` |
| Compile effect from dict | `sim_effects_compile*.cpp`, `sim_coordinator_catalog.cpp` |
| Execute opcode | `sim_effects_exec*.cpp`, `sim_effects_host.cpp` |
| New opcode | `sim_effects_compile_*.cpp` + `sim_effects_exec.cpp` (or category TU) |
| Damage / armor / reflect | `sim_damage_*.cpp`, `sim_damage.hpp` |
| Targeting / retarget | `sim_targeting_*.cpp` |
| Unit tick order | `sim_unit_tick.cpp`, `sim_unit_tick_*.cpp` |
| Combat / autos / projectiles | `sim_combat_*.cpp` |
| Movement / kite / separation | `sim_movement.cpp` (spatial cache via `SimWorld`) |
| DoT / HoT / periodic AoE | `sim_periodic_*.cpp` |
| CC / heal / shield status | `sim_status_*.cpp` |
| Godot bindings / class API | `teamfight_simulation_core.cpp` |
| Batch benchmark | `sim_match_benchmark*.cpp`, `sim_match_benchmark_host.hpp` |
| Draft pick/ban recommend | `sim_draft_ai_{recommender,evaluator,stats_database}.cpp`, `sim_draft_ai_types.hpp` |
| Draft winner prediction / analysis | `sim_draft_recommender.cpp` |
| `sim_host_*` trampoline | `sim_coordinator_host.cpp` |

## Invariants (do not break)

- `_units` and `_unit_cold` stay in lockstep; use `CoordinatorHostAccess::uc()` for elements inside `_units`, never a local `UnitState` copy.
- `MatchLoopState` / `MatchLoopHost` / `MatchRosterState` / `SpawnSlotState`: returned by value (reference aggregates). **Store in locals** before passing to any `T &` parameter (`simulate`, `step_tick`, `process_pending_spawns`, `register_built_unit`, `handle_death`, `assign_spawn_slot`, etc.) — never pass `_match_*_state()` / callback return temporaries to non-const references. `const UnitBuilderHost &` may bind to `_unit_builder_host()` temporaries.
- Effect path: `SimHostCallbacks::execute_effect` → `sim::effects::host_execute_effect` → `sim::effects::execution::execute` (avoid new coordinator hops on hot path).
- After batch matches: call `clear()` on engines so unit refs do not leak across runs.
- Native hot/cold: keep `_unit_cold` aligned with `_units` (same push/clear pairs).
- No namespace/file-scope `static`/`thread_local` Godot types (`String`/`StringName`/`Dictionary`/`Array`, or structs holding them): they init in `DllMain` before the API loads → load fails with **Error 1114**. Use function-local `static thread_local` or pointers. See [gdextension_error_1114_thread_local.md](gdextension_error_1114_thread_local.md).

## Removed — do not recreate

- `sim_damage.cpp`, `sim_status.cpp`, `sim_viewer.cpp` (stub TUs)
- Empty `sim_coordinator_{catalog,match,tick,…}.hpp` shells
- `TargetScoreContext::use_spatial` (removed; `use_spatial_broad_phase()` remains for movement/AoE only)
- See [algorithmic_optimization_analysis.md](algorithmic_optimization_analysis.md) for deferred perf experiments (not in tree unless re-profiled)

## Validation (after native edits)

Run the canonical gate in [README.md#validation-gate](../../README.md#validation-gate). Confirm no lingering Godot processes when finished.

## Extended commands

Full flag list (core gate, UI smoke, simulation checks, draft research, perf helpers): [command_reference.md](command_reference.md).

## Interactive UI

| Command | Purpose |
|---------|---------|
| `.\run_godot.ps1 --main-menu` | Main menu hub (viewer / stats / draft) |
| `.\run_godot.ps1 --simulation-viewer` | Simulation viewer directly |
| `.\run_godot.ps1 --simulation-viewer --stats-dashboard` | Stats dashboard directly |

## Concept → implementation index

| Concept doc | Native modules |
|-------------|----------------|
| [effect_system.md](../concepts/effect_system.md) | `sim_effects_compile*`, `sim_effects_exec*`, `sim_effects_host` |
| [combat_pipeline.md](../concepts/combat_pipeline.md) | `sim_match_loop`, `sim_unit_tick*`, `sim_combat*`, `sim_movement` |
| [targeting_ai.md](../concepts/targeting_ai.md) | `sim_targeting*`, `sim_targeting_strategies` |
| [damage_calculation.md](../concepts/damage_calculation.md) | `sim_damage*`, `sim_stats_modifiers` |
| [periodic_effects.md](../concepts/periodic_effects.md) | `sim_periodic*` |
| [status_effects.md](../concepts/status_effects.md) | `sim_status*`, `sim_effects_exec_status*` |
| [spatial_grid.md](../concepts/spatial_grid.md) | `sim_spatial`, `sim_aoe.hpp`, `SpatialBucketFillCache` in `sim_world.hpp` |
| [projectile_system.md](../concepts/projectile_system.md) | `sim_combat_projectile`, `sim_combat_actions` |
| [shield_system.md](../concepts/shield_system.md) | `sim_status_heal`, `sim_effects_exec_heal` |
| [mana_system.md](../concepts/mana_system.md) | `sim_status_heal`, `sim_unit_tick_regen`, `sim_effects_exec_mana` |
| [channel_system.md](../concepts/channel_system.md) | `sim_channel`, `sim_effects_exec_channel` |
| [spawn_system.md](../concepts/spawn_system.md) | `sim_match_spawn`, `sim_match_roster`, `sim_unit_builder` |
| [respawn_system.md](../concepts/respawn_system.md) | `sim_match_lifecycle`, `sim_match_spawn` |
| [movement_system.md](../concepts/movement_system.md) | `sim_movement`, `sim_unit_tick_movement` |
| [minion_system.md](../concepts/minion_system.md) | `sim_catalog`, `sim_unit_builder`, `sim_match_spawn` |
| [assist_system.md](../concepts/assist_system.md) | `sim_match_lifecycle`, `sim_damage_apply` |
| [balance_patches.md](../concepts/balance_patches.md) | `sim_catalog` |
| [kit_system.md](../concepts/kit_system.md) | `sim_catalog`, `kit_catalog.gd` |
| [champion_system.md](../concepts/champion_system.md) | `sim_catalog`, `champion_catalog.gd` |
| [matchup_tracking.md](../concepts/matchup_tracking.md) | `matchup_tracker.gd` (GDScript tooling) |
| [schema_contract.md](../concepts/schema_contract.md) | `parity_tools.gd`, `simulation_schema.gd`, golden fixtures |
| [rng_streams.md](../concepts/rng_streams.md) | `python_random.hpp`, `sim_match_loop` |
| [stat_modifiers.md](../concepts/stat_modifiers.md) | `sim_stats_modifiers`, `sim_effects_exec` |
| [aoe_shapes.md](../concepts/aoe_shapes.md) | `sim_aoe.hpp`, `sim_effects_exec_aoe` |
| [role_configs.md](../concepts/role_configs.md) | `sim_catalog`, `champion_catalog.gd` role configs |
| [determinism.md](../concepts/determinism.md) | `sim_match_loop`, golden fixtures via `headless_runner.gd` |
| [native_gdscript_duality.md](../concepts/native_gdscript_duality.md) | `teamfight_simulation_core`, `native_simulation_backend.gd` |
