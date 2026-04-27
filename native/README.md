# Native Simulation Core

Native GDExtension runtime for TeamfightGodot.

## Current state

- `TeamfightSimulationCore` is the production match engine target.
- GDScript remains the reference fallback and harness layer.
- The native path must preserve replay, parity, and fixture shapes.
- The extension entry point lives at [`../teamfight_simulation_core.gdextension`](../teamfight_simulation_core.gdextension).

## Core contract

- `TeamfightSimulationCore.clear()`
- `TeamfightSimulationCore.run_match(match_input)`
- `TeamfightSimulationCore.run_matches(match_inputs)`
- `TeamfightSimulationCore.run_match_simulation_only(match_input)`
- `TeamfightSimulationCore.run_matches_simulation_only(match_inputs)`
- `TeamfightSimulationCore.begin_match(match_input)`
- `TeamfightSimulationCore.advance_one_tick()`
- `TeamfightSimulationCore.finish_and_summarize()`

## Useful commands

Run all commands from the repo root with [`run_godot.ps1`](../run_godot.ps1). User args go after `--`.

Compile/load check:
```powershell
.\run_godot.ps1 -- --check-only
```

Native load smoke:
```powershell
.\run_godot.ps1 -- --check-native-load
```

Fixture parity:
```powershell
.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json
```

Visual simulation:
```powershell
.\run_godot.ps1 --simulation-viewer
```

Stats dashboard:
```powershell
.\run_godot.ps1 --simulation-viewer --stats-dashboard
```
Smoke load:
```powershell
.\run_godot.ps1 -- --check-stats-dashboard
```

Batch stats export:
```powershell
.\run_godot.ps1 --generate-stats -- --out-dir=res://stats_output --team-sizes=1,2,3,4,5 --matches-per-size=100 --base-seed=0 --export-workers=0
```

Primary 5v5 throughput gate:
```powershell
.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=1
```

## Notes

- `--bench-skip-summaries` only enables the native batch path for `--workers=1`.
- `--export-workers=0` lets the stats export pick the detected max cap automatically.
- `--stats-dashboard` opens [`scenes/stats_dashboard.tscn`](../scenes/stats_dashboard.tscn) through [`scripts/app/game_root.gd`](../scripts/app/game_root.gd).
- On Windows, the known root-certificate warning can appear during Godot startup and does not necessarily mean the run failed.
