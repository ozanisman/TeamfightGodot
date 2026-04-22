# Batch Core Native Scaffold

This directory is the starting point for the native batch engine.

Target shape:
- `set_units`
- `set_seed`
- `step`
- `get_match_result`
- `snapshot_units`
- optional event recording

GDExtension entrypoint:
- `BatchMatchEngineNative`
- `register_types.cpp`

Build:
- `.\native\batch_core\build.ps1 -Config Debug`
- `.\native\batch_core\build.ps1 -Config Release`
- requires `GODOT_CPP_DIR` to point at a godot-cpp checkout
- optionally accepts `GODOT_CPP_LIB` if you want to pin a specific library file

The project currently uses the GDScript fallback in `scripts/batch_match_engine.gd`.
The native class is wired as a runtime bridge for the current fallback and becomes the default path when the DLL exists.
