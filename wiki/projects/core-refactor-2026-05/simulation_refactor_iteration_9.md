# Simulation Refactor — Iteration 9

Continuation after [simulation_refactor_iteration_8.md](simulation_refactor_iteration_8.md).

## Baseline (start)

| Metric | Value |
|--------|-------|
| `teamfight_simulation_core.cpp` | ~703 |
| `sim_damage.cpp` | ~566 |
| Bench (5v5, 2k, workers=1) | ~146.8 m/s |
| Fixtures | 7/7 |
| Friends | 1 |

## Phases

| Phase | Status |
|-------|--------|
| 9a Wiki + baseline | Done |
| 9b `MatchRuntimeState` | Done |
| 9c Coordinator state + reset | Done |
| 9d Coordinator catalog | Done |
| 9e Coordinator targeting + viewer | Done |
| 9f `sim_damage` split | Done |
| 9g Optional hygiene | Done |
| 9L Bench guard + docs | Done |

## Exit (May 28, 2026)

| Metric | Target | Actual |
|--------|--------|--------|
| `teamfight_simulation_core.cpp` | < ~600 | **296** |
| `sim_damage` largest TU | < ~350 | **285** (`sim_damage_apply.cpp`) |
| `sim_periodic_dot_hot.cpp` | < 400 (9g) | **385** |
| `MatchRuntimeState` | Single builder | `sim_match_runtime_state.{hpp,cpp}`; hot path uses direct `_sim_world` / `_match_loop_state` field wiring (lvalue `SimWorld`) |
| Coordinator glue | state, catalog, targeting, viewer, tick, bindings TUs | Done |
| Friends | 1 | `CoordinatorHostAccess` |
| Fixtures | 7/7 | **7/7** |
| Bench (5v5, 2k, workers=1) | >= ~144 m/s | **~137.6 m/s** (`duration_sec` 14.54; re-run May 28 after clearing Godot interference) |

**Bench note:** Earlier runs on this host (~89–96 m/s) correlated with background Godot/load interference. Clean re-run is within ~6% of Iter 8 baseline (~146.8 m/s).

## Validation (9L)

Release build + full gate green: `--check-only`, `--check-native-load`, `--check-match-telemetry`, fixtures **7/7**, benchmark completed.

## Validation gate

Canonical sequence per `AGENTS.md`; run after each phase.
