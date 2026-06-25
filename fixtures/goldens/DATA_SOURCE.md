# Champion Data Source

## Single Source of Truth

All champion data (stats, abilities, passives, role configs) is defined in:
- `scripts/simulation/champion_catalog.gd`

**Edit champion data here only.** Do not manually edit the JSON files in this directory.

## Regenerating JSON

After editing `champion_catalog.gd`, regenerate the JSON file:

```powershell
godot --headless --script scripts/tools/export_champion_schema.gd
```

This updates:
- `fixtures/goldens/champion_schema.json` - Consumed by both GDScript and C++ backends

## Build Automation

The C++ build process automatically regenerates the JSON before compilation via CMake. CMake requires a Godot executable at configure time (`GODOT_EXE` or `GODOT`, or `C:\Godot\godot.exe`).

From the repo root:

```powershell
cmake -B native/build -S native
cmake --build native/build --config Release
```

The JSON is regenerated automatically if `champion_catalog.gd` has changed.

## File Structure

- `scripts/simulation/champion_catalog.gd` - **Source of truth** (edit here)
- `scripts/tools/export_champion_schema.gd` - Export script

See **Other golden JSON artifacts** below for all tracked JSON files in this directory.

## Other golden JSON artifacts

| File | Source | Notes |
|------|--------|-------|
| `champion_schema.json` | `export_champion_schema.gd` / CMake | Generated from `champion_catalog.gd` |
| `minion_schema.json` | `export_champion_schema.gd` / CMake | Generated from `champion_catalog.gd` alongside `champion_schema.json` |
| `balance_patches.json` | Manually maintained | Author balance patches by hand; validated by `--check-balance-patches` |
| `champion_kits.json` | `kit_catalog.gd` / `KitCatalog.write_kit_json_to_file()` | Generated from `KIT_DATA` in `kit_catalog.gd` |
| `match_fixtures.json` | `--rewrite-fixture-summaries=res://fixtures/goldens/match_fixtures.json` | Real-champion golden parity fixtures (15 cases); see [FIXTURE_BEHAVIORS.md](FIXTURE_BEHAVIORS.md); regenerate via `run_godot.ps1` |
| `contract_schema.json` | `--dump-contract-json` | Match input/summary contract; regenerate when schema changes |
| `champion_kits.example.json` | Reference | Example kit structure only |
