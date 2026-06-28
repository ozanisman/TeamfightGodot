# Wiki

A living knowledge base.

## Agents (read this first for native C++)

**[native_agent_guide.md](notes/native_agent_guide.md)** — canonical routing, task→file map, invariants, denylists, validation commands.  
Then [simulation_module_map.md](notes/simulation_module_map.md).

## Structure

- **notes** — atomic observations, module maps, perf status
- **concepts** — stable definitions of systems

## Native simulation (current)

| Doc | Purpose |
|-----|---------|
| [native_agent_guide.md](notes/native_agent_guide.md) | **Agent entry** — routing, invariants, do-not-read list |
| [simulation_module_map.md](notes/simulation_module_map.md) | Module ownership, coordinator split, where to add features |
| [native_draft_ai.md](notes/native_draft_ai.md) | Draft AI architecture, scoring model, validation results |
| [native_draft_ai_baseline.md](notes/native_draft_ai_baseline.md) | Draft AI baseline metrics (Phase 29 partial comp scoring) |
| [draft_order_bias_audit.md](notes/draft_order_bias_audit.md) | Draft order bias audit (Phase 38) — structural advantage analysis |
| [performance_optimization_status.md](notes/performance_optimization_status.md) | Dated benchmark + validation gate |
| [algorithmic_optimization_analysis.md](notes/algorithmic_optimization_analysis.md) | Deferred perf ideas (profiling backlog) |
| [ui_architecture_roadmap.md](notes/ui_architecture_roadmap.md) | UI theme/component direction, deferred ideas, and current avoid-list |

**Coordinator:** [`native/src/teamfight_simulation_core.{hpp,cpp}`](../native/src/teamfight_simulation_core.hpp) plus `sim_coordinator_{match,state,catalog,targeting,viewer,tick,bindings,host}.cpp`. Shared host API: [`sim_coordinator_host.hpp`](../native/src/simulation/sim_coordinator_host.hpp) only (no per-TU empty coordinator headers).

## Reference

| Doc | Purpose |
|-----|---------|
| [command_reference.md](notes/command_reference.md) | Headless flags and `run_godot.ps1` routing |
| [glossary.md](notes/glossary.md) | Term definitions (opcode, fixture, parity, etc.) |
| [DATA_SOURCE.md](../fixtures/goldens/DATA_SOURCE.md) | Champion/kit JSON source of truth and regeneration |

**Stats directories:** Production draft AI CSVs live under `res://model_stats/stats_output_100k/` (produce via `--generate-stats`). Research and ceiling tooling uses `res://stats_output/` (gitignored ad-hoc outputs). See [native_draft_ai.md](notes/native_draft_ai.md) and [draft_prediction_context.md](notes/draft_prediction_context.md).

## Troubleshooting

| Doc | Purpose |
|-----|---------|
| [gdextension_error_1114_thread_local.md](notes/gdextension_error_1114_thread_local.md) | GDExtension load error 1114 — **active invariant** (resolved incident, rule still applies) |
| [godot_windows_headless_file_issue.md](notes/godot_windows_headless_file_issue.md) | Headless file I/O on Windows — **historical, resolved** |
| [gdscript_preload_circular_dependency.md](notes/gdscript_preload_circular_dependency.md) | GDScript preload circular dependency |

## Draft AI (extended)

| Doc | Purpose |
|-----|---------|
| [draft_prediction_context.md](notes/draft_prediction_context.md) | Agent briefing on draft prediction signal and limitations |

## Concepts

| Doc | Purpose |
|-----|---------|
| [effect_system.md](concepts/effect_system.md) | Modular opcode-based ability/ultimate/passive execution |
| [targeting_ai.md](concepts/targeting_ai.md) | Scoring-based enemy/ally selection with role strategies |
| [combat_pipeline.md](concepts/combat_pipeline.md) | Tick-based simulation loop and match end conditions |
| [champion_system.md](concepts/champion_system.md) | Champion data structure, roles, kit system |
| [damage_calculation.md](concepts/damage_calculation.md) | Damage types, defense multipliers, lifesteal, reflect |
| [status_effects.md](concepts/status_effects.md) | Crowd control and state modifiers |
| [periodic_effects.md](concepts/periodic_effects.md) | DoT/HoT effects, stacking, tick modes |
| [projectile_system.md](concepts/projectile_system.md) | Projectile movement, collision, splash damage |
| [shield_system.md](concepts/shield_system.md) | Shield mechanics, decay, tracking |
| [mana_system.md](concepts/mana_system.md) | Mana resource mechanics |
| [match_telemetry.md](concepts/match_telemetry.md) | Per-unit telemetry counters |
| [channel_system.md](concepts/channel_system.md) | Sustained interruptible channel effects |
| [spawn_system.md](concepts/spawn_system.md) | Initial team composition and positioning |
| [respawn_system.md](concepts/respawn_system.md) | Death, respawn timing, spawn slots |
| [movement_system.md](concepts/movement_system.md) | Unit movement, collision avoidance, kiting |
| [minion_system.md](concepts/minion_system.md) | Minion definitions, summon ally effects, lifecycle |
| [assist_system.md](concepts/assist_system.md) | Assist credit based on damage contribution |
| [balance_patches.md](concepts/balance_patches.md) | Versioned champion stat/ability overrides |
| [kit_system.md](concepts/kit_system.md) | Ability/ultimate/passive swap mechanism |
| [matchup_tracking.md](concepts/matchup_tracking.md) | Winrate tracking for champion relationships |
| [schema_contract.md](concepts/schema_contract.md) | Parity testing framework and signatures |
| [rng_streams.md](concepts/rng_streams.md) | Deterministic RNG and seeding |
| [spatial_grid.md](concepts/spatial_grid.md) | Spatial grid optimization for targeting/AOE |
| [stat_modifiers.md](concepts/stat_modifiers.md) | Temporary and permanent stat modifiers |
| [aoe_shapes.md](concepts/aoe_shapes.md) | Area-of-effect targeting shapes |
| [role_configs.md](concepts/role_configs.md) | Role-specific stat modifiers and passive hooks |
| [determinism.md](concepts/determinism.md) | Determinism requirements and parity testing |
| [native_gdscript_duality.md](concepts/native_gdscript_duality.md) | Native C++ backend via GDExtension |

## Principles

- Write ideas down as they emerge
- Keep files small and focused
- Merge and refine over time
- Prefer linking over hierarchy
