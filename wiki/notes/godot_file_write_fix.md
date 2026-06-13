#
**RESOLVED** - The issue was NOT a Godot 4.6 file-write bug. Both scripts had parse errors or were invoked via the wrong launcher path.

### Solution Implemented

| Item | Fix |
|------|-----|
| `verify_draft_aware_signal.gd` | `trained_weights` dict restored; writes via `ProjectSettings.globalize_path()` |
| `generate_draft_aware_training_data.gd` | Uses `globalize_path`, `_ensure_parent_dir`, incremental write |
| `run_godot.ps1` | Added `--generate-draft-aware-training-data` / `--verify-draft-aware-signal` with 900s/300s timeouts, log failure detection, and `Assert-DurableOutput` post-exit checks |

### Validated

- Direct `godot.exe --script` — both scripts write and persist files
- `.\run_godot.ps1 -- --verify-draft-aware-signal` — exit 0, durable CSV verified (231 bytes)
- `.\run_godot.ps1 -- --generate-draft-aware-training-data` — exit 0, durable CSV verified (186 bytes smoke)
- `--check-only` — passes

### Important Notes

1. Godot 4.6 Windows FS bugs ([#116176](https://github.com/godotengine/godot/issues/116176)) affect trailing `.`/space in filenames only — not CSV paths
2. Use `ProjectSettings.globalize_path()` for reliable file operations
3. Kill stray `godot` processes before long batch runs to avoid races on output files
