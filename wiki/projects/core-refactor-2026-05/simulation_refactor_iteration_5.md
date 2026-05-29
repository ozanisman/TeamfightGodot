# Simulation Refactor — Iteration 5

Continuation after [simulation_refactor_iteration_4.md](simulation_refactor_iteration_4.md).

## Baseline (start)

| Metric | Value |
|--------|-------|
| `teamfight_simulation_core.cpp` | ~1895 |
| Friends | 4 |
| `sim_effects_compile.cpp` | ~1428 (monolithic) |
| Bench (5v5, 2k, workers=1) | ~101.3 m/s |
| Fixtures | 7/7 |

## Exit (May 28, 2026)

| Metric | Target | Actual |
|--------|--------|--------|
| `teamfight_simulation_core.cpp` | < ~1500 | **~1120** |
| Friends | ≤ 3 | **1** (`CoordinatorHostAccess` only) |
| `sim_effects_compile` split | category TUs | **Done** (`_damage`, `_status`, `_aoe`, `_spawn`, `_internal`) |
| Shared `sn_*` | single table | **`sim_effect_kinds.inl.hpp`** |
| `sim_coordinator_host.cpp` | host bodies | **Done** |
| Fixtures | 7/7 | **7/7** |
| Bench | document | **~103.4 m/s** (optional 5L: no further change) |

## Phases

| Phase | Status |
|-------|--------|
| 5a Coordinator dead `sn_*` purge | Done |
| 5b `sim_effect_kinds.inl.hpp` | Done |
| 5c `sim_coordinator_host.cpp` + friends ≤3 | Done |
| 5d `sim_effects_compile` category split | Done |
| 5e `sim_profile.cpp` cold-path extract | Done |
| 5L Bench profile | Done (documented; no hot-path change) |

## Validation gate

Canonical sequence per `AGENTS.md`; all green on exit.
