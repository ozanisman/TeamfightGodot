# Native-First Batch Core Todo

## Now
- Keep `BatchRunner` as orchestration and output only.
- Keep `BatchMatchEngine` as the adapter that prefers native and falls back to GDScript.
- Keep `BatchMatchEngineFallback` as the current GDScript match executor.
- Keep `SimulationContract` as the frozen batch schema boundary.
- Keep match summaries, benchmark summaries, and snapshot records contract-driven.
- Native scaffold exists under `native/batch_core/` with `BatchMatchEngineNative`, registration stubs, a build script, and a `.gdextension` manifest.

## Next
- Replace the GDScript unit-factory fallback in `BatchMatchEngine` with native roster materialization when the compiled extension is available.
- Replace the GDScript fallback world step with contiguous native state storage.
- Preserve `set_units`, `set_seed`, `step`, `get_match_result`, `snapshot_units`, and event recording at the native boundary.
- Add a local parity harness for native-vs-fallback batch runs before deleting any fallback shims.
- Wire the Godot project to the compiled GDExtension only after the native path is parity-verified.

## After parity
- Remove remaining batch-path compatibility shims that are only needed by the GDScript fallback.
- Keep the live UI adapters separate from the batch simulation core.
- Add native-versus-GDScript parity checks for fixed-seed batch runs.
