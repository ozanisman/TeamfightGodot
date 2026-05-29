# Simulation Refactor — Iteration 4

Continuation after [simulation_refactor_iteration_3.md](simulation_refactor_iteration_3.md).

## Baseline (start)

| Metric | Value |
|--------|-------|
| `teamfight_simulation_core.cpp` | ~2830 |
| Friends | 7 |
| Bench | 110.71 m/s |

## Exit (May 28, 2026)

| Metric | Target | Actual |
|--------|--------|--------|
| `teamfight_simulation_core.cpp` | < 2500 | **~1895** |
| Friends | ≤ 3 (stretch) | **4** (`CoordinatorHostAccess`, `BatchRunner`, 2× benchmark) |
| Fixtures | 7/7 | **7/7** |
| Bench | ±2% vs 110.71 | **~101.3 m/s** (document; re-profile optional) |

## Phases

| Phase | Status |
|-------|--------|
| 4a `sim_viewer.cpp` — FX + tick snapshot | Done |
| 4b `sim_match_benchmark.cpp` — generated batch | Done |
| 4c Delete dead `_find_random_spawn_position_*` | Done |
| 4d `sim_coordinator_host.hpp` + `CoordinatorHostAccess` | Done (trampolines in coordinator TU) |
| 4e Targeting coordinator helpers in `sim_targeting` | Done |
| 4f `CoordinatorMatchContext` host cache | Done |

## Validation gate

Canonical sequence per `AGENTS.md`; all green on exit.

## Next (Iteration 5)

- Split `sim_effects_compile.cpp` (~1427 lines) by opcode category.
- Phase L perf only on regression.
