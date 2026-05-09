# Targeting, Scoring, and Spatial Drift Audit

Scope: 5v5 batch runtime in the native core. **Decision log and measurement context** below are analysis-first; the **pre-flanking pruning** section documents a parity-safe targeting optimization that landed in the native core.

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

## Pre-flanking adjusted lower bound (`_select_enemy_target` / `_score_enemy_target_prefix`)

**Motivation:** Existing branch pruning compared `best_adjusted` to a lower bound computed **after** the full prefix score (including flanking and taunt bump). Some work can be skipped before flanking when even a **conservative** minimum prefix score cannot beat the current best adjusted score.

**Invariant (lower is better on all tuple fields):** Lexicographic ordering on `(adjusted_score, raw_score, bucket_rank, distance, instance_id)` is unchanged.

**Rule (after computing the non-flanking prefix base `score`, before flanking):**

1. Let `prefix_lb` be a lower bound on the full prefix return (base + flanking + taunt bump in the flanking/tie-in block). With `align * isolation` in `[0,1]`, the flanking subtract `align * isolation * flanking_weight` is bounded above by `flanking_weight`; `TAUNT_SCORE_BONUS` is applied at most once when the forced-target id matches. Hence:
   - `prefix_lb = score - flanking_weight * TARGETING_PREFIX_FLANKING_ALIGN_ISOLATION_PRODUCT_MAX` when `flanking_weight > 0` (constant is `1.0`; see `native/src/teamfight_simulation_core.hpp`).
   - If the forced-target taunt applies (`forced_target_id`, `forced_target_remaining`, `instance_id` match), add `TAUNT_SCORE_BONUS` (`-100`).
2. Conservative adjusted lower bound matching the **same** bodyguard slack already used post-prefix:  
   `candidate LB = prefix_lb + bucket_rank * bucket_margin - bodyguard_bonus_bound`.
3. If `candidate LB > best_adjusted`, return early from prefix processing (skip flanking + skip full scoring for that candidate).

**Why this is parity-safe vs the old post-prefix prune:** The new bound is **looser** than the post-prefix bound that uses the true prefix score (because `prefix_lb` is a lower bound on achievable prefix score). Any candidate skipped by the early gate would also be skipped once the true prefix lower bound from the old check was evaluated; order of candidates does not affect correctness of the skip condition.

**ABI:** `EnemyPrefixAdjustedEarlyPrune` in `teamfight_simulation_core.hpp` bundles `best_adjusted`, `bucket_rank`, `bodyguard_bonus_bound`, and an `early_skip_dest` out-pointer for the call from `_select_enemy_target`. Optional `adjusted_early_prune == nullptr` restores the previous “no early gate” path.

**Spatial iteration reorder (plan phase 3):** **Not implemented.** Sorting or spatially reordering the enemy candidate list can change which candidate wins under **floating‑point** pruning when `best_adjusted` is updated in a different order than the legacy `enemy_indices` walk, even without changing the deterministic tie tuple. Revisit only with a deterministic secondary key (for example stable index order after distance) or a proof that prune decisions are order‑independent under the chosen sort.

## Verification snapshot (local gate, Release native + `run_godot.ps1`)

Run the canonical sequence in `AGENTS.md` when changing this area. Example numbers from one local run after the change (expect machine variance):

- `cmake --build native/build --config Release`
- `.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=1` — order of **~200 matches/sec** observed on the same host as a **~211 matches/sec** timing on the immediate pre-change binary (variance + early-gate cost).
- Workers 8 and `--check-benchmark-sharded` for multi-shard aggregate throughput.
- Fixture gate: `.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json`. This checkout may still report a **pre-existing** mismatch on `effect_control_wall`; compare before/after fingerprints for any **new** regressions when goldens are authoritative on your branch.
- `TEAMFIGHT_BENCH_PHASES=1` adds a stderr JSON line with `avg_ns_per_match_simulate` alongside the summary object.
