# Native Simulation Backend

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → [simulation_module_map.md](../notes/simulation_module_map.md)

Production C++ core via GDExtension. GDScript owns catalog/schema export, validation tooling, and UI — not match simulation.

Native backend (`TeamfightSimulationCore` in [`native/src/teamfight_simulation_core.{hpp,cpp}`](../../native/src/teamfight_simulation_core.hpp)) implements all runtime combat in C++; gameplay modules live under [`native/src/simulation/`](../../native/src/simulation/). See [simulation_module_map.md](../notes/simulation_module_map.md). `simulation_backend.gd` is an abstract stub only.

The `native_simulation_backend.gd` script wraps GDExtension loading and method calls. HeadlessRunner and the simulation viewer use this backend. Smoke load: `--check-native-load`.

Performance optimization: native owns deterministic draft generation for benchmark fast paths (`run_generated_matches_simulation_only`, `run_generated_matches_stats_partial`). Stats-only mode skips summary allocation.

Incremental API (begin_match, advance_one_tick, finish_and_summarize) supports simulation viewer and gameplay loops.
