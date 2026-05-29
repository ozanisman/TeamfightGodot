# Simulation Refactor — Iteration 11

Continuation after [simulation_refactor_iteration_10.md](simulation_refactor_iteration_10.md).

**Merge `core-refactor` → `main`:** manual; run canonical gate on branch (see [performance_optimization_status.md](../../notes/performance_optimization_status.md)).

## Baseline (start)

| Metric | Value |
|--------|-------|
| `teamfight_simulation_core.cpp` | ~296 |
| `teamfight_simulation_core.hpp` | ~304; many `using` aliases |
| `sim_effects_compile.cpp` | ~464 |
| Bench (quiet host, 5v5, 2k, workers=1) | ~139.4 m/s |
| Fixtures | 7/7 |
| Friends | 1 |

## Exit (complete)

| Metric | Value | Target |
|--------|-------|--------|
| `sim_effects_compile.cpp` | **184** | < ~350 |
| `sim_effects_compile_opcodes.cpp` | **279** (new) | — |
| `teamfight_simulation_core.hpp` | **~270** | < ~220 (stretch; aliases removed) |
| Coordinator TUs | `sim::*` at call sites | no `TeamfightSimulationCore::Type` aliases |
| Friends | **1** | 1 |
| Fixtures | **7/7** | 7/7 |
| Bench (quiet host) | **~140.8 m/s** (`duration_sec` 14.21) | >= ~135 m/s |

## Phases

| Phase | Status |
|-------|--------|
| 11a Wiki + baseline | Done |
| 11b Compile opcode split | Done — `sim_effects_compile_opcodes.cpp` |
| 11c Coordinator header slim | Done — dropped public `using` aliases; `sim::*` in coordinator TUs |
| 11d Optional hygiene | **Skipped** — `sim_periodic_dot_hot.cpp` 385, `sim_catalog.cpp` 398; defer to Iter 12 |
| 11L Bench guard + docs | Done |

## Structural refactor

**Iters 3–11 complete** (native layout, coordinator splits, compile/exec/status/periodic modularization, encapsulation). Release merge to `main` is a separate step.

## Validation gate

All green (May 28, 2026): cmake Release, `--check-only`, `--check-native-load`, `--check-match-telemetry`, fixtures 7/7, benchmark workers=1.

**Pre-merge re-check:** **~142.4 m/s** (2000×5v5, workers=1, `duration_sec` 14.04); fixtures 7/7.

## Post–Iter 11 hygiene (native)

- Removed stub TUs and empty coordinator `.hpp` shells; deleted one-off refactor backups/scripts.
- `sim_coordinator_match.cpp`: `MatchLoopState` / `MatchLoopHost` locals for `simulate` / `step_tick`.
- IWYU/clangd: `export` on `sim_stats.inl.hpp`; `using namespace godot` in `sim_effect_kinds.inl.hpp`.

## Deferred (Iter 12+)

- `sim_catalog`, `sim_unit_builder` splits
- Projectile spatial index; targeting precompute (see `wiki/notes/algorithmic_optimization_analysis.md`)
