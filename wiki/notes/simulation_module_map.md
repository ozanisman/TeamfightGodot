# Simulation module map (native)

Hot-path logic lives under `native/src/simulation/`. `TeamfightSimulationCore` is Godot glue, match lifecycle, and thin delegators.

| Module | Responsibility |
|--------|----------------|
| `sim_world.hpp` | `SimWorld` view, `SimHostCallbacks`, `uc()` |
| `sim_stats` / `sim_stats.inl.hpp` | Effective stats, movement speed multiplier |
| `sim_spatial` | Grid buckets, stamp generation |
| `sim_aoe.hpp` | `for_each_unit_in_circle` / `for_each_unit_in_shape` |
| `sim_targeting` (+ `_internal`, `_score`, `_select_enemy`, `_select_ally`, `_select_effect`, `_context`) | Scoring, selection, `select_targets`, `prepare_tick_context` |
| `sim_match_spawn` | Random summon spawn positions with expansion |
| `sim_targeting_strategies` | Role strategy tables, `build_role_strategy_cache` |
| `sim_channel` | Channel tick (interrupt/complete/tick effects) |
| `sim_stats_modifiers` | Stat stacks, duration modifiers, consume/set stacks |
| `sim_damage` (+ `_internal`, `_modifiers`, `_apply`) | Defense, multiplier modifiers, `apply_damage` / reflect / ally defense |
| `sim_match_runtime_state` | `MatchRuntimeState` refs bundle; `runtime_from()` (coordinator hot path uses direct wiring) |
| `sim_status` (+ `_internal`, `_cc`, `_heal`, `_aoe`) | CC, heal/shield/mana, AOE status, `resolve_aoe_direction` |
| `sim_effects_exec_status` (+ `_heal`, `_cc`, `_channel`, `_mana`) | Status opcode execution (thin dispatcher) |
| `sim_viewer` (+ `_fx`, `_snapshot`) | FX buffer, `build_tick_snapshot` |
| `sim_periodic` (+ `_internal`, `_dot_hot`, `_aoe`, `_reflect`) | DoT/HoT, AOE damage/taunt, knockback, reflect |
| `sim_combat` (+ `_internal`, `_actions`, `_projectile`) | Cast, auto-attack, projectiles, post-attack/heal |
| `sim_movement` | Move toward target, kite, dash collision |
| `sim_unit_tick` (+ `_internal`, `_cooldowns`, `_regen`, `_cast`, `_combat`, `_movement`) | Per-unit tick phases (`update_unit` sequencer) |
| `sim_effects_host` | `EffectExecBindings`, `host_execute_effect` trampoline (no coordinator hop) |
| `sim_effect_kinds.inl.hpp` | Shared lazy `StringName` accessors for effect kinds / trace keys |
| `sim_effects_compile` (+ `_internal`, `_damage`, `_status`, `_aoe`, `_spawn`) / `sim_effects_exec` (+ `_damage`, `_status`, `_spawn`, `_aoe`) | Effect compile + VM (`SimMatchHost`: spawns, projectiles, catalog, instance id) |
| `sim_coordinator_host.cpp` | `sim_host_*` trampolines; `CoordinatorHostAccess` (viewer, world, units/cold, benchmark) |
| `sim_match_benchmark_stats_internal` | Stat entry helpers, matchup aggregation for stats partial |
| `sim_profile` / `sim_profile_counters.hpp` | `TEAMFIGHT_SIM_PROFILE` reset + stderr JSON; `Counters` + `RuntimeFlags`; `unit_tick_profile()` |
| `sim_match_benchmark_host.hpp` | `GeneratedMatchHost` alias for batch runners |
| `sim_catalog` | Champion/minion JSON, balance patches |
| `sim_match` | `_build_summary` / stats summary |
| `sim_match_loop` | `step_tick`, `simulate`, sudden-death driver (`MatchLoopHost` callbacks for coordinator hot paths) |
| `sim_match_lifecycle` | Death scoring/assists/takedowns, respawn reset, spawn slot assign/release |
| `sim_match_roster` | Unit registration, pending spawn drain, team append |
| `sim_unit_builder` | Champion/minion unit construction |
| `sim_viewer` | `ViewerFxBuffer`, FX record helpers, `build_tick_snapshot`; `ViewerHooks` callbacks |
| `sim_match_benchmark` / `sim_match_benchmark_stats` | Generated batch sim-only + stats partial aggregation |
| `sim_match_benchmark_host` | `GeneratedMatchHost` accessors for benchmark setup |
| `sim_coordinator_match.cpp` | Godot match/batch API implementations |
| `sim_coordinator_tick.cpp` | `_update_unit` / `_update_projectiles` + unit-tick profile wiring |
| `sim_coordinator_bindings.cpp` | `_bind_sim_host`, exec bindings, combat/unit-tick/channel host hooks |
| `sim_coordinator_state.cpp` | Roster/score/spawn builders, `_reset_runtime_state`, `_sim_world` / `_match_loop_state` |
| `sim_coordinator_catalog.cpp` | Catalog load, compile/finalize reflect passives |
| `sim_coordinator_targeting.cpp` | Targeting frame sync, select helpers, tick context prep |
| `sim_coordinator_viewer.cpp` | Trace emit, tick snapshot, score breakdown |
| `sim_coordinator_host.hpp` | `sim_host_*` trampolines; `CoordinatorHostAccess` (viewer/world) |

## Adding features

- **New opcode:** `sim_effects_compile_*.cpp` (pick category TU) + handler in `sim_effects_exec.cpp`; delegate damage/status/periodic as needed.
- **New CC or heal:** `sim_status.cpp` + exec opcode.
- **New AoE shape:** `sim_aoe.hpp` `shape_contains` + compile metadata; see `wiki/concepts/aoe_shapes.md`.
- **New DoT/HoT:** `sim_periodic.cpp` + exec opcode.

Nested effects: `SimHostCallbacks::execute_effect` → `sim::effects::host_execute_effect` → `sim::effects::execution::execute`.
