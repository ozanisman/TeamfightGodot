# Simulation module map (native)

> **Agents:** start at [native_agent_guide.md](native_agent_guide.md) (task→file, invariants, denylists). This file is the full module table.

Hot-path logic lives under [`native/src/simulation/`](../../native/src/simulation/). Godot-facing coordinator: [`native/src/teamfight_simulation_core.{hpp,cpp}`](../../native/src/teamfight_simulation_core.hpp) — glue, bindings, and thin delegators (`sim::*` types in the header; no public type-alias mirror of `simulation_types.hpp`).

## Coordinator

| Piece | Role |
|-------|------|
| `teamfight_simulation_core.{hpp,cpp}` | Class definition, Godot bindings, catalog/match entrypoints |
| `sim_coordinator_match.cpp` | `run_match`, batch paths, `begin_match` / `advance_one_tick` |
| `sim_coordinator_state.cpp` | `_match_loop_state`, `_match_roster_state`, `_reset_runtime_state`, `_sim_world` |
| `sim_coordinator_catalog.cpp` | Catalog load, `_compile_effect`, reflect passives |
| `sim_coordinator_targeting.cpp` | Targeting frame sync, select helpers, `_prepare_tick_context` |
| `sim_coordinator_viewer.cpp` | Trace, tick snapshot, score breakdown |
| `sim_coordinator_tick.cpp` | `_update_unit`, `_update_projectiles` |
| `sim_coordinator_bindings.cpp` | `_bind_sim_host`, exec/combat/unit-tick/channel hooks |
| `sim_coordinator_host.{hpp,cpp}` | `sim_host_*` trampolines; `CoordinatorHostAccess` (units/cold, world, viewer) |

Coordinator `.cpp` files include `teamfight_simulation_core.hpp` directly (no empty `sim_coordinator_*.hpp` shells).

**Match loop call pattern:** build `match::MatchLoopState` / `match::MatchLoopHost` locals from `_match_loop_state()` / `_match_loop_host()` before `simulate` / `step_tick` (reference aggregates; do not pass temporaries).

## Native test boundary

Coordinator-independent simulation modules build into `teamfight_simulation_runtime` and link into both the production extension and the test-only extension. `native/tests/teamfight_simulation_test_runner.cpp` constructs typed `SimWorld` fixtures directly; it does not add champions, catalog overlays, or test methods to `TeamfightSimulationCore`. Run the suite with `--check-native-simulation-tests`; production packaging may configure `TEAMFIGHT_BUILD_NATIVE_TESTS=OFF`.

## Simulation modules

| Module | Responsibility |
|--------|----------------|
| `sim_world.hpp` | `SimWorld` view, `SimHostCallbacks`, `SpatialBucketFillCache`, `uc()` |
| `sim_stats.hpp` / `sim_stats.inl.hpp` | `get_effective_*` (X-macro); `distance_between`; `movement_speed_multiplier` in `sim_stats.cpp` |
| `sim_spatial` | Grid buckets, stamp generation, `use_spatial_broad_phase` |
| `sim_aoe.hpp` | `for_each_unit_in_circle` / `for_each_unit_in_shape` |
| `sim_targeting` (+ `_internal`, `_score`, `_select_enemy`, `_ally`, `_effect`, `_context`, `_strategies`) | Scoring, selection, `prepare_tick_context` |
| `sim_match_spawn` | Random summon spawn positions with expansion |
| `sim_channel` | Channel tick (interrupt / complete / tick effects) |
| `sim_stats_modifiers` | Stat stacks, duration modifiers, consume / set stacks |
| `sim_damage.hpp` (+ `_internal`, `_modifiers`, `_apply`) | Defense, multipliers, `apply_damage` / reflect / ally defense (no `sim_damage.cpp` TU) |
| `sim_match_runtime_state` | `MatchRuntimeState`, `runtime_from()` (coordinator tick path uses direct `SimWorld` / loop wiring) |
| `sim_status.hpp` (+ `_internal`, `_cc`, `_heal`, `_aoe`) | CC, heal / shield / mana, AOE status (no stub `sim_status.cpp`) |
| `sim_effects_exec` (+ `_damage`, `_status`, `_heal`, `_cc`, `_channel`, `_mana`, `_spawn`, `_aoe`) | Effect VM; status split TUs + thin dispatcher |
| `sim_viewer.hpp` (+ `_fx`, `_snapshot`) | FX buffer, `build_tick_snapshot` (no stub `sim_viewer.cpp`) |
| `sim_periodic.hpp` (+ `_internal`, `_dot_hot`, `_aoe`, `_reflect`) | DoT / HoT, AOE periodic, knockback, reflect |
| `sim_combat` (+ `_internal`, `_actions`, `_projectile`) | Cast, auto-attack, projectiles |
| `sim_passive_hooks` | Unified execute-hook runner (`post_*`, `on_takedown`, `on_knockback*`) |
| `sim_movement` | Move toward target, kite, separation (spatial bucket cache) |
| `sim_unit_tick` (+ `_internal`, `_cooldowns`, `_regen`, `_cast`, `_combat`, `_movement`) | Per-unit tick sequencer |
| `sim_effects_host` | `EffectExecBindings`, `host_execute_effect` (no coordinator hop) |
| `sim_effect_kinds.inl.hpp` | Shared lazy `StringName` accessors (`sim::effect_kinds::sn_*`) |
| `sim_effects_compile` (+ `_opcodes`, `_internal`, `_damage`, `_status`, `_aoe`, `_spawn`) | Compile pipeline; opcodes in `_opcodes.cpp` |
| `sim_catalog` | Champion / minion JSON, balance patches, effective cache |
| `sim_match` | `_build_summary` / stats summary |
| `sim_match_loop` | `step_tick`, `simulate`, sudden-death driver |
| `sim_match_lifecycle` | Death / assist / takedown, respawn, spawn slots |
| `sim_match_roster` | Unit registration, pending spawns, team append |
| `sim_unit_builder` | Champion / minion unit construction |
| `sim_profile` / `sim_profile_counters.hpp` | `TEAMFIGHT_SIM_PROFILE`, `unit_tick_profile()` |
| `sim_match_benchmark` (+ `_stats`, `_stats_internal`, `_host`) | Batch sim-only, stats partial, `GeneratedMatchHost` |

## Draft AI (native)

Two cooperating namespaces under `native/src/simulation/` provide draft pick/ban recommendation and winner prediction. **Off the match hot path.** Deep design + phase history: [native_draft_ai.md](native_draft_ai.md).

| Module | Namespace | Responsibility |
|--------|-----------|----------------|
| `sim_draft_ai_types.hpp` | `sim::draft_ai` | `DraftStrategy` enum, `DraftStatValue`, pick/ban score-breakdown structs |
| `sim_draft_ai_stats_database` | `sim::draft_ai` | Loads CSV stats (base/synergy/counter winrates, role combinations) from a stats dir; Bayesian smoothing |
| `sim_draft_ai_evaluator` | `sim::draft_ai` | Per-candidate pick/ban scoring (weighted signals) |
| `sim_draft_ai_recommender` | `sim::draft_ai` | **Production** pick/ban recommender (`recommend_picks` / `recommend_bans`); strategy dispatch + lookahead diagnostic |
| `sim_draft_recommender` | `sim::draft` | Winner prediction + analysis tools (stress test, signal influence); **not** used for pick/ban routing |

- **Strategies** (`DraftStrategy`, `sim_draft_ai_types.hpp:12-17`): `NATIVE_FULL` (default/production); `NATIVE_LOOKAHEAD`, `NATIVE_LOOKAHEAD_PICK`, `NATIVE_LOOKAHEAD_BAN` (quarantined experimental, side-bias - see note).
- **Scoring modes** (`sim::draft::ScoringMode`, `sim_draft_recommender.hpp:17-23`): `CERTIFIED_PAIRWISE_PROBABILITY`, `DRAFT_AWARE_PAIRWISE_PROBABILITY`. Legacy additive/multiplicative modes are removed; `score_from_signals` is dead for production (`sim_draft_recommender.cpp:200-238`).
- **Bound API** (`teamfight_simulation_core.cpp`): pick/ban routing -> `get_draft_ai_pick_recommendations`, `get_draft_ai_ban_recommendations` (`sim::draft_ai`). Winner prediction / analysis -> `predict_draft_winner`, `get_draft_recommendation*`, `analyze_draft_signal_influence`, `run_controlled_draft_evaluation`, `run_stress_test*`, `debug_*draft*` (`sim::draft`).
- **Stats input:** CSV dir, default `res://model_stats/stats_output_100k/` (`combat_stats.csv`, `matchup_with.csv`, `matchup_vs.csv`, `role_combinations.csv`).

## Removed / non-build artifacts (do not reintroduce)

- Stub TUs: `sim_damage.cpp`, `sim_status.cpp`, `sim_viewer.cpp` (logic lives in split modules above).
- Empty `sim_coordinator_{catalog,match,…}.hpp` shells.
- One-off refactor scripts under `native/tools/` and `native/src/simulation/_extract_profile.py` (deleted).

## Optional file splits (maintainability only)

Largest `.cpp` TUs (~350–400 lines): `sim_catalog`, `sim_unit_builder`, `sim_periodic_dot_hot`, `sim_stats_modifiers`, `sim_match_lifecycle`, `sim_periodic_aoe`, `sim_effects_exec`. Split only when editing those areas or compile time hurts.

## Adding features

- **New opcode:** `sim_effects_compile_*.cpp` + handler in `sim_effects_exec.cpp` (or category TU).
- **New CC or heal:** `sim_status_{cc,heal,aoe}.cpp` or `sim_status_internal.cpp` + exec opcode.
- **New AoE shape:** `sim_aoe.hpp` + compile metadata; see [aoe_shapes.md](../concepts/aoe_shapes.md).
- **New DoT / HoT:** `sim_periodic_dot_hot.cpp` or `sim_periodic_internal.cpp` + exec opcode.

Nested effects: `SimHostCallbacks::execute_effect` → `sim::effects::host_execute_effect` → `sim::effects::execution::execute`.

## Tooling / IDE

- **Validation:** canonical gate in [README.md#validation-gate](../../README.md#validation-gate), including the focused native module suite before golden parity.
- **clangd:** [`.clangd`](../../.clangd) uses `UnusedIncludes: Strict`. Facade headers use `// IWYU pragma: export` on `.inl.hpp` includes (e.g. `sim_stats.hpp`). `.inl.hpp` files that use Godot types include `using namespace godot;` so standalone parsing works.
