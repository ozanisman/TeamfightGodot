# Simulation Refactor — Iteration 7

Continuation after [simulation_refactor_iteration_6.md](simulation_refactor_iteration_6.md).

## Baseline (start)

| Metric | Value |
|--------|-------|
| `teamfight_simulation_core.cpp` | ~1079 |
| `sim_targeting_select.cpp` | 561 |
| `sim_periodic.cpp` | 881 |
| `sim_match_benchmark.cpp` | 566 |
| `sim_coordinator_host.cpp` | ~317 |
| Bench (5v5, 2k, workers=1) | ~118.3 m/s |
| Fixtures | 7/7 |

## Exit (May 28, 2026)

| Metric | Target | Actual |
|--------|--------|--------|
| `teamfight_simulation_core.cpp` | < ~850 | **~839** |
| Hpp private methods | < 40 | **~38** (delegator purge) |
| Targeting select TUs | all < ~450 | **enemy 344**, ally 100, effect 152 |
| `sim_periodic` split | largest < ~400 | **dot_hot 406**, aoe 352, reflect 93** |
| `sim_match_benchmark.cpp` | < ~200 | **~180** |
| `sim_coordinator_host.cpp` | < ~250 | **~227** |
| Friends | 1 | **1** |
| Fixtures | 7/7 | **7/7** |
| Bench | >= ~116 m/s | **~138.9 m/s** |

## Phases

| Phase | Status |
|-------|--------|
| 7a Wiki + baseline | Done |
| 7b Targeting select split (`_enemy`, `_ally`, `_effect`) | Done |
| 7c Coordinator delegator purge | Done |
| 7d Match glue (`sim_coordinator_match.cpp`) | Done |
| 7e Periodic split (`_internal`, `_dot_hot`, `_aoe`, `_reflect`) | Done |
| 7f Benchmark stats (`sim_match_benchmark_stats.cpp`) | Done |
| 7g `GeneratedMatchHost` in `sim_match_benchmark_host.cpp` | Done |
| 7L Bench guard | Done |

## Validation gate

Canonical sequence per `AGENTS.md`; all green on exit.
