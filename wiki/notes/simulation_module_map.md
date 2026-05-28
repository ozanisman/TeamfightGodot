# Simulation module map (native)

Hot-path logic lives under `native/src/simulation/`. `TeamfightSimulationCore` is Godot glue, match lifecycle, and thin delegators.

| Module | Responsibility |
|--------|----------------|
| `sim_world.hpp` | `SimWorld` view, `SimHostCallbacks`, `uc()` |
| `sim_stats` / `sim_stats.inl.hpp` | Effective stats, movement speed multiplier |
| `sim_spatial` | Grid buckets, stamp generation |
| `sim_aoe.hpp` | `for_each_unit_in_circle` / `for_each_unit_in_shape` |
| `sim_targeting` | Scoring, selection, `prepare_tick_context` |
| `sim_targeting_strategies` | Role strategy tables, `build_role_strategy_cache` |
| `sim_channel` | Channel tick (interrupt/complete/tick effects) |
| `sim_stats_modifiers` | Stat stacks, duration modifiers, consume/set stacks |
| `sim_damage` | Defense, modifiers, `apply_damage` hub |
| `sim_status` | CC, heal, shield, AOE status, `resolve_aoe_direction` |
| `sim_periodic` | DoT/HoT, AOE damage/taunt, knockback, reflect |
| `sim_combat` | Cast, auto-attack, projectiles, post-attack/heal |
| `sim_movement` | Move toward target, kite, dash collision |
| `sim_unit_tick` | Per-unit tick phases (`_update_unit` sequencer) |
| `sim_effects_compile` / `sim_effects_exec` | Effect VM (`SimMatchHost` for spawn/minion catalog) |
| `sim_catalog` | Champion/minion JSON, balance patches |
| `sim_match` | `_build_summary` / stats summary |

## Adding features

- **New opcode:** `sim_effects_compile.cpp` + handler in `sim_effects_exec.cpp`; delegate damage/status/periodic as needed.
- **New CC or heal:** `sim_status.cpp` + exec opcode.
- **New AoE shape:** `sim_aoe.hpp` `shape_contains` + compile metadata; see `wiki/concepts/aoe_shapes.md`.
- **New DoT/HoT:** `sim_periodic.cpp` + exec opcode.

Recursion for nested effects stays on `SimHostCallbacks::execute_effect` → coordinator `_execute_effect` → `sim::effects::execution::execute`.
