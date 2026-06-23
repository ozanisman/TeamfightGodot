# Performance status

> **Agents:** validation commands and benchmark numbers only. Code layout → [native_agent_guide.md](native_agent_guide.md).

**Last updated:** 2026-06-22

## Allocation tracking infrastructure (Slice 1 - 2026-05-29)

**Status:** Infrastructure added, currently unused.

**Components:**
- `sim_allocation_tracker.hpp/cpp` - Type-based allocation tracking with atomic counters
- `SIM_TRACK_ALLOC` macro - No-op in Release mode, active in Debug builds
- `sim_profile_counters.hpp` - Allocation counter fields (currently all zeros)
- `sim_profile.hpp/cpp` - Integration functions for enabling/disabling tracking
- `stat_definitions.hpp` - `stat_index` helper for stat lookups

**Why unused:**
- Requires manual instrumentation at every allocation site
- Debug build measurement showed zero allocations (no instrumentation in hot paths)
- External audit already identified high-severity issues via profiling

**Future use:**
- Infrastructure remains available for targeted allocation measurement
- Can be enabled by adding `SIM_TRACK_ALLOC` calls to specific hot paths
- Useful for validating optimization impact on allocation patterns

## Targeting optimization (2026-05-29)

Dual-path distance pruning (20% threshold) was tested but broke fixture parity; reverted to maintain baseline.

## Targeting frame sync optimization (2026-05-29)

Attempted to eliminate bulk targeting frame sync pass by integrating sync into unit tick loop (end-of-tick sync per unit). Testing showed no meaningful performance improvement

## Benchmark (canonical gate)

Release build, **2000 matches**, **5v5**, **`--bench-skip-summaries`**, **workers=1**:

| Metric | Value |
|--------|-------|
| Matches/sec | **~103–110** |
| `duration_sec` | ~18–19 |
| Fixtures | **9/9** (unchanged gate) |

Recorded after native refactor + hygiene pass. Re-run the gate before merge or when claiming a perf change; numbers vary with background load.

> **Note:** An earlier 142.4 m/s result was an outlier. Consistent results are ~103–110 m/s. Always run the gate multiple times and flag discrepancies >10-15% before claiming an optimization.

**Regression compare (optional):** [`scripts/tools/run_perf_iteration_gate.ps1`](../../scripts/tools/run_perf_iteration_gate.ps1) vs [`scripts/tools/perf_gate_baseline.json`](../../scripts/tools/perf_gate_baseline.json) (2026-05-28 capture: w=1 median **146.2** m/s, w=8 median **628.7** m/s).

## Profiling snapshot (2026-05-28)

50 matches with `--sim-profile --targeting-profile`: `ns_update_units` dominates; within unit tick — `uu_targeting`, `uu_cooldowns_cc`, `uu_separation`; enemy scoring (`se_calls`); many ticks skip full retarget (`tgt_retarget_keeps` high).

Further ideas (not scheduled): [algorithmic_optimization_analysis.md](algorithmic_optimization_analysis.md).

## Validation gate

Run the canonical gate in [README.md#validation-gate](../../README.md#validation-gate). Confirm no lingering Godot processes after runs.
