# Native Simulation Backend

Production C++ core via GDExtension.

Native backend (TeamfightSimulationCore) implements simulation in C++ via GDExtension. NativeSimulationBackend wraps GDExtension loading and method calls. HeadlessRunner uses the native backend via `--check-native-load` flag.

Performance optimization: native owns deterministic draft generation for benchmark fast paths (`run_generated_matches_simulation_only`, `run_generated_matches_stats_partial`). Stats-only mode skips summary allocation.

Incremental API (begin_match, advance_one_tick, finish_and_summarize) supports simulation viewer and gameplay loops.
