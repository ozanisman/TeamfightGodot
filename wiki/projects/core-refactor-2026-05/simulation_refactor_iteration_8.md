# Simulation Refactor — Iteration 8

Continuation after [simulation_refactor_iteration_7.md](simulation_refactor_iteration_7.md).

## Baseline (start)

| Metric | Value |
|--------|-------|
| `teamfight_simulation_core.cpp` | ~840 |
| `sim_unit_tick.cpp` | 581 |
| `sim_combat.cpp` | 531 |
| `sim_damage.cpp` | 566 |
| Bench (5v5, 2k, workers=1) | ~138.9 m/s |
| Fixtures | 7/7 |
| Friends | 1 |

## Phases

| Phase | Status |
|-------|--------|
| 8a Wiki + baseline | Done |
| 8b Unit tick split (`_internal`, `_cooldowns`, `_regen`, `_cast`, `_combat`, `_movement`) | Done |
| 8c Coordinator tick glue (`sim_coordinator_tick.cpp`) | Done |
| 8d Coordinator bindings (`sim_coordinator_bindings.cpp`) | Done |
| 8e Combat split (`_projectile`, `_actions`) | Done |
| 8f Profile `RuntimeFlags` | Done |
| 8L Bench guard + docs | Done |

## Exit (May 28, 2026)

| Metric | Target | Actual |
|--------|--------|--------|
| `teamfight_simulation_core.cpp` | < ~700 | **~703** |
| `sim_unit_tick` largest TU | < ~350 | **cooldowns 168** |
| `sim_combat` largest TU | < ~350 | **actions 188** |
| Friends | 1 | **1** |
| Fixtures | 7/7 | **7/7** |
| Bench | >= ~136 m/s | **~146.8 m/s** |

## Validation gate

Canonical sequence per `AGENTS.md`; run after each phase.
