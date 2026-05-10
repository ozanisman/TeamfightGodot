# Simulation and testing tools

This project runs the teamfight simulator **headless** through Godot 4.x. The **production** match loop lives in the **native** GDExtension ([`native/`](../native/README.md)); GDScript provides harnesses, batch workers, and parity helpers.

## Entry point: `run_godot.ps1`

From the repo root, use the wrapper so logs go to [`logs/godot.log`](../logs/) and headless runs get a timeout.

- **Godot binary:** the script expects `C:\Godot\godot.exe`. Change `$godotExe` in [`run_godot.ps1`](../run_godot.ps1) if your install differs.
- **Arguments after `--`** are forwarded to Godot user args (parsed by the GDScript entry scripts below).

Typical pattern:

```powershell
cd <repo-root>
.\run_godot.ps1 -- <flags...>
```

Special **first-flag** modes (select a different `--script` and shorter timeouts):

| Flag | Script | Purpose |
|------|--------|--------|
| `--check-only` | [`scripts/tools/check_gdscript_preload.gd`](../scripts/tools/check_gdscript_preload.gd) | Minimal preload/compile gate: resolves core simulation + tooling scripts (no scene instantiate). |
| `--check-native-load` | [`scripts/tools/check_native_load.gd`](../scripts/tools/check_native_load.gd) | Loads `teamfight_simulation_core.gdextension` and instantiates `TeamfightSimulationCore`. |
| `--check-match-telemetry` | [`scripts/tools/check_match_telemetry.gd`](../scripts/tools/check_match_telemetry.gd) | Runs one native match and asserts each `unit_stats` row includes versioned `telemetry` from the native summary. |
| `--check-determinism` | [`scripts/tools/check_determinism.gd`](../scripts/tools/check_determinism.gd) | Runs a **subset** of golden inputs twice per case; fails if canonical payloads/signatures differ. |
| `--check-benchmark` | [`scripts/tools/check_benchmark.gd`](../scripts/tools/check_benchmark.gd) | Throughput benchmark (prints JSON to stdout). |
| `--check-benchmark-sharded` | [`scripts/tools/run_benchmark_sharded.ps1`](../scripts/tools/run_benchmark_sharded.ps1) | Spawns multiple benchmark processes (PowerShell driver) and reports aggregate wall-clock matches/sec. |
| `--check-balance-patches` | [`scripts/tools/check_balance_patches.gd`](../scripts/tools/check_balance_patches.gd) | Native balance-patch API and overlay behavior smoke suite. |
| `--check-stats-dashboard` | [`scripts/tools/check_stats_dashboard_load.gd`](../scripts/tools/check_stats_dashboard_load.gd) | Loads fixture CSVs and instantiates the stats dashboard scene. |
| `--check-stats-aggregator` | [`scripts/tools/check_stats_aggregator_roundtrip.gd`](../scripts/tools/check_stats_aggregator_roundtrip.gd) | Writes synthetic summaries through [`stats_csv_aggregator.gd`](../scripts/tools/stats_csv_aggregator.gd) and reloads with [`stats_dashboard_loader.gd`](../scripts/tools/stats_dashboard_loader.gd). |
| `--check-stats-csv-determinism` | [`scripts/tools/check_stats_csv_determinism.gd`](../scripts/tools/check_stats_csv_determinism.gd) | Two identical small `--generate-stats` runs compare canonical CSV payloads (parity harness before aggregation optimizations). Timeout 240s default. |
| `--generate-stats` | [`scripts/tools/generate_simulation_stats.gd`](../scripts/tools/generate_simulation_stats.gd) | Headless native batch → CSV bundle under `res://stats_output` (or override with flags below). Default timeout 600s; override with `RUN_GODOT_GENERATE_STATS_TIMEOUT_SECONDS`. |

If **none** of the above are present, the default headless path is [`scripts/tools/headless_bootstrap.gd`](../scripts/tools/headless_bootstrap.gd) → [`scripts/simulation/headless_runner.gd`](../scripts/simulation/headless_runner.gd).

**Timeouts:** overridable via `RUN_GODOT_TIMEOUT_SECONDS` / `RUN_GODOT_CHECK_TIMEOUT_SECONDS` (see `run_godot.ps1`).

---

## Golden fixtures (full parity gate)

**Contract:** [`fixtures/goldens/match_fixtures.json`](../fixtures/goldens/match_fixtures.json) holds named scenarios with expected match signatures / payloads.

```powershell
.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json
```

- Success: `Fixture parity passed for N case(s).`
- Failures print a JSON blob with `expected_signature`, `actual_signature`, and a `payload_diff` summary.

**After any native or gameplay change** that can affect combat, targeting, or tick ordering, run this before merging.

**Debug one fixture** (enables trace/targeting flags for that case only):

```powershell
.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json --debug-fixture-name=<fixture_name>
```

---

## Throughput benchmark (`--check-benchmark`)

Implemented in [`scripts/tools/check_benchmark.gd`](../scripts/tools/check_benchmark.gd) using [`scripts/simulation/simulation_batch_worker.gd`](../scripts/simulation/simulation_batch_worker.gd).

**Important:** `matches_per_sec` is **total matches ÷ wall time** for the whole process. Larger `--batch-count` often looks slightly faster because fixed startup/teardown cost is amortized—**compare runs using the same flags and batch size**.
For accepted throughput claims, use direct PowerShell runs outside AI/IDE automation and compare median runs from the same machine; AI/IDE-triggered runs are useful smoke probes but not authoritative baselines.

Common flags (all after `--`):

| Flag | Meaning |
|------|--------|
| `--batch-count=N` | Number of matches (default in script is large; set explicitly for gates). |
| `--team-size=N` | Roster size per side (e.g. `5` for 5v5). |
| `--workers=N` / `--max-workers=N` | Thread count capping (default uses CPU count). |
| `--base-seed=N` | Seeds are `N + match_index` for the chunk. |
| `--bench-skip-summaries` | Skips building full replay summaries when possible; with a native backend, [`check_benchmark.gd`](../scripts/tools/check_benchmark.gd) sets **`allow_native_batch`** so workers can use **`run_generated_matches_simulation_only`** whenever this flag is on (not restricted to `workers==1`). |
| `--sim-profile` | Enables native sim profiling; **stderr** includes one JSON line **per match** with per-tick and score breakdowns. Prefer small `--batch-count` and `--workers=1` to limit output (see [Profiling](#profiling-benchmark-hot-path) below). |
| `--targeting-profile` | Adds detailed targeting counters to `--sim-profile` output (`tgt_*`). Has no effect without `--sim-profile`; keep it off for throughput medians. |

Example **primary 5v5 gate** (median of 3 runs on the same machine):

```powershell
.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=1
```

Baseline numbers and methodology: [`logs/benchmark_rundown.md`](../logs/benchmark_rundown.md).

### Profiling benchmark hot path

- **`duration_sec`** in the benchmark JSON is the right quantity for **parallel speedup** vs `matches_per_sec` alone (total matches ÷ wall time for the whole run).
- **`TEAMFIGHT_BENCH_PHASES=1`** (environment): native `run_generated_matches_simulation_only` prints **one** `bench_phases` JSON line **per worker chunk** to stderr — catalog ensure, chunk preamble, per-match setup vs **`_simulate`** totals and averages. Useful for `workers>1` (multiple lines; may interleave).
- **`TEAMFIGHT_SIM_PROFILE=1`** or **`--sim-profile`**: per-tick counters; **one stderr line per match** when enabled — use a **small** `--batch-count` and **`workers=1`**.
- **`TEAMFIGHT_TARGETING_PROFILE=1`** or **`--targeting-profile` with `--sim-profile`**: adds detailed targeting counters (`tgt_retarget_keeps`, `tgt_enemy_scans`, `tgt_candidates_scored`, `tgt_candidates_prefix_pruned`, `tgt_ally_scans`, `tgt_frame_syncs`). Use only for profiling probes, not median throughput gates.
- Example recorded numbers and OS sampling pointers: [`logs/benchmark_profiling_notes.md`](../logs/benchmark_profiling_notes.md).

**Multi-process wall-clock** (aggregate m/s = total matches ÷ outer wall time, not average of shard JSON):

- [`scripts/tools/run_benchmark_sharded.ps1`](../scripts/tools/run_benchmark_sharded.ps1)
- [`scripts/tools/run_benchmark_shard_worker.ps1`](../scripts/tools/run_benchmark_shard_worker.ps1)

`run_godot.ps1` also exposes this via `--check-benchmark-sharded` (same `--batch-count` / `--team-size` flags) plus:

- `--shards=N`: process count (defaults to 8 when not specified)
- `--workers-per-shard=N`: benchmark threads inside each shard’s Godot process (default 1)

### When to use sharded benchmarks

- **Same total work, more physical parallelism:** each shard is a **separate Godot + native** process. On Windows, heavy in-process multi-threading (`--workers=8` on one process) often hits **contention, thermal throttling, and memory bandwidth** limits; sharding can improve **outer wall-clock** even when per-process `matches_per_sec` is unchanged.
- **`--check-benchmark-sharded` defaults match the bench-skip throughput path:** [`run_benchmark_sharded.ps1`](../scripts/tools/run_benchmark_sharded.ps1) passes `-BenchSkipSummaries` into each shard (skip full summaries + native **`run_generated_matches_simulation_only`**). Do not rely on sharded runs for fixture parity—they only measure throughput.
- **How work is split:** shard `s` runs `batch_count_s = ceil(Total/N)` matches with **`--base-seed = s × chunk`** (see driver). That preserves the usual seed sequence **0 .. TotalBatchCount−1** across shards (`run_benchmark_shard_worker.ps1`).
- **`--workers-per-shard`:** each shard invokes `run_godot.ps1` with `--workers=$WorkersPerShard`. Use **`1`** for predictable scaling (one sim chunk per process); raising it runs multiple Godot threads **inside each shard**, which multiplies parallelism but can re-introduce contention.
- **What to read:** the driver prints **`aggregate_matches_per_sec`** and **`aggregate_duration_sec`** — those are authoritative for throughput. Lines `matches_per_sec` from individual shards reflect **single-process** duration; averaging them misrepresents the whole run (see [`logs/benchmark_rundown.md`](../logs/benchmark_rundown.md) harness section).
- **Logs:** per-shard stdout goes to **`logs/bench_shard_<s>.log`**; stderr to **`logs/bench_shard_<s>.log.err`**. Native `bench_phases` / profiling lines may appear in stderr files.
- **Timeouts:** `--check-benchmark-sharded` uses a **longer default timeout** on the outer `run_godot.ps1` waiter (multiple child processes); override with `RUN_GODOT_CHECK_TIMEOUT_SECONDS` if needed.

Example:

```powershell
.\run_godot.ps1 -- --check-benchmark-sharded --batch-count=2000 --team-size=5 --bench-skip-summaries --shards=8 --workers-per-shard=1
```

Direct PowerShell invocation (same driver `run_godot.ps1` uses):

```powershell
.\scripts\tools\run_benchmark_sharded.ps1 -TotalBatchCount 2000 -ShardCount 8 -TeamSize 5 -WorkersPerShard 1 -BenchSkipSummaries
```

---

## General headless match runs (default bootstrap)

With **no** `--check-*` flag, [`headless_runner.gd`](../scripts/simulation/headless_runner.gd) handles CLI:

| Flag | Purpose |
|------|--------|
| `--fixture-file=res://...` | Run golden comparison (see above). |
| `--match-file=...` | Load match input JSON. |
| `--out=...` | Write replay summary JSON. |
| `--batch-count=N` | Run N matches (threaded batch); prints JSON array to stdout unless `--out` set. |
| `--team-size=N` | Team size for batch workers. |
| `--seed=N` | Base seed for batch. |
| `--player=archetypes` / `--enemy=archetypes` | Comma-separated archetype names (with defaults otherwise). |
| `--tick-rate=` | Override tick rate for built inputs. |
| `--dump-schema-hash=` / `--dump-schema-json=` | Champion schema export. |
| `--dump-contract-hash=` / `--dump-contract-json=` | Contract schema export. |
| `--sign-fixture-in=` / `--sign-fixture-out=` | Fixture signing helper. |
| `--rewrite-fixture-summaries=` | Regenerate expected summaries in a fixture file (use with care). |

Single-match summary prints to stdout if `--out` is omitted.

---

## Stats CSV export (`--generate-stats`)

Runs [`generate_simulation_stats.gd`](../scripts/tools/generate_simulation_stats.gd) (native extension required). Forwards user args after `--` to Godot:

| Flag | Default | Meaning |
|------|---------|--------|
| `--out-dir=` | `res://stats_output` | Output folder for the four CSVs. |
| `--team-sizes=` | `1,2,3,4,5` | Comma-separated team sizes to simulate. |
| `--matches-per-size=` | `100` | Match count **per** listed team size. |
| `--base-seed=` | `0` | Base seed offset (per-size seeds derived internally). |
| `--export-workers=` | `0` | Parallel chunk threads (`0` = auto cap vs matches and cores). |
| `--profile-stats` | off | Print JSON timing attribution lines to stderr (see [`stats_simulation_csv_generator.gd`](../scripts/tools/stats_simulation_csv_generator.gd)). |
| `--write-match-log` | off | Write `match_log.csv`; omitted by default to avoid retaining one row per match. |
| `--no-worker-aggregate` | off | Disable worker-local stats aggregation and return per-match summaries to the main thread. |
| `--no-native-generated-stats` | off | Disable the native generated-stats fast path and use the GDScript match-input fallback. |

`generate_simulation_stats` sets process env **`TEAMFIGHT_STATS_EXPORT_MINIMAL=1`** so native `run_matches_stats` / batch stats summaries omit per-unit **`telemetry`** dictionaries (CSV aggregation does not use them). Other entry points leave summaries unchanged unless you set that env yourself.

Example:

```powershell
.\run_godot.ps1 --generate-stats -- --matches-per-size=20 "--team-sizes=1,2" --out-dir=res://stats_output
```
Quote comma-list arguments in PowerShell, e.g. `"--team-sizes=1,2,3,4,5"`, so the full list is forwarded as one Godot user argument.

### Generate-stats throughput gate

When iterating on stats-export performance, keep one **frozen** workload for apples-to-apples wall time:

```powershell
1..3 | ForEach-Object {
	(Measure-Command {
		.\run_godot.ps1 --generate-stats -- `
			--matches-per-size=50 --team-sizes=5 --export-workers=8 --base-seed=0 `
			--out-dir=user://bench_stats_throughput
	}).TotalSeconds
}
```

Take the **median** of the outer seconds (`Measure-Command`); optionally add `--profile-stats` for stderr JSON buckets (`catalog_build_ns`, `assembly_ns`, etc.). **Regression policy:** if the median worsens by **more than about 5%** versus the baseline from the immediately previous optimization step on the **same machine and flags**, stop further stats-throughput tweaks for that series, record both medians, and do **not** assume an automatic revert (decide separately). Change `matches-per-size` or `team-sizes` only when starting a **new** comparison series—never mid-series unless you deliberately widen the workload.

---

## Other checks

| Command | Use |
|---------|-----|
| `.\run_godot.ps1 -- --check-only` | After editing GDScript: catch parse/load errors quickly. |
| `.\run_godot.ps1 -- --check-native-load` | CI/smoke: extension loads and class registers. |
| `.\run_godot.ps1 -- --check-determinism` | Regression: same seed → same outcome on repeated backend instances (subset of goldens). |
| `.\run_godot.ps1 -- --check-balance-patches` | Balance patch overlays and `get_balance_patches` round-trip. |
| `.\run_godot.ps1 --check-stats-aggregator` | Aggregator CSV shape + loader round-trip (no heavy sim). |
| `.\run_godot.ps1 --check-stats-csv-determinism` | Two-pass stats CSV determinism harness (adjust args after `--`; defaults are small/fast). |
| `.\run_godot.ps1 --generate-stats` | Full native CSV generation (long-running; see table above). |

---

## Related code map

| Area | Location |
|------|----------|
| Native sim core | [`native/src/teamfight_simulation_core.*`](../native/src/) |
| GDScript backend adapter | [`scripts/simulation/native_simulation_backend.gd`](../scripts/simulation/native_simulation_backend.gd) |
| Parity hashing / payloads | [`scripts/simulation/parity_tools.gd`](../scripts/simulation/parity_tools.gd) |
| Match input / summary schema | [`match_replay_input.gd`](../scripts/simulation/match_replay_input.gd), [`match_replay_summary.gd`](../scripts/simulation/match_replay_summary.gd) |
| Gameplay-driven stepping | [`scripts/simulation/native_simulation_backend.gd`](../scripts/simulation/native_simulation_backend.gd) |

Design note on Python-shaped internals: [`docs/python_relics_vs_godot.md`](python_relics_vs_godot.md).

---

## Best practices

1. **Native changes:** build the extension **Release** before benchmarking (`cmake --build` in `native/build`).
2. **Correctness first:** `Fixture parity passed` for **all** cases in `match_fixtures.json`.
3. **Benchmark discipline:** same machine, same `--batch-count`, `--team-size`, `--workers`, and `--bench-skip-summaries` choice; prefer **median of 3** for 5v5 primary gate ([`benchmark_rundown.md`](../logs/benchmark_rundown.md)).
4. **Threading:** current benchmark code passes `allow_native_batch` whenever **`--bench-skip-summaries`** is enabled, so each worker may use native `run_generated_matches_simulation_only()` when available. Treat worker-count recommendations as measurement-dependent; use direct stress runs on the target machine before changing defaults, and use **process sharding** when process-level isolation is desired.
5. **GDScript workflow:** run `--check-only` before long headless runs after script edits; avoid heavy work at parse time (see [`AGENTS.md`](../AGENTS.md)).
6. **Logs:** inspect `logs/godot.log` when a run fails or times out; `run_godot.ps1` tails the log in `--check-only` for parse errors.
