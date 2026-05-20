# TeamfightGodot

High-performance autobattler simulator with Godot 4 and C++. Native GDExtension core for production simulation, GDScript for validation and tooling.

## Structure

- `native/` - C++ GDExtension core (CMake)
- `scripts/` - GDScript simulation, tools, and app logic
- `scenes/` - Game and viewer scenes
- `fixtures/goldens/` - Parity fixtures and champion data
- `docs/` - Testing and benchmark documentation

## Setup

**Requirements**: Godot 4.6 at `C:\Godot\godot.exe`, CMake 3.24+, C++17 compiler, PowerShell

```powershell
# Build native extension
cd native
cmake -B build -S .
cmake --build build --config Release

# Verify
cd ..
.\run_godot.ps1 -- --check-only
.\run_godot.ps1 -- --check-native-load
.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json
```

## Common Commands

```powershell
.\run_godot.ps1 -- --check-only                    # GDScript compile check
.\run_godot.ps1 -- --check-native-load            # Extension load check
.\run_godot.ps1 -- --check-determinism             # Determinism regression
.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json  # Parity tests
.\run_godot.ps1 --simulation-viewer               # Visual viewer
.\run_godot.ps1 --simulation-viewer --stats-dashboard  # Stats dashboard
.\run_godot.ps1 -- --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=1  # 5v5 benchmark
```

## Key Flags

| Flag | Purpose |
|------|---------|
| `--batch-count=N` | Match count for batch runs |
| `--team-size=N` | Roster size per side |
| `--workers=N` | Thread cap for benchmarks |
| `--bench-skip-summaries` | Skip summaries for throughput |
| `--base-seed=N` | Starting seed |
| `--sim-profile` | Enable profiling |

## Development

**Champion data**: Edit `scripts/simulation/champion_catalog.gd`. JSON auto-regenerates on build.

**Validation sequence**: `--check-only` → `--check-native-load` → `--check-match-telemetry` → `--fixture-file` → `--check-benchmark`

**Sharded benchmarking** (multi-process):
```powershell
.\run_godot.ps1 -- --check-benchmark-sharded --batch-count=2000 --team-size=5 --bench-skip-summaries --shards=8 --workers-per-shard=1
```

## Documentation

- [`docs/simulation_and_testing.md`](docs/simulation_and_testing.md) - Testing and benchmarking guide
- [`docs/benchmark_rundown.md`](docs/benchmark_rundown.md) - Benchmark methodology
- [`AGENTS.md`](AGENTS.md) - Contributing guidelines

## Notes

- All Godot runs use `run_godot.ps1` for logging and timeouts
- `--check-only` is first check after GDScript edits
- Production requires native core; GDScript is for validation only
