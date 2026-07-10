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
- **Native draft AI** — C++ pick/ban recommender (`sim::draft_ai`) with configurable difficulty tiers, self-play stats snapshots, and validation gates; interactive draft testing UI.
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
| Draft AI | Pick/ban recommendations and validation gates (off match hot path) | `native/src/simulation/sim_draft_ai_*`, `sim_draft_recommender.*`, `scripts/tools/draft_*.gd`, `scripts/tools/native_draft_*.gd` |
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

Set the Godot executable in `run_godot.ps1` (and the `GODOT_EXE` CMake cache variable or `GODOT` environment variable for CMake) if it is not at `C:\Godot\godot.exe`.

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

### Draft AI validation

After draft AI or stats changes, run the quantitative harness → analyzer → gates pipeline. Optional self-play stats generation writes a new snapshot under `model_stats/` (promotion to production is manual).

**Full commands, thresholds, certification, and smoke recipes:** [`wiki/notes/draft_ai_validation_gate.md`](wiki/notes/draft_ai_validation_gate.md)

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
| `.\run_godot.ps1 -- --check-large-projectile-damage` | Large projectile damage regression |
| `.\run_godot.ps1 -- --check-stats-csv-determinism` | Stats CSV determinism |
| `.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json` | Real-champion golden parity (15 fixtures) |
| `.\run_godot.ps1 -- --check-benchmark ...` | Throughput benchmark |

## Draft AI

Native C++ pick/ban recommender with configurable difficulty tiers (easy/normal/hard) and self-play stats snapshots. Default stats: `res://model_stats/stats_output_100k/` (generate via `--generate-stats`). Interactive testing: `.\run_godot.ps1 --main-menu` → Draft Testing.

- [`wiki/notes/native_draft_ai.md`](wiki/notes/native_draft_ai.md) — architecture and scoring
- [`wiki/notes/draft_ai_validation_gate.md`](wiki/notes/draft_ai_validation_gate.md) — quantitative validation pipeline
- [`wiki/notes/command_reference.md`](wiki/notes/command_reference.md) — headless flags

## Troubleshooting

Headless or check failures: inspect `logs/godot.log` (created by `run_godot.ps1`).

- GDExtension load error 1114: [`wiki/notes/gdextension_error_1114_thread_local.md`](wiki/notes/gdextension_error_1114_thread_local.md)
- Headless file I/O on Windows: [`wiki/notes/godot_windows_headless_file_issue.md`](wiki/notes/godot_windows_headless_file_issue.md)

## Documentation

- [`wiki/README.md`](wiki/README.md) — knowledge base index
- [`wiki/notes/command_reference.md`](wiki/notes/command_reference.md) — headless flags and launcher routing
- [`wiki/notes/native_agent_guide.md`](wiki/notes/native_agent_guide.md) — native edit routing, invariants
- [`wiki/notes/simulation_module_map.md`](wiki/notes/simulation_module_map.md) — module ownership
- [`wiki/notes/draft_ai_validation_gate.md`](wiki/notes/draft_ai_validation_gate.md) — draft AI quantitative validation pipeline
- [`wiki/notes/native_draft_ai.md`](wiki/notes/native_draft_ai.md) — draft subsystem
- [`fixtures/goldens/DATA_SOURCE.md`](fixtures/goldens/DATA_SOURCE.md) — champion data workflow
- [`examples/balance_and_kit_example.gd`](examples/balance_and_kit_example.gd) — balance patches reference
- [`AGENTS.md`](AGENTS.md) — contributor and agent rules
