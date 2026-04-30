# Simulation batch benchmark rundown

Generated from [scripts/tools/check_benchmark.gd](../scripts/tools/check_benchmark.gd) via `run_godot.ps1 -- --check-benchmark`. Native DLL built **Release**. Older raw transcripts: [bench_2000_t1.txt](bench_2000_t1.txt), [bench_2000_t5.txt](bench_2000_t5.txt), [bench_10000_t1.txt](bench_10000_t1.txt).

**April 2026:** Native changes include catalog load **mutex**, **spatial broad-phase** for targeting/density/kite/obscurance only when either team has **≥ 6** alive (standard **5v5 uses brute** at full roster). Optional **`run_matches_simulation_only`** (bench: native batch when **`--workers=1`** + **`--bench-skip-summaries`**). Benchmark exits after **two idle frames** before `quit`.

## Measurement gates (performance work)

Use after **every** native change intended to move throughput:

1. **Build:** `cmake --build` the extension **Release** (e.g. `native/build`).
2. **Parity:** `.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json` — must report all cases passed.
3. **Primary (in-sim 5v5):** run **3×** and take the **median** `matches_per_sec`:  
   `.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=1`  
   **Baseline (Apr 2026, reference setup):** **299** `matches_per_sec` — ten-run mean **~299.4** on the measurement host; compare new work **on the same machine** vs this figure. (Prior documented primary reference: **260**; older medians: **290**, **270**.)  
   Optional **seed offset** (for process sharding): `--base-seed=N` — match seeds are `N + local_index` (see [Harness / multi-process](#harness--multi-process-sharding-future)).
4. **Guardrail (1v1):** optional `.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=1` (full summaries).
5. **Multi-worker wall-clock (5v5, same batch):** `.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=8` — **450** `matches_per_sec` reference (ten-run mean **~449.9**; prior doc figure **290**); use same machine for before/after.

If the median in (3) does not improve vs the prior baseline, **revert** or iterate before stacking more optimizations.

## Environment

| Field | Value |
|--------|--------|
| CPU | AMD Ryzen 7 7840HS w/ Radeon 780M Graphics |
| Logical processors | 16 |
| Godot | 4.6.2.stable (from run output) |
| Default benchmark workers | 16 (`min(batch_count, OS.get_processor_count())`), overridable with `--workers=` / `--max-workers=` |
| Match length | 60 s sim time |
| Tick rate | 0.2 s ([sim_constants.gd](../scripts/simulation/sim_constants.gd) `SIMULATION_TICK_RATE`) → **300 ticks/match** |

## Current primary benchmarks (5v5, Apr 2026)

**Reference `matches_per_sec` (rounded ten-run means, same gate):** **299** with **`--workers=1`** (mean ~**299.4**), **450** with **`--workers=8`** (mean ~**449.9**). Same command except workers; see table. Longer batches (e.g. `--batch-count=10000`) should be compared **on the same machine**; do not mix batch sizes when claiming regression/improvement vs these figures.

| Workers | Reference `matches_per_sec` | Command |
|---------|---------------------------:|---------|
| **1** | **299** | `.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=1` |
| **8** | **450** | `.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=8` |
| Build | | **Release** native extension |
| Method | | Median of **3** runs for step (3) in [Measurement gates](#measurement-gates-performance-work); compare repeated runs to **299** / **450** (or raw means) on the same machine |

Older tables in this file stay as **historical** context (different code revisions, OS load, and hardware).

## Results — ten-run benchmark (30 Apr 2026, current tree)

**Context:** Release native extension as of this date; includes targeting hot-path tweaks (see [`targeting_opt.md`](targeting_opt.md)), `TickContext` backliner alive counts / assassin tank context precompute, and related simulation core changes. **Ten consecutive runs** per worker count; command identical to the primary 5v5 gate.

**Command**

`.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=<1|8>`

### `matches_per_sec`

| Workers | mean | stdev | min | max | median |
|---------|-----:|------:|-----:|-----:|-------:|
| **1** | 299.43 | 9.12 | 283.61 | 311.00 | **300.56** |
| **8** | 449.92 | 2.39 | 446.59 | 454.29 | **450.05** |

Raw **workers=1** runs: 308.35, 308.68, 309.01, 311.00, 309.01, 283.61, 289.85, 292.19, 289.85, 292.78  
Raw **workers=8** runs: 450.05, 450.05, 452.16, 451.45, 450.74, 447.28, 448.65, 446.59, 447.96, 454.29

### `duration_sec` (whole-batch wall time)

| Workers | mean | stdev | min | max | median |
|---------|-----:|------:|-----:|-----:|-------:|
| **1** | 6.646 | 0.212 | 6.362 | 7.052 | 6.549 |
| **8** | 4.572 | 0.194 | 4.423 | 5.020 | 4.444 |

**Wall-clock speedup:** paired **T1/T8** duration ratio **~1.46** (mean of per-run ratios **1.456**, median **1.455**).

**Relation to [Current primary benchmarks](#current-primary-benchmarks-5v5-apr-2026):** The rounded references **299** / **450** (and means ~**299.4** / ~**449.9**) were written for the **Ryzen 7 7840HS** row in [Environment](#environment). These Apr 30 runs are **intended** to be on that same reference laptop (the Godot benchmark JSON does **not** include CPU model—verify with `SystemInformation` / Task Manager if you need proof). For regression work, compare **before/after** on one fixed host; treat this block as an **updated ten-run baseline** for the current tree where the host matches Environment.

## Results — targeting optimization sweep (Apr 2026)

**Change set:** Sub-profiling inside `_score_enemy_target` (`score_enemy_ns` in `--sim-profile` JSON), precomputed **frontline** / **carry** index lists in `TickContext` for obscurance and bodyguard, `_scratch_critical_allies` for ally targeting, mid-tick **`alive`** filtering on those lists for golden parity. Parity: **22/22** goldens.

### Primary gate (5v5, 2000 matches, workers 1, bench-skip)

| | `matches_per_sec` |
|--|------------------:|
| **reference baseline (current doc)** | **299** |
| median (historical targeting sweep) | **290** |

*(Current reference **299** / **450** (`workers=8`): see [Current primary benchmarks](#current-primary-benchmarks-5v5-apr-2026). Table **290** row kept for that sweep only.)*

## Results — continuation pass (Apr 2026, 2000 matches, Release)

**Change set:** Hot-path micro-optimizations in [`teamfight_simulation_core.cpp`](../native/src/teamfight_simulation_core.cpp): precomputed attacker–enemy distance passed into `_score_enemy_target` (removes duplicate `sqrt` in `_select_enemy_target`), bodyguard checks use **d²** before `sqrt`, cluster density brute path and `_spatial_count_neighbors_in_grid` use **d²** only. **`--base-seed=`** on [`check_benchmark.gd`](../scripts/tools/check_benchmark.gd); multi-process driver [`run_benchmark_sharded.ps1`](../scripts/tools/run_benchmark_sharded.ps1) + [`run_benchmark_shard_worker.ps1`](../scripts/tools/run_benchmark_shard_worker.ps1). **No** broad-phase / per-tick grid change (deferred). Parity: **22/22** goldens.

### Baseline before this change (same machine, 3 runs, primary command)

| | `matches_per_sec` |
|--|------------------:|
| run 1 | 190.2 |
| run 2 | 183.1 |
| run 3 | 188.7 |
| **median** | **188.7** |

### After change (3 runs)

| | `matches_per_sec` |
|--|------------------:|
| run 1 | 195.5 |
| run 2 | 193.4 |
| run 3 | 193.9 |
| **median** | **193.9** |

**1v1 guardrail** (single run after change): `team_size=1`, default **16** workers, full summaries — **~816** `matches_per_sec` (unchanged ballpark vs prior ~793–797).

## Results — roadmap pass (Apr 2026, 2000 matches, Release)

**Change set:** `SPATIAL_BROAD_PHASE_TEAM_THRESHOLD` **6** (standard 5v5 uses brute for targeting/density/kite/obscurance). Separation uses the same **≥ 6 alive on team** rule for grid-based ally pruning. Parity: **22/22** goldens.

### 1v1 (`team_size` 1), 16 workers (guardrail, single run)

| Mode | duration_sec | matches_per_sec |
|------|-------------:|----------------:|
| Full summaries | 2.521 | **793.3** |

### 5v5 (`team_size` 5), 10 units

| Mode | Workers | Metric | Value |
|------|---------|--------|------:|
| `--bench-skip-summaries` | **1** | **median** `matches_per_sec` (3 runs) | **120.0** |
| | | run spread | 118.6, 120.0, 123.2 |
| `--bench-skip-summaries` | 16 | `matches_per_sec` (1 run) | **78.3** |
| Full summaries | 16 | `matches_per_sec` (1 run) | **83.2** |

Absolute m/s varies with OS load; compare **before/after** on the same machine using the **median-of-3** gate in [Measurement gates](#measurement-gates-performance-work).

**Notes**

- **5v5 in-sim** (`--workers=1 --bench-skip-summaries`): use **median of 3** for decisions; native batch applies for the whole chunk on one worker.
- **16 workers:** wall-clock is **contention-limited** (bench-skip can be slightly slower than full summaries when summary cost is hidden by lock contention—compare per session).
- **Multi-thread native batch** remains unsafe on Windows; see [Harness / multi-process](#harness--multi-process-sharding-future).

## Results — April 2026 refresh (prior table, pre-threshold-6)

### 1v1 (`team_size` 1), 16 workers

| Mode | duration_sec | matches_per_sec | peak_memory_growth | static_memory_delta |
|------|-------------:|----------------:|-------------------:|--------------------:|
| Full summaries (default) | 2.539 | **787.6** | ~5.5 MB | ~11.3 MB |
| `--bench-skip-summaries` | 2.509 | **797.0** | 0 | ~1.0 MB |

### 5v5 (`team_size` 5), 10 units

| Mode | Workers | duration_sec | matches_per_sec | peak_memory_growth | static_memory_delta |
|------|---------|-------------:|----------------:|-------------------:|--------------------:|
| Full summaries | 16 | 22.651 | **88.3** | ~36.4 MB | ~42.3 MB |
| `--bench-skip-summaries` | 16 | 22.574 | **88.6** | 0 | ~1.2 MB |
| `--bench-skip-summaries` | **1** | 14.588 | **137.1** | ~12.5 MB | ~0.7 MB |

**Notes (historical)**

- **5v5 / 1v1 ratio** was ~**8.9×** with spatial at threshold **5** on 16 workers (full summaries).
- **Concurrent** `run_matches_simulation_only` from **multiple** worker threads **faults** on Windows.

## Historical baseline (pre–April 2026 tables)

### Run A — 2000 matches, team_size 1 (1v1)

| Metric | Value |
|--------|------:|
| duration_sec | 2.521 |
| **matches_per_sec** | **793.4** |
| Approx. match-ticks/sec (matches_per_sec × 300) | ~238,000 |
| peak_memory_growth | 5.87 MB |
| static_memory_delta | 11.3 MB |
| allocation_churn_estimate | ~7.3 MB |

### Run B — 2000 matches, team_size 5 (5v5, 10 units)

| Metric | Value |
|--------|------:|
| duration_sec | 21.022 |
| **matches_per_sec** | **95.1** |
| Approx. match-ticks/sec | ~28,500 |
| peak_memory_growth | 34.5 MB |
| static_memory_delta | 42.7 MB |
| allocation_churn_estimate | ~4.6 MB |

### Run C — 10000 matches, team_size 1 (1v1, longer run)

| Metric | Value |
|--------|------:|
| duration_sec | 11.206 |
| **matches_per_sec** | **892.4** |
| Approx. match-ticks/sec | ~268,000 |
| peak_memory_growth | 41.2 MB |
| static_memory_delta | 50.8 MB |

## Harness / multi-process sharding

To raise wall-clock without unsafe **multi-thread** native batch on Windows, shard across **processes**:

1. **CLI:** `check_benchmark.gd` accepts `--base-seed=N` (printed in JSON as `base_seed`). Worker seeds: `base_seed + match_index` for each chunk item.
2. **Driver:** `.\scripts\tools\run_benchmark_sharded.ps1` (`-TotalBatchCount`, `-ShardCount`, `-TeamSize`, `-BenchSkipSummaries`, `-WorkersPerShard`) spawns one `powershell.exe` per shard calling `run_benchmark_shard_worker.ps1`.
3. **Metric:** use **`aggregate_matches_per_sec`** from the driver output (**total** matches / **outer** wall-clock), not the average of per-shard JSON rates.

Godot may still exit non-zero on teardown warnings; the driver treats a shard as OK if stdout contains benchmark JSON (`matches_per_sec`).

## Analysis

1. **Throughput:** **1v1** remains **~790+** m/s on 16 workers (guardrail). **5v5** reference baselines with **`--bench-skip-summaries`**: **299** m/s at **`--workers=1`**, **450** m/s at **`--workers=8`** (ten-run means, [Current primary benchmarks](#current-primary-benchmarks-5v5-apr-2026)).
2. **5v5 scaling:** Broad-phase for targeting/density/kite/obscurance activates only at **≥ 6** alive on a team; standard 5v5 at full roster stays **brute** to avoid small-`n` grid overhead. **6v6+** or lopsided counts still use grids.
3. **Threading:** For **5v5**, prefer **`--workers=1`** for core sim throughput vs heavy worker counts when using bench-skip + native batch (less contention). **`--workers=8`** is the documented multi-worker reference (**450** m/s mean); tune for your machine.
4. **Memory:** Bench-skip removes most static delta / peak growth from summary retention; full-summary 5v5 still allocates heavily per match.
5. **Godot exit noise:** `ObjectDB instances leaked` / resources in use may still appear; teardown now waits **two** frames before `quit`. Further reduction may need explicit unrefs or longer drain.

## Next steps (remaining)

1. **Multi-thread + native batch:** If safe concurrent batched sim is required, profile whether the fault is GDExtension **Variant** marshalling, **Godot core**, or simulator globals; consider a **job queue** on one native runner thread instead of N concurrent cores.
2. **5v5 wall-clock:** Use [Harness / multi-process sharding](#harness--multi-process-sharding) for parallel processes; tune shard count vs startup overhead on large `--batch-count`.
3. **Re-run Run C (10k × 1v1)** when advertising long-run stability; numbers above are **2k** batches only.
4. **Keep Release builds** and avoid `debug_combat_trace` in batch workloads.
