# Simulation Refactor — Iteration 10

Continuation after [simulation_refactor_iteration_9.md](simulation_refactor_iteration_9.md).

## Baseline (start)

| Metric | Value |
|--------|-------|
| `teamfight_simulation_core.cpp` | ~296 |
| `_units` / `_unit_cold` | public |
| Largest TUs | `sim_effects_exec_status` 453, `sim_status` 430, `sim_match_benchmark_stats` 420 |
| Bench (5v5, 2k, workers=1) | ~137.6 m/s |
| Fixtures | 7/7 |
| Friends | 1 |

## Phases

| Phase | Status |
|-------|--------|
| 10a Wiki + baseline | Done |
| 10b Encapsulate `_units` / `_unit_cold` | Done |
| 10c Split `sim_effects_exec_status` | Done |
| 10d Split `sim_status` | Done |
| 10e Split `sim_viewer` | Done |
| 10f Split `sim_match_benchmark_stats` | Done |
| 10g `MatchRuntimeState` hot-path wiring | Reverted (bench regression) |
| 10h Profile `unit_tick_profile` | Done |
| 10L Bench guard + docs | Done |
| 10M Branch closure | User action (open PR `core-refactor` → `main`) |

## Exit (May 28, 2026)

| Metric | Target | Actual |
|--------|--------|--------|
| `_units` / `_unit_cold` | private | **private** via `CoordinatorHostAccess` |
| Largest split TU | < ~350 | **172** (`sim_effects_exec_status_channel.cpp`) |
| Friends | 1 | **1** |
| Fixtures | 7/7 | **7/7** |
| Bench | >= ~135 m/s | **~139.4 m/s** (`duration_sec` 14.35; quiet host May 28). Confirmatory **~137.1 m/s**. Prior **~87 m/s** runs were host load (other apps competing for CPU). |
| `core._*` in `sim_match_benchmark.cpp` | 0 | **0** |

**10g:** `runtime_from()` wiring in `_sim_world()` / `_match_loop_state()` reverted after ~35% bench drop; direct field wiring restored.

**10M:** Merge `core-refactor` when ready; run full validation gate on CI.

## Bench phases (clean reprofile, `TEAMFIGHT_BENCH_PHASES=1`)

| Phase | avg / match |
|-------|-------------|
| setup | ~0.72 ms |
| simulate | ~6.39 ms |

## Validation (10L)

Release build + `--check-only`, `--check-native-load`, `--check-match-telemetry`, fixtures **7/7**, benchmark completed.

## Validation gate

Canonical sequence per `AGENTS.md`; benchmark on a quiet host (close heavy apps; kill stray Godot if any).
