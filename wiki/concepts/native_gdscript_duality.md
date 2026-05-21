# Native GDScript Duality

Production C++ core with GDScript validation path.

Native backend (TeamfightSimulationCore) implements simulation in C++ via GDExtension. GDScript backend provides reference implementation for validation. Both paths must produce identical outputs for parity.

NativeSimulationBackend wraps GDExtension loading and method calls. HeadlessRunner supports both backends via `--check-native-load` flag. Native core required for production benchmarks; GDScript used for development and testing.

Performance optimization: native owns deterministic draft generation for benchmark fast paths (`run_generated_matches_simulation_only`, `run_generated_matches_stats_partial`). Stats-only mode skips summary allocation.

Incremental API (begin_match, advance_one_tick, finish_and_summarize) supports simulation viewer and gameplay loops.
