# Godot 4.6 Windows Headless File Bug

## Issue Summary

Godot 4.6.2 stable has a file system bug in headless mode on Windows where `FileAccess.open()` with WRITE mode reports success and verification passes, but files do not persist to disk after script completion. This blocks automated validation and testing workflows.

## Affected Files

- `scripts/tools/verify_draft_aware_signal.gd` — Verification script that should write model stats but fails in headless mode
- `scripts/tools/generate_draft_aware_training_data.gd` — Data generation script (partially mitigated with absolute paths)
- `model_stats/draft_aware_training_250k/` — Output directory where files should be written but aren't persisting

## Observed Behavior

- Script exits with code 0 (success)
- File verification succeeds immediately after writing (can read back content)
- File does not exist on disk after script completion
- Intermittent — some files write, others don't
- Multiple concurrent Godot processes interfere with file operations
- Absolute paths work better than `res://` or `user://` paths
- `OS.delay_msec()` calls don't resolve the issue

## Workarounds Attempted

- Switched from `res://` to `user://` paths
- Switched to absolute Windows paths
- Added incremental writing with periodic flushes
- Added `OS.delay_msec()` for filesystem sync
- Added verification steps
- Killed interfering Godot processes
- Partial success with data generation (large files still fail)

## Sources

1. GitHub Issue #116176 — FileAccess & DirAccess can create invalid files on Windows
   URL: https://github.com/godotengine/godot/issues/116176
   Reproducible in: v4.6.stable.steam [89cea14]
   Windows 11 specific issue with file creation

2. GitHub Issue #118354 — Godot 4.6.2 stable crashes with access violation in headless mode
   URL: https://github.com/godotengine/godot/issues/118354
   Headless mode crashes with `--quit` on Windows

3. Godot Forum Thread — FileAccess not creating nor opening file
   URL: https://forum.godotengine.org/t/fileaccess-not-creating-nor-opening-file/136766
   Godot 4.6 stable on Windows 11
   Files not being created despite successful `FileAccess.open` calls

## Impact

- Blocks 5-seed randomization validation
- Blocks ban recommendation function testing with real weights
- Requires manual non-headless testing or Godot downgrade

## Resolution Options

1. **Manual non-headless testing in Godot editor** (5 minutes)
2. **Downgrade to Godot 4.2.2 or 4.3 stable** (5 minutes install)
3. **Wait for Godot 4.7 fix**

## Current Status

Active workaround: Use manual non-headless testing for affected scripts. Large file generation still unreliable in headless mode.
