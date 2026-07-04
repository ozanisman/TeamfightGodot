# TeamfightGodot

![Godot](https://img.shields.io/badge/Godot-4.6-478CBF?logo=godotengine&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.24%2B-064F8C?logo=cmake&logoColor=white)
![GDExtension](https://img.shields.io/badge/GDExtension-native-333333)
![Deterministic](https://img.shields.io/badge/simulation-deterministic-2ea043)
![Platform](https://img.shields.io/badge/platform-Windows-0078D6?logo=windows&logoColor=white)

Deterministic autobattler simulation platform on Godot 4 with a native C++ match engine, interactive inspection tools, and a native draft AI for pick/ban recommendations.

The GDExtension core runs combat, targeting, and batch matches at high throughput. GDScript owns validation, golden fixtures, and developer tooling. Godot scenes provide a main menu, simulation viewer, stats dashboard, and draft testing UI without coupling UI to the hot simulation path.

## Highlights

- **Deterministic by design** — seeded match loops, reproducible batch runs, and regression checks (`--check-determinism`, fixture parity).
- **Native performance** — C++17 simulation in `native/`; modular coordinator and effect pipeline tuned for large-scale batch execution (multi-thousand matches per gate).
- **Native draft AI** — C++ pick/ban recommender (`sim::draft_ai`) off the match hot path; interactive draft testing UI and headless validation gates.
- **Agent-oriented docs** — [`wiki/`](wiki/) captures module maps, invariants, and validation gates for ongoing work.

## Quick start

```powershell
git submodule update --init --recursive

cmake -B native/build -S native
cmake --build native/build --config Release

.\run_godot.ps1 -- --check-only
.\run_godot.ps1 -- --check-native-load
.\run_godot.ps1 -- --check-native-simulation-tests

# Interactive hub (main menu → viewer / stats / draft testing)
.\run_godot.ps1 --main-menu
```

After native or simulation changes, run the full [validation gate](#validation-gate) below.

Native build output lands in `native/bin/` and is loaded via [`teamfight_simulation_core.gdextension`](teamfight_simulation_core.gdextension). Edit champion data in [`scripts/simulation/champion_catalog.gd`](scripts/simulation/champion_catalog.gd) only; `champion_schema.json` regenerates on native build (see [`fixtures/goldens/DATA_SOURCE.md`](fixtures/goldens/DATA_SOURCE.md)).

## Architecture

| Layer | Role | Location |
|-------|------|----------|
| Runtime | Match loop, units, effects, damage, targeting | `native/src/` (`teamfight_simulation_core.*`, `simulation/`) |
| Bindings | Godot API, batch hosts, benchmarks | `native/src/`, `scripts/simulation/` |
| Validation | Fixture parity, determinism, telemetry | `fixtures/goldens/`, headless flags via `run_godot.ps1` |
| Draft AI | Pick/ban recommendations (off match hot path) | `native/src/simulation/sim_draft_ai_*`, `sim_draft_recommender.*`, `scripts/tools/draft_strategy_*.gd` |
| Tooling & UI | Viewer, stats, catalog authoring, draft testing | `scripts/`, `scenes/` |

```
  champion_catalog.gd ──► champion_schema.json ──► native loader
                                                          │
                                                   TeamfightSimulationCore
                                                    (match loop · combat)
                                                          │
              ┌───────────────────────────────────────────┼───────────────────────────┐
              ▼                                           ▼                           ▼
     fixtures/goldens/                            sim_draft_ai (pick/ban)      scenes/ · main menu
     parity · determinism                         off hot path                 viewer · stats · draft
              │                                           │
              └──────────────── run_godot.ps1 ──────────────┘
                        benchmarks · checks · validation
```

## Repository layout

| Path | Purpose |
|------|---------|
| `native/` | C++ GDExtension; build tree `native/build/`, artifact `native/bin/` |
| `third_party/godot-cpp/` | GDExtension bindings submodule |
| `scripts/simulation/` | Catalog, schema export, headless runner, batch workers |
| `scripts/app/` | Main menu, game root, stats dashboard app logic |
| `scripts/ui/` | Reusable UI components (`scripts/ui/components/`) |
| `scripts/tools/` | Headless checks, benchmarks, draft validation tools |
| `scenes/` | `game_root` (entry), `main_menu`, `simulation_viewer`, `stats_dashboard`, `draft_testing` |
| `scenes/components/` | Reusable scene components (`draft_champion_tile`, `draft_screen_shell`, etc.) |
| `assets/` | Shared UI theme |
| `fixtures/goldens/` | `match_fixtures.json`, `champion_schema.json`, `contract_schema.json`, `balance_patches.json`, `champion_kits.json`, `minion_schema.json` |
| `fixtures/stats_dashboard/` | Sample CSVs for `--check-stats-dashboard` smoke |
| `examples/` | Reference-only balance/kit example ([`balance_and_kit_example.gd`](examples/balance_and_kit_example.gd)) |
| `model_stats/` | Local/generated draft model CSVs (gitignored; produce via `--generate-stats` or draft tooling) |
| `stats_output/` | Local/generated stats CSV output (gitignored) |
| `logs/` | Runtime log from `run_godot.ps1` (`godot.log`; gitignored) |
| `wiki/` | Concepts, module maps, performance status, draft AI docs |
| `teamfight_simulation_core.gdextension` | Extension manifest (loads `native/bin/*.dll`) |

## Requirements

- Godot **4.6** (project targets GL Compatibility); required at **cmake configure** and runtime
- CMake **3.24+**, C++17 toolchain
- **git submodule** — `third_party/godot-cpp` must be initialized
- **Python 3** — `--check-only` runs `check_sim_effects_compile_structure.py` and `check_gdscript_preload.gd`
- PowerShell on **Windows** (`run_godot.ps1` is the supported launcher; other platforms are untested)

Set the Godot executable in `run_godot.ps1` (and `GODOT_EXE` / `GODOT` for cmake) if it is not at `C:\Godot\godot.exe`.

There is **no repo-root CI**; correctness is enforced by the Godot-hosted native module suite, local headless gates, and real-champion golden fixtures.

## Validation gate

Run after native or simulation changes (in order):

```powershell
cmake --build native/build --config Release
.\run_godot.ps1 -- --check-only
.\run_godot.ps1 -- --check-native-load
.\run_godot.ps1 -- --check-native-simulation-tests
.\run_godot.ps1 -- --check-match-telemetry
.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json
.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=1
```

Ensure no Godot processes remain when finished. Benchmark expectations and latest numbers: [`wiki/notes/performance_optimization_status.md`](wiki/notes/performance_optimization_status.md).

### Draft AI validation gate

Run after draft AI or stats changes to catch quantitative regressions (win rates, side bias):

```powershell
# 1. Generate full-draft validation data (slowest step; adjust N as needed)
godot --headless --path . --script res://scripts/tools/native_draft_validation_harness.gd \
  -- --trials=50 --sims-per-draft=25 \
     --blue-strategies=native_full,native_softmax,random,base_power_only \
     --red-strategies=native_full,native_softmax,random,base_power_only \
     --output=res://model_stats/native_draft_validation.csv \
     --draft-summary-output=res://model_stats/native_draft_validation_drafts.csv

# 2. Analyze with Wilson CIs / A/B report
godot --headless --path . --script res://scripts/tools/native_draft_validation_analyzer.gd \
  -- --input=res://model_stats/native_draft_validation.csv \
     --draft-summary=res://model_stats/native_draft_validation_drafts.csv \
     --output=res://model_stats/native_draft_validation_summary.csv \
     --ab-output=res://model_stats/native_draft_validation_ab_report.csv \
     --native-strategy-names=native_full,native_softmax

# 2b. Elo ladder + ordering gate
godot --headless --path . --script res://scripts/tools/native_draft_elo_ladder.gd \
  -- --draft-summary=res://model_stats/native_draft_validation_drafts.csv \
     --strategies=native_full,native_softmax,random,base_power_only

godot --headless --path . --script res://scripts/tools/native_draft_elo_gate.gd \
  -- --input=res://model_stats/native_draft_elo_ladder.csv \
     --draft-summary=res://model_stats/native_draft_validation_drafts.csv

# Run steps 2b ladder then gate sequentially (gate reads the CSV ladder writes).
# Do not invoke both in parallel. --draft-summary rejects stale ladder CSV.

# 3. Check quantitative thresholds and emit PASS/FAIL report
godot --headless --path . --script res://scripts/tools/native_draft_quantitative_gate.gd \
  -- --summary=res://model_stats/native_draft_validation_summary.csv \
     --ab-report=res://model_stats/native_draft_validation_ab_report.csv \
     --output=res://logs/native_draft_quantitative_gate_report.md

# 3b. Difficulty tier calibration + monotonic separation gate
godot --headless --path . --script res://scripts/tools/native_draft_validation_harness.gd \
  -- --trials=50 --sims-per-draft=25 \
     --blue-strategies=native_softmax_easy,native_softmax,native_softmax_hard \
     --red-strategies=random \
     --output=res://model_stats/native_draft_tier_calibration.csv \
     --draft-summary-output=res://model_stats/native_draft_tier_calibration_drafts.csv

godot --headless --path . --script res://scripts/tools/native_draft_validation_analyzer.gd \
  -- --input=res://model_stats/native_draft_tier_calibration.csv \
     --draft-summary=res://model_stats/native_draft_tier_calibration_drafts.csv \
     --output=res://model_stats/native_draft_tier_calibration_summary.csv \
     --ab-output=res://model_stats/native_draft_tier_calibration_ab_report.csv \
     --native-strategy-names=native_softmax,native_softmax_easy,native_softmax_hard

godot --headless --path . --script res://scripts/tools/native_draft_tier_gate.gd \
  -- --summary=res://model_stats/native_draft_tier_calibration_summary.csv \
     --draft-summary=res://model_stats/native_draft_tier_calibration_drafts.csv

# 4. Aggregate into the suite report (reads all PASS/FAIL files)
godot --headless --path . --script res://scripts/tools/run_draft_ai_validation_suite.gd
```

Thresholds default to the refreshed baselines in [`wiki/notes/draft_ai_improvement_plan.md`](wiki/notes/draft_ai_improvement_plan.md). Override any threshold with `--<metric>=<value>` (e.g., `--native_softmax_self_play_bias_max_pp=15.0`).

Stats snapshots now include a `stats_manifest.json` with `schema_version`, `snapshot_id`, `generated_at`, `catalog_version`, `generator_name`, `generator_args`, and file hashes. File hashes are `String.hash()` 32-bit values and the native loader validates every listed CSV file hash against the file on disk. The native `DraftStatsDatabase` warns when the manifest is missing, has an unsupported schema, contains invalid types, or has a hash mismatch. `generator_args` records the CLI arguments passed to the generator, so avoid passing secrets in stats-generation commands. To fail loudly on a mismatch, add a `certification` section to `model_stats/draft_ai_config.json`:

```json
{
  "certification": {
    "stats_snapshot_id": "<snapshot_id_from_manifest>"
  }
}
```

When `certification.stats_snapshot_id` is set, every native pick/ban recommendation call compares the loaded stats snapshot against the configured ID and returns an empty result with an error if they differ. The validation harness and quantitative gate inherit this check because they use the same native recommendation API.

## Common commands

All runs should go through `run_godot.ps1` (logging to `logs/godot.log`, timeouts). After GDScript edits, run `--check-only` before long smoke or benchmark scenes. Full flag list: [`wiki/notes/command_reference.md`](wiki/notes/command_reference.md).

### Interactive UI

`--main-menu` and `--simulation-viewer` disable headless mode in `run_godot.ps1`.

| Command | Purpose |
|---------|---------|
| `.\run_godot.ps1 --main-menu` | Main menu hub (viewer / stats / draft via in-app buttons) |
| `.\run_godot.ps1 --simulation-viewer` | Simulation viewer directly |
| `.\run_godot.ps1 --simulation-viewer --stats-dashboard` | Stats dashboard directly |

### Headless checks

| Command | Purpose |
|---------|---------|
| `.\run_godot.ps1 -- --check-main-menu` | Main menu scene load smoke |
| `.\run_godot.ps1 -- --check-stats-dashboard` | Stats dashboard loader smoke |
| `.\run_godot.ps1 -- --check-draft-ui` | Draft testing UI load smoke |
| `.\run_godot.ps1 -- --check-determinism` | Replay determinism probe |
| `.\run_godot.ps1 -- --check-balance-patches` | Balance patch and kit resolution |
| `.\run_godot.ps1 -- --check-stats-aggregator` | Stats CSV aggregator roundtrip |
| `.\run_godot.ps1 -- --check-projectile-payloads` | Projectile payload regression |
| `.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json` | Real-champion golden parity (15 fixtures) |
| `.\run_godot.ps1 -- --check-benchmark ...` | Throughput benchmark |

## Draft AI

Native C++ pick/ban recommender (`sim::draft_ai`) runs off the match tick hot path. `sim_draft_recommender.*` handles winner prediction and analysis. GDScript wrappers live in `scripts/tools/draft_strategy_*.gd`.

Default stats directory: `res://model_stats/stats_output_100k/` (requires `combat_stats.csv`, `matchup_with.csv`, `matchup_vs.csv`). Produce CSVs via `--generate-stats`. Headless validation: `--check-draft-ui`, `--validate-native-strategy`, `--validate-full-draft` (see [command reference](wiki/notes/command_reference.md)). Interactive testing: `.\run_godot.ps1 --main-menu` → Draft Testing.

Deep dive: [`wiki/notes/native_draft_ai.md`](wiki/notes/native_draft_ai.md)

## Troubleshooting

Headless or check failures: inspect `logs/godot.log` (created by `run_godot.ps1`).

- GDExtension load error 1114: [`wiki/notes/gdextension_error_1114_thread_local.md`](wiki/notes/gdextension_error_1114_thread_local.md)
- Headless file I/O on Windows: [`wiki/notes/godot_windows_headless_file_issue.md`](wiki/notes/godot_windows_headless_file_issue.md)

## Documentation

- [`wiki/README.md`](wiki/README.md) — knowledge base index
- [`wiki/notes/command_reference.md`](wiki/notes/command_reference.md) — headless flags and launcher routing
- [`wiki/notes/native_agent_guide.md`](wiki/notes/native_agent_guide.md) — native edit routing, invariants
- [`wiki/notes/simulation_module_map.md`](wiki/notes/simulation_module_map.md) — module ownership
- [`wiki/notes/native_draft_ai.md`](wiki/notes/native_draft_ai.md) — draft subsystem
- [`fixtures/goldens/DATA_SOURCE.md`](fixtures/goldens/DATA_SOURCE.md) — champion data workflow
- [`examples/balance_and_kit_example.gd`](examples/balance_and_kit_example.gd) — balance patches reference
- [`AGENTS.md`](AGENTS.md) — contributor and agent rules
