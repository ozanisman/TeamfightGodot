# Batch Simulation and Stats Performance Audit

This audit maps the current batch simulation and statistics pipeline, records fresh IDE-run probes, and defines a prioritized optimization backlog for future implementation.

## Status

- **Native Release build:** passed.
- **GDScript preload:** passed via `--check-only`.
- **Native load:** passed via `--check-native-load`.
- **Match telemetry:** passed via `--check-match-telemetry`.
- **Fixture parity:** blocked by schema signature mismatch before fixture comparison.
- **Benchmark authority:** probe timings below were run through the IDE/tool path and should not be used as authoritative baseline numbers.

## Validation and Probe Results

| Area | Command | Result | Notes |
|---|---|---:|---|
| Build | `cmake --build native/build --config Release` | Pass | Produced `native/bin/teamfight_simulation_core.dll`. |
| Preload | `.\run_godot.ps1 -- --check-only` | Pass | Godot 4.6.2. |
| Native load | `.\run_godot.ps1 -- --check-native-load` | Pass | `TeamfightSimulationCore loaded`. |
| Telemetry | `.\run_godot.ps1 -- --check-match-telemetry` | Pass | `check_match_telemetry: OK (2 units)`. |
| Fixture parity | `.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json` | Fail | Expected schema `706b791c...`, got `9e86d136...`. |
| Bench skip, 5v5, 1 worker | `--check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=1` | Probe pass | `duration_sec=15.994278`, `matches_per_sec=125.044719`. Non-authoritative. |
| Bench skip, 5v5, 3 workers | `--check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=3` | Probe pass | `duration_sec=6.226244`, `matches_per_sec=321.220948`. Non-authoritative. |
| Full summary benchmark, 5v5, 1 worker | `--check-benchmark --batch-count=2000 --team-size=5 --workers=1` | Instrumentation suspect | Reported `batch_count=1` and `matches_per_sec=0.059205` because full-summary chunks return one chunk wrapper. |
| Stats profile, all team sizes, 1 worker | `--generate-stats -- --matches-per-size=200 "--team-sizes=1,2,3,4,5" --export-workers=1 --profile-stats` | Probe pass | `wall_ns=3894190000`, `match_count=1000`, dominant chunk phase `native_run_ns`. |
| Stats profile, all team sizes, 3 workers | `--generate-stats -- --matches-per-size=200 "--team-sizes=1,2,3,4,5" --export-workers=3 --profile-stats` | Probe pass | `wall_ns=1967998000`, `match_count=1000`, dominant top-level phase `chunk_total_ns`. |

## Current Pipeline Map

### Benchmark-only simulation

1. `scripts/tools/check_benchmark.gd` parses `--batch-count`, `--team-size`, `--workers`, `--base-seed`, `--bench-skip-summaries`, and `--sim-profile`.
2. It starts one `SimulationBatchWorker` per Godot `Thread`, passing `bench_skip_summaries` and `allow_native_batch` when summary output is skipped.
3. `scripts/simulation/simulation_batch_worker.gd` initializes thread-local catalog state, creates a `NativeSimulationBackend`, and selects one of three paths:
   - native generated simulation-only batch: `run_generated_matches_simulation_only(base_seed, chunk_len, team_size)`,
   - native simulation-only over GDScript-built inputs: `run_matches_simulation_only(inputs)`,
   - per-match `run_match_stats()` or `run_match()` with summary data returned to GDScript.
4. `scripts/simulation/native_simulation_backend.gd` loads/instantiates `TeamfightSimulationCore` and forwards calls across the GDExtension boundary.
5. `native/src/teamfight_simulation_core.cpp` owns catalog loading, runtime state reset, match setup, `_simulate()`, optional summary construction, progress counters, and profiling output.

### Stats generation

1. `scripts/tools/generate_simulation_stats.gd` parses output directory, team sizes, matches per size, base seed, export worker cap, and `--profile-stats`.
2. `StatsSimulationCsvGenerator.run()` creates one shared `StatsCsvAggregator`, runs each team size sequentially, and starts worker threads per size.
3. Workers return one chunk result containing `match_results`, `matchup_data`, and optional chunk profile data.
4. The main thread joins workers, loops every returned match result, sends each result to `StatsCsvAggregator.consume_summary()`, then writes summary, combat, role, combo, match log, and matchup CSV files.
5. The stats path always requires compact per-match summaries today; it cannot use the benchmark-only generated native batch path because aggregation needs per-unit stats and compositions.

## Bottleneck Attribution

| Rank | Area | Evidence | Interpretation |
|---:|---|---|---|
| 1 | Native match execution | Stats profiles show `native_run_ns` at about 93-95% of summed chunk phases. | Simulation remains the dominant compute phase once full stats are generated. |
| 2 | GDExtension summary marshalling and GDScript aggregation | Stats path creates one dictionary per match plus per-unit dictionaries, then re-reads them through `StatsCsvAggregator`. | The boundary/aggregation shape is allocation-heavy and likely important for large stats runs even if per-probe bookkeeping was smaller than native time. |
| 3 | Worker join accounting | `worker_join_ns` dominates top-level setup because it includes time spent waiting for worker completion. | Current `stats_profile_summary` top-level labels are misleading; join time should be reported as wait/wall overlap, not setup overhead. |
| 4 | Full-summary benchmark instrumentation | Non-bench-skip 2000-match benchmark reports `batch_count=1`. | `check_benchmark.gd` cannot currently compare full-summary throughput correctly when `run_chunk()` returns a chunk wrapper. |
| 5 | Documentation and safety drift | Current docs describe native batch availability inconsistently for multi-worker paths. | Measurement protocol should be corrected before tuning worker/thread defaults. |

## Correctness and Measurement Blockers

- **P0 fixture schema mismatch:** fixture parity currently fails before gameplay comparison. Do not accept native optimization changes until the schema fixture state is resolved or consciously re-signed.
- **P0 full-summary benchmark count bug:** `check_benchmark.gd` sums `chunk_results.size()`, but stats/full-summary worker output can be `[chunk_result]` where the actual match count is under `match_results`.
- **P0 stats profile labeling:** `worker_join_ns` is real elapsed wait time, but presenting it as setup makes top-level percentages double-count/obscure chunk wall time.
- **P0 command quoting trap:** PowerShell requires quoting comma-list arguments such as `"--team-sizes=1,2,3,4,5"`; otherwise only the first team size may be processed.
- **P0 authoritative baseline gap:** timings gathered through the IDE/tool path are useful smoke probes only. Direct user-run medians are still required for accepted performance claims.

## Prioritized Implementation Backlog

### P0: Fix measurement correctness

#### P0.1: Restore the parity gate

- **Goal:** Make fixture validation usable again before accepting any native or stats optimization.
- **Problem:** `match_fixtures.json` fails on schema signature mismatch before gameplay fixture comparison.
- **Files:** `fixtures/goldens/match_fixtures.json`, schema export/signing paths in `scripts/simulation/headless_runner.gd`, and current contract/schema sources.
- **Implementation steps:**
  1. Reproduce the failure with `.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json`.
  2. Dump the current schema/contract hash using the existing CLI export flags.
  3. Compare the current schema payload against the fixture-stored hash source.
  4. Decide whether the schema changed intentionally.
  5. If intentional, re-sign fixtures explicitly; if not, restore schema compatibility.
- **Stop condition:** Do not re-sign if the diff includes unexplained summary or telemetry shape changes.
- **Validation:** fixture parity passes for all cases after `--check-only` and `--check-native-load`.
- **Risk:** medium; careless re-signing can hide behavior or contract drift.

#### P0.2: Fix full-summary benchmark accounting

- **Status:** Implemented; `--check-only`, full-summary benchmark count, and bench-skip benchmark count validations passed.
- **Goal:** Make `--check-benchmark` report actual completed matches for both simulation-only and full-summary modes.
- **Problem:** non-bench-skip runs can return one chunk wrapper containing `match_results`, but `check_benchmark.gd` currently counts `chunk_results.size()`.
- **Files:** `scripts/tools/check_benchmark.gd`; only touch `scripts/simulation/simulation_batch_worker.gd` if a clearer returned shape is needed.
- **Implementation steps:**
  1. Add a local helper in `check_benchmark.gd` that counts:
     - one per `true` simulation-only result,
     - one per plain summary dictionary,
     - `match_results.size()` for chunk wrapper dictionaries.
  2. Replace `completed_matches += chunk_results.size()` with the helper.
  3. Preserve existing memory metrics and worker reporting.
  4. Keep the change reporting-only; do not alter simulation output shape unless required.
- **Stop condition:** If the helper cannot distinguish summary dictionaries from chunk wrappers safely, add an explicit `match_count` field to chunk results instead of guessing.
- **Validation:** `--check-only`; `--check-benchmark --batch-count=20 --team-size=5 --workers=1` reports `batch_count=20`; bench-skip run still reports requested count.
- **Risk:** low; benchmark JSON only.

#### P0.3: Make stats profiling labels non-misleading

- **Status:** Implemented; `--check-only` and small `--profile-stats` validation passed.
- **Goal:** Separate elapsed wait/wall timing from additive phase timing in `stats_profile_summary`.
- **Problem:** `worker_join_ns` is wait time while workers are doing chunk work, but the summary currently groups it into `setup_total_ns`, which makes percentages look additive when they overlap.
- **Files:** `scripts/tools/stats_simulation_csv_generator.gd`.
- **Implementation steps:**
  1. Rename summary concepts, not necessarily raw fields, so `worker_join_ns` is shown as worker wait/wall overlap.
  2. Report additive main-thread bookkeeping separately from worker chunk summed wall time.
  3. Keep raw profile fields backward-compatible unless there is a clear consumer update.
  4. Add a summary note/field that `chunk_total_ns` is summed across chunks and may exceed wall time when workers run in parallel.
- **Stop condition:** If existing dashboard/tooling parses these exact profile keys, preserve old keys and add clearer new keys instead.
- **Validation:** `--check-only`; `--generate-stats -- --matches-per-size=20 "--team-sizes=1,2" --export-workers=2 --profile-stats --out-dir=res://stats_output_profile_check` emits coherent labels.
- **Risk:** low; profiling output only.

#### P0.4: Document CLI quoting and current measurement rules

- **Status:** Implemented; docs now quote comma-list examples and distinguish direct-run baselines from AI/IDE probes.
- **Goal:** Prevent accidental one-size stats runs and non-authoritative timing claims.
- **Problem:** PowerShell splits comma-list args unless quoted, and IDE/tool-run timings are useful probes but not accepted baselines.
- **Files:** `docs/simulation_and_testing.md`, `docs/performance_audit_batch_stats.md`.
- **Implementation steps:**
  1. Add quoted examples for `--team-sizes=1,2,3,4,5`.
  2. Mark direct user-run medians as the only accepted throughput baseline source.
  3. Add a short note that AI/IDE probe results can verify instrumentation but should not be used for performance claims.
- **Stop condition:** Avoid updating historical benchmark numbers from tool-run probes.
- **Validation:** docs-only review plus one quoted small stats command.
- **Risk:** low.

#### P0.5: Resolve native batch/threading documentation drift

- **Status:** Implemented; docs now state the current code behavior and require direct stress runs before changing worker defaults.
- **Goal:** Make docs match the current code path before tuning worker defaults.
- **Problem:** current documentation has conflicting statements about native batch availability and multi-worker safety.
- **Files:** `docs/simulation_and_testing.md`, `docs/benchmark_rundown.md`, `scripts/tools/check_benchmark.gd`, `scripts/simulation/simulation_batch_worker.gd`.
- **Implementation steps:**
  1. Confirm current code behavior: `check_benchmark.gd` passes `allow_native_batch = bench_skip_summaries` for every worker.
  2. Confirm `SimulationBatchWorker` can call `run_generated_matches_simulation_only()` when `bench_skip_summaries` and `allow_native_batch` are true.
  3. Update docs to describe current behavior separately from any known platform safety caveat.
  4. If safety is uncertain, require direct stress testing before recommending default worker counts.
- **Stop condition:** Do not claim multi-worker native batch is safe or unsafe beyond what current tests demonstrate.
- **Validation:** docs align with current code and direct-run measurement instructions.
- **Risk:** low for docs; medium if it leads to changing worker defaults later.

### P1: Reduce stats pipeline allocations

- **Avoid retaining match logs by default for large generated stats runs**
  - **Status:** Implemented; generated stats skips `match_log.csv` by default and supports `--write-match-log`.
  - **Files:** `scripts/tools/stats_csv_aggregator.gd`, `scripts/tools/generate_simulation_stats.gd`, dashboard consumers if they require `match_log.csv`.
  - **Change:** add an option to skip or stream match log rows instead of storing `_match_logs` for the whole run.
  - **Impact:** reduces memory retention and CSV construction cost for large stats exports.
  - **Risk:** medium if dashboard assumes the file always exists.
  - **Validation:** stats dashboard load check and CSV roundtrip.

- **Aggregate chunk-local stats before main-thread merge**
  - **Status:** Implemented; generated stats uses worker-local partial aggregation by default and supports `--no-worker-aggregate`.
  - **Files:** `simulation_batch_worker.gd`, `stats_csv_aggregator.gd`, `stats_simulation_csv_generator.gd`.
  - **Change:** have workers aggregate hero/role/combo/matchup partials per chunk and return compact partial aggregates instead of every per-match dictionary.
  - **Impact:** reduces GDScript object traffic, main-thread per-match loops, and memory pressure.
  - **Risk:** medium; aggregation logic must remain deterministic and match CSV output.
  - **Validation:** `--check-stats-aggregator`, dashboard load, generated CSV diff on fixed seed/count.

### P1: Optimize native/stats boundary

- **Native generated stats API**
  - **Files:** `native/src/teamfight_simulation_core.*`, `native_simulation_backend.gd`, `simulation_batch_worker.gd`.
  - **Status:** Active target; replace the noise-level `run_matches_stats_partial` experiment with generated native stats.
  - **Change:** native method generates draft teams, builds units directly, simulates, and returns a compact stats partial plus matchup data.
  - **Impact:** removes GDScript match-input assembly and native dictionary parsing from generated stats.
  - **Risk:** medium; native draft generation must stay byte-equivalent with the GDScript fallback.
  - **Validation:** fixture parity, telemetry check, direct-vs-fallback stats CSV equality, direct-run benchmark medians.

- **Generated stats input path**
  - **Files:** `simulation_batch_worker.gd`, `teamfight_simulation_core.cpp`.
  - **Change:** for stats runs, reuse native deterministic draft generation where possible instead of building one GDScript `MatchReplayInput` per match.
  - **Impact:** reduces `assembly_ns` and marshalling overhead; probe shows this is smaller than native run but still non-zero.
  - **Risk:** medium; draft generation must match existing seed semantics.
  - **Validation:** fixed-seed CSV diff and fixture/determinism checks.

### P2: Native simulation hot path

- **Current profile note:** `--sim-profile` on 20 generated 5v5 matches shows native update remains dominant. Summed buckets: `uu_targeting` ~31.8 ms, `uu_cooldowns_cc` ~28.1 ms, `uu_combat` ~14.4 ms, `uu_movement` ~10.3 ms. A branch-based timer clamp experiment did not improve `uu_cooldowns_cc`, so it was reverted.
- **Targeting candidate reduction**
  - **Files:** `native/src/teamfight_simulation_core.cpp`.
  - **Change:** profile `_update_unit()`, target selection, `_score_enemy_target()`, and spatial candidate selection before changing behavior; consider pruning only when parity-safe.
  - **Impact:** potentially high because native run dominates chunk time.
  - **Risk:** high; target order and winner outcomes may change.
  - **Validation:** full fixture parity and direct-run medians.

- **Tick-context and targeting-frame synchronization review**
  - **Files:** `native/src/teamfight_simulation_core.cpp`, `teamfight_simulation_core.hpp`.
  - **Change:** verify `_sync_targeting_frame_unit()` frequency and `_prepare_tick_context()` rebuild work against profiler data; reduce redundant sync/rebuild only when observable.
  - **Impact:** medium if hot in samples.
  - **Risk:** high; stale targeting data can break combat decisions.
  - **Validation:** fixture parity plus `--sim-profile` comparison.

### P2: Scaling strategy

- **Document safe worker defaults from direct medians**
  - **Files:** `docs/simulation_and_testing.md`, `docs/benchmark_rundown.md`.
  - **Change:** resolve contradiction around native batch and multi-worker safety, document direct-run worker sweet spots, and explain process sharding vs threads.
  - **Impact:** prevents regressions from bad measurement choices.
  - **Risk:** low.
  - **Validation:** docs match current code path and direct-run measurements.

## Recommended Next Execution Order

1. Fix fixture schema gate or explicitly decide it is out of scope for performance-only work.
2. Fix benchmark full-summary counting and stats profile labeling.
3. Add a small generated-stats CSV diff fixture for fixed seed/count before optimizing aggregation.
4. Implement chunk-local or native-side aggregation experiments behind a flag.
5. Only then tune native targeting/simulation hot paths using direct-run medians and parity gates.

## Spike note (stats FFI vs native `_simulate`)

On representative `--generate-stats --profile-stats` runs, summed worker phases showed **`native_run_ns` ~93–95%** of chunk work and **`assembly_ns`** a distant second (`docs/performance_audit_batch_stats.md`, § Bottleneck Attribution). Subsequent “big-win” optimizations should prioritize **fewer/heavier FFI summaries** or **native-generated drafts** before micro-tuning `_simulate()`, unless profiling on a frozen recipe shows assembly or marshalling creeping up after batching fixes.

Native `_simulate()` remains the correctness-critical hot path (`TEAMFIGHT_SIM_PROFILE=1` / `--sim-profile`): target targeting/tick/context only behind full parity gates.

## 2026-05 Stats Generator Throughput Snapshot

Frozen workload: `--matches-per-size=200 "--team-sizes=1,2,3,4,5"`.

- Direct generator medians: `workers=1` 3.412s, `workers=3` 1.824s, `workers=8` 1.825s.
- Sharded process-per-size median with `ExportWorkers=1`: 2.960s; stats sharding was rejected.
- Native partial aggregate vs fallback at `workers=3`: 200-per-size was noise/slightly worse (1.824s vs 1.813s); 1000-per-size was noise/slightly better (7.983s vs 8.024s). This experiment was removed.
- Profile still attributes ~99% of summed chunk work to native simulation. The next high-impact pass should target generated-draft/native sim cost.

## Direct-Run Measurement Matrix Still Needed

Run outside the IDE/tool path before accepting any throughput claim:

```powershell
cmake --build native/build --config Release
.\run_godot.ps1 -- --check-only
.\run_godot.ps1 -- --check-native-load
.\run_godot.ps1 -- --check-match-telemetry
.\run_godot.ps1 --check-stats-csv-determinism -- --matches-per-size=12 "--team-sizes=3,5" --export-workers=2
.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json
.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=1
.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=3
.\run_godot.ps1 --generate-stats -- --matches-per-size=200 "--team-sizes=1,2,3,4,5" --export-workers=1 --profile-stats --out-dir=res://stats_output_audit_direct_w1
.\run_godot.ps1 --generate-stats -- --matches-per-size=200 "--team-sizes=1,2,3,4,5" --export-workers=3 --profile-stats --out-dir=res://stats_output_audit_direct_w3
```
