# Champion Data Source

## Single Source of Truth

All champion data (stats, abilities, passives, role configs) is defined in:
- `scripts/simulation/champion_catalog.gd`

**Edit champion data here only.** Do not manually edit the JSON files in this directory.

## Regenerating JSON

After editing `champion_catalog.gd`, regenerate the JSON file:

```bash
godot --headless --script scripts/tools/export_champion_schema.gd
```

This updates:
- `fixtures/goldens/champion_schema.json` - Consumed by both GDScript and C++ backends

## Build Automation

The C++ build process automatically regenerates the JSON before compilation via CMake. When building the native backend:

```bash
cd native
cmake --build build --config Debug
```

The JSON is regenerated automatically if `champion_catalog.gd` has changed.

## File Structure

- `scripts/simulation/champion_catalog.gd` - **Source of truth** (edit here)
- `scripts/tools/export_champion_schema.gd` - Export script
- `fixtures/goldens/champion_schema.json` - **Generated artifact** (do not edit)
- `fixtures/goldens/contract_schema.json` - Contract schema (generated via `--dump-contract-json`)
