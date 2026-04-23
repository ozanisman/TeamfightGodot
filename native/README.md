# Native Simulation Core

This directory holds the future compiled simulation runtime for Teamfight.

Current state:
- the production game still uses the GDScript fallback core
- this folder contains the native API skeleton and build target layout
- the runtime backend will switch to this implementation once the library is built and loaded

Expected contract:
- `TeamfightSimulationCore.clear()`
- `TeamfightSimulationCore.run_match(match_input)`
- `TeamfightSimulationCore.run_matches(match_inputs)`

The native implementation must preserve the existing replay contract and parity fixture shapes.
