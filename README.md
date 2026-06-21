# TeamfightGodot

![Godot](https://img.shields.io/badge/Godot-4.6-478CBF?logo=godotengine&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.24%2B-064F8C?logo=cmake&logoColor=white)
![GDExtension](https://img.shields.io/badge/GDExtension-native-333333)
![Deterministic](https://img.shields.io/badge/simulation-deterministic-2ea043)
![Platform](https://img.shields.io/badge/platform-Windows-0078D6?logo=windows&logoColor=white)

Deterministic autobattler simulation platform built on Godot 4, with a native C++ match engine for production workloads.

The native GDExtension core runs combat, targeting, and batch matches at high throughput. GDScript owns validation, golden fixtures, and developer tooling. Godot scenes provide interactive inspection (viewer, stats dashboard) without coupling UI to the hot simulation path.

## Highlights

- **Deterministic by design** — seeded match loops, reproducible batch runs, and regression checks (`--check-determinism`, fixture parity).
- **Native performance** — C++17 simulation in `native/`; modular coordinator and effect pipeline tuned for large-scale batch execution (multi-thousand matches per gate).
- **Clear layering** — runtime (`TeamfightSimulationCore`), validation layer (fixtures, telemetry, compile checks), and tooling (benchmarks, stats, viewer) stay separated.
- **Regression safety** — golden fixtures under `fixtures/goldens/` lock behavior across native and catalog changes.
- **Agent-oriented docs** — [`wiki/`](wiki/) captures module maps, invariants, and validation gates for ongoing work.

## Architecture

| Layer | Role | Location |
|-------|------|----------|
| Runtime | Match loop, units, effects, damage, targeting | `native/src/` (`teamfight_simulation_core.*`, `simulation/`) |
| Bindings | Godot API, batch hosts, benchmarks | `native/src/`, `scripts/simulation/` |
| Validation | Fixture parity, determinism, telemetry | `fixtures/goldens/`, headless flags via `run_godot.ps1` |
| Tooling & UI | Viewer, stats, catalog authoring | `scripts/`, `scenes/` |

```
  champion_catalog.gd ──► JSON schemas ──► native loader
                                                │
                                         TeamfightSimulationCore
                                          (match loop · combat)
                                                │
              ┌─────────────────────────────────┼─────────────────────────────────┐
              ▼                                 ▼                                 ▼
     fixtures/goldens/                  run_godot.ps1                      scenes/ · viewer
     parity · determinism               benchmarks · checks                stats dashboard
```

Champion and balance data flow from GDScript catalog definitions into JSON consumed by the native loader; edit `scripts/simulation/champion_catalog.gd` (schemas regenerate on build).

## Repository layout

| Path | Purpose |
|------|---------|
| `native/` | C++ GDExtension (CMake) |
| `scripts/` | GDScript simulation helpers, tools, app entry |
| `scenes/` | Viewer and game scenes |
| `fixtures/goldens/` | Parity fixtures and champion/minion schemas |
| `wiki/` | Concepts, module maps, performance status |
| `examples/` | Sample balance/kit scripts |

## Requirements

- Godot **4.6** (project targets GL Compatibility)
- CMake **3.24+**, C++17 toolchain
- PowerShell (Windows; `run_godot.ps1` is the supported launcher)

Set the Godot executable in `run_godot.ps1` if it is not at `C:\Godot\godot.exe`.

## Quick start

```powershell
cmake -B native/build -S native
cmake --build native/build --config Release

.\run_godot.ps1 -- --check-only
.\run_godot.ps1 -- --check-native-load
.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json
```

## Validation gate

Run after native or simulation changes (in order):

```powershell
cmake --build native/build --config Release
.\run_godot.ps1 -- --check-only
.\run_godot.ps1 -- --check-native-load
.\run_godot.ps1 -- --check-match-telemetry
.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json
.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=1
```

Ensure no Godot processes remain when finished. Benchmark expectations and latest numbers: [`wiki/notes/performance_optimization_status.md`](wiki/notes/performance_optimization_status.md).

## Common commands

| Command | Purpose |
|---------|---------|
| `.\run_godot.ps1 -- --check-only` | GDScript preload / compile check |
| `.\run_godot.ps1 -- --check-native-load` | Extension load smoke |
| `.\run_godot.ps1 -- --check-determinism` | Determinism regression |
| `.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json` | Golden parity (9 fixtures) |
| `.\run_godot.ps1 --simulation-viewer` | Interactive match viewer |
| `.\run_godot.ps1 --simulation-viewer --stats-dashboard` | Stats dashboard |
| `.\run_godot.ps1 -- --check-benchmark ...` | Throughput benchmark (see flags below) |

**Benchmark flags:** `--batch-count`, `--team-size`, `--workers`, `--bench-skip-summaries`, `--base-seed`, `--sim-profile`

**Sharded benchmark (multi-process):**

```powershell
.\run_godot.ps1 -- --check-benchmark-sharded --batch-count=2000 --team-size=5 --bench-skip-summaries --shards=8 --workers-per-shard=1
```

All headless runs should go through `run_godot.ps1` (logging, timeouts). After GDScript edits, run `--check-only` before long smoke or benchmark scenes.

## Documentation

- [`wiki/README.md`](wiki/README.md) — knowledge base index
- [`wiki/notes/native_agent_guide.md`](wiki/notes/native_agent_guide.md) — native edit routing, invariants, validation
- [`wiki/notes/simulation_module_map.md`](wiki/notes/simulation_module_map.md) — module ownership
- [`AGENTS.md`](AGENTS.md) — wiki usage for contributors and agents
