# Simulation Refactor — Iteration 6

Continuation after [simulation_refactor_iteration_5.md](simulation_refactor_iteration_5.md).

## Baseline (start)

| Metric | Value |
|--------|-------|
| `teamfight_simulation_core.cpp` | ~1120 |
| Friends | 1 (`CoordinatorHostAccess`) |
| `sim_targeting.cpp` | ~1063 (monolithic) |
| Bench (5v5, 2k, workers=1) | ~103.4 m/s |
| Fixtures | 7/7 |

## Exit (May 28, 2026)

| Metric | Target | Actual |
|--------|--------|--------|
| `sim_targeting` split | 3–4 TUs; largest &lt; ~450 | **Done** (`_internal`, `_score`, `_select` 561, `_context`); stub **114** |
| `teamfight_simulation_core.cpp` | &lt; ~1050 | **~1079** |
| Friends | 1 | **1** |
| Fixtures | 7/7 | **7/7** |
| Bench | document; optional ≥106 m/s | **~118.3 m/s** (6L: cache match-context hosts on reset) |
| Compile guard | `try_fill` scope check | **`check_sim_effects_compile_structure.py`** in `--check-only` |
| Benchmark coupling | reduce `core._*` | **0** direct private field touches in `sim_match_benchmark.cpp` |
| Profile | `Counters` struct | **`sim_profile_counters.hpp`** |

## Phases

| Phase | Status |
|-------|--------|
| 6a Wiki + baseline | Done |
| 6b `sim_targeting` split | Done |
| 6c Coordinator dead JSON purge | Done |
| 6e Compile `try_fill` return-true guard | Done |
| 6d `GeneratedMatchHost` / `CoordinatorHostAccess` | Done |
| 6f `sim::profile::Counters` | Done |
| 6L Bench (evidence-only) | Done (`_match_context_hosts_cached`; ~+14% m/s vs Iter 5 exit) |

## Validation gate

Canonical sequence per `AGENTS.md`; all green on exit.
