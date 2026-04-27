# TeamfightGodot

TeamfightGodot is the Godot 4 rewrite of the Teamfight autobattler simulator. The current project shape is:

- native `TeamfightSimulationCore` in C++ for the production match engine
- GDScript harnesses for replay, parity, batch jobs, stats, and viewer tooling
- frozen goldens in `fixtures/goldens/` as the source of truth for combat parity

## Layout

- `native/` - GDExtension core and build files
- `scripts/` - GDScript runtime, tools, and batch workers
- `scenes/` - main game, stats dashboard, and viewer scenes
- `fixtures/goldens/` - replay parity fixtures
- `docs/` - workflow, testing, and benchmark notes
- `logs/` - benchmark and run artifacts

## Entry points

- Main scene: `scenes/game_root.tscn`
- Visual simulation: `scenes/simulation_viewer.tscn`
- Stats dashboard: `scenes/stats_dashboard.tscn`

Run everything from the repo root through [`run_godot.ps1`](run_godot.ps1). It expects `C:\Godot\godot.exe` unless you change the script.

## Quick start

1. Build the native extension in `native/build`:
   ```powershell
   cmake --build native/build --config Release
   ```
2. Verify scripts and native load:
   ```powershell
   .\run_godot.ps1 -- --check-only
   .\run_godot.ps1 -- --check-native-load
   ```
3. Run fixture parity:
   ```powershell
   .\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json
   ```

## Common commands

Compile/load check:
```powershell
.\run_godot.ps1 -- --check-only
```

Native load smoke:
```powershell
.\run_godot.ps1 -- --check-native-load
```

Determinism subset:
```powershell
.\run_godot.ps1 -- --check-determinism
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

Dashboard smoke load:
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

## Useful flags

- `--batch-count=N` - number of matches for batch runs
- `--team-size=N` - roster size per side
- `--workers=N` / `--max-workers=N` - worker cap for benchmark runs
- `--bench-skip-summaries` - skip replay summary construction when benchmarking
- `--base-seed=N` - starting seed for batch runs
- `--player=...` / `--enemy=...` - archetype lists for single batch runs
- `--tick-rate=N` - override input tick rate
- `--export-workers=N` - cap stats export threads; `0` uses the detected auto cap

## Notes

- `--check-only` is the first check to run after editing GDScript.
- `--bench-skip-summaries` only enables the native batch path when `--workers=1`.
- The root-certificate warning on Windows can appear during startup and is not necessarily a failure.
- If native loading fails, the app falls back to the GDScript simulation core.
- More detailed workflow notes live in [`docs/simulation_and_testing.md`](docs/simulation_and_testing.md).
