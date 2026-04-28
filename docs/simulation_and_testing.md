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
| `--check-only` | [`scripts/tools/check_only.gd`](../scripts/tools/check_only.gd) | Fast compile/load check: preloads core simulation scripts and exits. |
| `--check-native-load` | [`scripts/tools/check_native_load.gd`](../scripts/tools/check_native_load.gd) | Loads `teamfight_simulation_core.gdextension` and instantiates `TeamfightSimulationCore`. |
| `--check-determinism` | [`scripts/tools/check_determinism.gd`](../scripts/tools/check_determinism.gd) | Runs a **subset** of golden inputs twice per case; fails if canonical payloads/signatures differ. |
| `--check-benchmark` | [`scripts/tools/check_benchmark.gd`](../scripts/tools/check_benchmark.gd) | Throughput benchmark (prints JSON to stdout). |
| `--check-balance-patches` | [`scripts/tools/check_balance_patches.gd`](../scripts/tools/check_balance_patches.gd) | Native balance-patch API and overlay behavior smoke suite. |
| `--check-stats-dashboard` | [`scripts/tools/check_stats_dashboard_load.gd`](../scripts/tools/check_stats_dashboard_load.gd) | Loads fixture CSVs and instantiates the stats dashboard scene. |
| `--check-stats-aggregator` | [`scripts/tools/check_stats_aggregator_roundtrip.gd`](../scripts/tools/check_stats_aggregator_roundtrip.gd) | Writes synthetic summaries through [`stats_csv_aggregator.gd`](../scripts/tools/stats_csv_aggregator.gd) and reloads with [`stats_dashboard_loader.gd`](../scripts/tools/stats_dashboard_loader.gd). |
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

Common flags (all after `--`):

| Flag | Meaning |
|------|--------|
| `--batch-count=N` | Number of matches (default in script is large; set explicitly for gates). |
| `--team-size=N` | Roster size per side (e.g. `5` for 5v5). |
| `--workers=N` / `--max-workers=N` | Thread count capping (default uses CPU count). |
| `--base-seed=N` | Seeds are `N + match_index` for the chunk. |
| `--bench-skip-summaries` | Skips building full replay summaries when possible; enables **native batch** path only when **`workers==1`** (safe on Windows). |
| `--sim-profile` | Enables native sim profiling; stderr includes JSON with per-tick and **`score_enemy_ns`** breakdown (see native `TeamfightSimulationCore`). |

Example **primary 5v5 gate** (median of 3 runs on the same machine):

```powershell
.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=1
```

Baseline numbers and methodology: [`logs/benchmark_rundown.md`](../logs/benchmark_rundown.md).

**Multi-process wall-clock** (aggregate m/s = total matches ÷ outer wall time, not average of shard JSON):

- [`scripts/tools/run_benchmark_sharded.ps1`](../scripts/tools/run_benchmark_sharded.ps1)
- [`scripts/tools/run_benchmark_shard_worker.ps1`](../scripts/tools/run_benchmark_shard_worker.ps1)

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

Example:

```powershell
.\run_godot.ps1 --generate-stats -- --matches-per-size=20 --team-sizes=1,2 --out-dir=res://stats_output
```

---

## Other checks

| Command | Use |
|---------|-----|
| `.\run_godot.ps1 -- --check-only` | After editing GDScript: catch parse/load errors quickly. |
| `.\run_godot.ps1 -- --check-native-load` | CI/smoke: extension loads and class registers. |
| `.\run_godot.ps1 -- --check-determinism` | Regression: same seed → same outcome on repeated backend instances (subset of goldens). |
| `.\run_godot.ps1 -- --check-balance-patches` | Balance patch overlays and `get_balance_patches` round-trip. |
| `.\run_godot.ps1 --check-stats-aggregator` | Aggregator CSV shape + loader round-trip (no heavy sim). |
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
4. **Threading:** native **batched** sim is only enabled in benchmarks when **`--bench-skip-summaries` and `--workers=1`**; multi-thread native batch is unsafe on Windows—use **process sharding** for parallel throughput.
5. **GDScript workflow:** run `--check-only` before long headless runs after script edits; avoid heavy work at parse time (see [`AGENTS.md`](../AGENTS.md)).
6. **Logs:** inspect `logs/godot.log` when a run fails or times out; `run_godot.ps1` tails the log in `--check-only` for parse errors.
