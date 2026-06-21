# Algorithmic Optimization Analysis

> **Status (reset):** The previous version of this note referenced line numbers and
> symbols (`_collect_effects`, a global distance cache, "obscurance aux grid",
> "bodyguard scoring", `carry_indices`) from a pre-refactor **monolithic** source file.
> None of those map to the current modular `native/src/simulation/` layout, so the
> concrete line/symbol references were removed. The two failed-experiment lessons below
> are preserved because they remain valid. **All ideas here are unverified against the
> current code and require fresh profiling before implementation.**

## Performance numbers — single source of truth

This note no longer duplicates benchmark figures (the duplication was the original drift
source). Canonical numbers live in:
- [performance_optimization_status.md](performance_optimization_status.md) — dated benchmark + validation gate
- [`scripts/tools/perf_gate_baseline.json`](../../scripts/tools/perf_gate_baseline.json) — regression baseline (median m/s by worker count)

## Profiling entry points (current)

Run with env-gated counters to locate hot spots before optimizing:
- `--check-benchmark ... --sim-profile --targeting-profile` (emits `sim::profile::Counters` JSON to stderr)
- Counter definitions: [`sim_profile_counters.hpp`](../../native/src/simulation/sim_profile_counters.hpp)

Latest profiling (see perf status doc) indicates `ns_update_units` dominates; within the
per-unit tick the largest buckets are `uu_targeting`, `uu_cooldowns_cc`, and `uu_separation`.

## Current spatial caching state

Contrary to the old "spatial grid caching FAILED" framing, a **per-tick spatial fill cache
now exists**:
- `fill_buckets_for_indices_cached()` and `invalidate_spatial_bucket_fill()` in
  [`sim_spatial.cpp`](../../native/src/simulation/sim_spatial.cpp)
- Invalidated once per tick in [`sim_match_loop.cpp`](../../native/src/simulation/sim_match_loop.cpp)
  (`step_tick`), and on significant unit movement via `on_unit_position_changed()`
- Used by movement/kite/separation ([`sim_movement.cpp`](../../native/src/simulation/sim_movement.cpp),
  [`sim_unit_tick_movement.cpp`](../../native/src/simulation/sim_unit_tick_movement.cpp)) and AoE
  ([`sim_aoe.hpp`](../../native/src/simulation/sim_aoe.hpp))
- Membership uses stamp generations (`stamp_circle` etc.) rather than per-operation rebuilds

So a future spatial-caching effort should extend/measure the **existing** cache, not assume
grids are rebuilt from scratch 3-5 times per tick.

## Failed experiments (preserved lessons)

### 1. Single shared spatial grid per tick — FAILED (parity)
- Idea: build one grid per team per tick, reuse for every spatial operation.
- Result: broke parity on the `effect_control_wall` fixture.
- Lesson: operations need **different filtered unit subsets** (specific teams/frontline), not a
  single shared grid. The current `fill_buckets_for_indices_cached` keys on the index set for
  this reason.

### 2. Skip targeting for CC'd (stunned/rooted) units — FAILED (parity)
- Idea: stunned/rooted units cannot act, so skip their targeting evaluation.
- Result: broke parity (`effect_control_wall`; winner changed).
- Lesson: targeting state still feeds **assist tracking, threat, and retarget pressure** even
  when a unit cannot act. Do not gate targeting on actionability.

## Candidate ideas remaining (UNVERIFIED — re-profile first)

Restated generically against current modules. Each needs fresh profiling to confirm the bottleneck
and a parity check before/after.

### A. Targeting score reuse across attackers
- Location: [`sim_targeting_score.cpp`](../../native/src/simulation/sim_targeting_score.cpp),
  [`sim_targeting_select_enemy.cpp`](../../native/src/simulation/sim_targeting_select_enemy.cpp)
- Observation: enemy scoring currently has per-attacker components (distance, threat response,
  role priority, peel). Some terms are attacker-independent (target HP ratio, target perceived
  threat) and could be computed once per tick per team.
- Risk: scoring is tightly coupled to per-attacker strategy weights; naive caching changes target
  selection and breaks parity. Treat any behavior change as a deliberate, fixture-updating change.

### B. Effect dispatch
- Location: `exec_route_for_opcode()` in
  [`sim_effects_exec.cpp`](../../native/src/simulation/sim_effects_exec.cpp)
- Note: dispatch is already a `switch` over opcode (not an if-else string chain as the old note
  claimed). Likely low ROI; verify with profiling before touching.

## Key insight

The hot path is already heavily optimized (hot/cold/rare split, reference-aggregate loop state,
stat caching, spatial fill cache, opcode-compiled effects). Remaining large wins probably require
either (a) accepted gameplay/parity changes with fixture regeneration, or (b) data-layout work in
the per-unit tick. Re-profile against the current modular code before committing to any item above.
