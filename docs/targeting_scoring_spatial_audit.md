# Targeting, Scoring, and Spatial Drift Audit

Scope: 5v5 batch runtime in the native core. Analysis only. No gameplay or parity-affecting code changes.

## Current measurement context

The current local batch logs in this checkout are older artifacts, but they are still useful as a reference point for the current code shape.

- `logs/bench_0.txt:6-17`, `logs/bench_1.txt:6-17`, `logs/bench_2.txt:6-17`: 2000 matches, `team_size=5`, `workers=1`, `bench_skip_summaries=true`, about 190.9-193.4 matches/sec.
- `logs/bench_10k_profile_stdout.txt:6-17`: 10000 matches, same mode, about 204.9 matches/sec.
- `logs/bench_10k_profile_stderr.txt:1`: profile split shows `update_units` dominates and `uu_targeting` is the largest unit-level bucket.

## Ranked hotspots

| Rank | Severity | Location | Why it matters | Low-impact refactor candidate |
|---|---|---|---|---|
| 1 | High | `native/src/teamfight_simulation_core.cpp:2299-2490`, especially `2311-2489` | `_select_enemy_target()` does a full enemy scan per evaluation, and `_score_enemy_target()` layers multiple per-candidate checks. Profile evidence shows `uu_targeting` dominates `update_units` (`logs/bench_10k_profile_stderr.txt:1`). | Replace repeated `StringName`/map lookups with cached numeric or direct-indexed values where behavior stays identical. Hoist candidate-independent weights out of the inner loop. Avoid recomputing the same target metadata more than once per evaluation. |
| 2 | High | `native/src/teamfight_simulation_core.hpp:359-363`, `native/src/teamfight_simulation_core.cpp:1492-1494`, `2393-2395`, `3256-3258` | Broad-phase policy is benchmark-sensitive. The code uses `SPATIAL_BROAD_PHASE_TEAM_THRESHOLD = 4`, which means 5v5 can enter the spatial path. Any throughput comparison must use this executed path unless the threshold is intentionally changed and revalidated. | Do not change it yet. Keep benchmark docs and comments synced to the executed path before touching runtime behavior. |
| 3 | Medium | `native/src/teamfight_simulation_core.cpp:1117-1190`, `1880-1969` | `_prepare_tick_context()` and `_refresh_target_pressure()` are full-tick scans that feed the targeter. They are cheaper than targeting itself, but they are on the hot path every tick. | Collapse redundant passes only where it does not change parity. Keep the existing data contract, but consider incremental maintenance of target pressure and density caches if a safe seam exists. |
| 4 | Medium | `scripts/simulation/simulation_batch_worker.gd:31-49`, `scripts/simulation/match_replay_input.gd:80-99`, `scripts/simulation/champion_catalog.gd:977-981` | Batch input construction allocated per match, and `get_champion_ids()` rebuilt an ID array every call. This was not the main kernel cost, but it was repeated for every batch item. | Implemented in this pass: champion IDs are cached once and the worker reuses scratch player/enemy arrays. |
| 5 | Low-Medium | `native/src/teamfight_simulation_core.cpp:3537-3572`, `scripts/simulation/replay_io.gd:10-19`, `scripts/simulation/headless_runner.gd:476-506` | Full-summary mode still serializes every match to dictionaries and JSON. This is expected overhead, but it matters when the run is not using `--bench-skip-summaries`. | Keep this out of the primary 5v5 throughput gate. If full-summary throughput becomes a product target, evaluate streaming/chunked output separately. |

## Drift table

| Drift | Evidence | Why it matters |
|---|---|---|
| Broad-phase threshold mismatch | The code uses `4` in `native/src/teamfight_simulation_core.hpp:360-363`, while older notes had 6+ wording. | This can mislead anyone trying to optimize 5v5 if benchmark notes are read as a different execution path than the one actually running. |
| Benchmark baseline mismatch | `logs/benchmark_rundown.md` records **301** m/s (`workers=1`) and **338** m/s (`workers=8`) reference baselines (ten-run means; prior doc **260** / **290**); older raw batch logs may still read ~190–193 m/s on other gates (`logs/bench_0.txt:6-17`, etc.). | Do not mix batch sizes or worker counts when comparing; rerun the exact gate on the same machine and build. |
| Threshold comment vs current code path | `_use_spatial_broad_phase()` is called from target selection and movement helpers (`native/src/teamfight_simulation_core.cpp:2393-2395`, `2981-2993`, `3256-3258`). | The threshold choice is part of the benchmark contract, not just a doc note, so future profiling work needs the executed path stated plainly. |

## Decision log for severe blockers

These are the places where performance pressure is real, but the safe action in this pass is to record the issue instead of changing it.

1. Full target scanning in `_select_enemy_target()` and `_score_enemy_target()` is the dominant kernel cost. A more aggressive pruning or approximate target search could improve throughput, but it risks combat semantics and fixture parity. Record-only for now.
2. The spatial threshold choice is a behavior decision, not just an optimization. Because the code and docs disagree, changing it without a benchmark-backed call would make later comparisons noisy. Record-only for now.
3. Any broader reuse of per-match batch objects needs care around aliasing and stale state. The current batch path already relies on clearing runtime state after each match (`native/src/teamfight_simulation_core.cpp:3600-3604`), so reuse should be treated as a separate change with explicit verification.

## Suggested next implementation pass

- Treat the current broad-phase threshold as the active benchmark contract unless a later phase intentionally changes it.
- Focus on reducing per-candidate scoring overhead first if the path stays as-is.
- If the threshold changes in a later phase, re-run the same benchmark gate before making any other optimization decisions.
- Batch-worker reuse from row 4 is already implemented.
- Only after those decisions, consider summary-path cleanup and deeper scoring refactors.
