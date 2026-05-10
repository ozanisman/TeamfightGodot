# Benchmark hot-path profiling (automated run notes)

Generated while executing the profiling plan: wall-clock comparison, `TEAMFIGHT_BENCH_PHASES`, `--sim-profile` sample, and OS sampling pointers.

## Wall-clock speedup (`workers=1` vs `workers=8`)

Same flags: `--check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries`.

| Config | `duration_sec` | `matches_per_sec` | Notes |
|--------|---------------:|------------------:|-------|
| `workers=1` | **8.563** | 233.6 | Single native batch path |
| `workers=8` | **5.978** | 334.5 | Eight chunks in parallel |

**Speedup** \(T_1 / T_8\) ≈ **1.43** (ideal would be up to ~8 only with eight independent full cores and no shared bottlenecks). Use `duration_sec`, not `matches_per_sec` alone, when reasoning about parallelism.

## Native batch phases (`TEAMFIGHT_BENCH_PHASES=1`)

`run_generated_matches_simulation_only` prints **one** JSON line per **chunk** to **stderr** when the environment variable is set (non-empty, not `0`).

Example (`workers=1`, `batch_count=500`, this host):

```json
{"bench_phases":{"batch_count":500,"ns_catalog_ensure":5373900,"ns_chunk_preamble":5800,"ns_match_setup_total":207483200,"ns_simulate_total":1495138900,"avg_ns_per_match_setup":414966,"avg_ns_per_match_simulate":2990278}}
```

Interpretation (single-threaded chunk):

- **`ns_catalog_ensure`**: first `_ensure_catalog_loaded()` (includes any serialized file I/O).
- **`ns_chunk_preamble`**: build archetype list after catalog.
- **`avg_ns_per_match_setup`** (~0.41 ms here): draft + `_build_role_strategy_cache` + `_prepare_tick_context` + `_refresh_target_pressure` + initial `_reset_runtime_state`, per match.
- **`avg_ns_per_match_simulate`** (~2.99 ms here): `_simulate()` only.

**Fraction in `_simulate` (approx.):** `avg_sim / (avg_setup + avg_sim)` ≈ **88%** on this sample → further wins are mostly **native tick loop / targeting**, not GD chunk orchestration.

With **`workers=8`**, each worker emits its own `bench_phases` line; **per-match simulate averages were much higher** (~20 ms) on the same machine in a `batch_count=200` trial—consistent with **contention, thermal throttling, or memory bandwidth** when all cores run hot. Treat multi-worker phase numbers as **per-thread wall time**, not comparable to single-thread averages without OS-level confirmation.

## `--sim-profile` (tick-level, stderr)

**Caution:** one JSON line **per match**; use small `--batch-count` (e.g. 20) and **`workers=1`** to avoid stderr floods and interleaving.

Qualitative pattern from 20×5v5 sample: **`update_units`** dominates **`sum_ns_per_tick_avg`**; inside units, **`uu_targeting`** is typically the largest slice; **`score_enemy_ns`** averages ~100ns per call with sub-buckets for base/bodyguard/obscurance/flanking.

Enabling:

- CLI: `--sim-profile` (calls native `sim_profile_set_enabled`), and/or
- Env: `TEAMFIGHT_SIM_PROFILE=1` (see native `TeamfightSimulationCore`).
- Targeting counters: add `--targeting-profile` with `--sim-profile`, or set `TEAMFIGHT_TARGETING_PROFILE=1`. This adds `tgt_*` fields for retarget keeps, full scans, candidates scored/pruned, ally scans, and targeting-frame syncs. Leave off for median throughput runs.

## OS sampling (manual)

The plan calls for comparing **native DLL vs Godot** stacks and lock/file hot spots under `workers=1` vs `workers=8`. No automated capture in CI; on Windows use for example:

1. **Windows Performance Recorder** (WPRUI or `wpr -start GeneralProfile` …) while running the same `run_godot.ps1 -- --check-benchmark ...` twice with `--workers=1` and `--workers=8`.
2. **Visual Studio** sampling on the Godot process.
3. **Very Sleepy** or similar: attach to `godot*.exe`, run short batches, compare **self time** in `teamfight_simulation_core.dll` vs executable / GDScript VM.

Look for: mutex in `_ensure_catalog_loaded`, `FileAccess`, allocator contention, or engine locks when scaling threads.

## One-line conclusion

**Single-thread:** most chunk time is in **`_simulate`** (~88% in sample); tick profiler points at **unit update / targeting**. **Multi-worker:** wall-clock speedup is modest (~1.4× for 8 workers here); phase timings suggest **per-core sim slows under parallel load** — use OS sampling to separate **contention vs power limits**.
