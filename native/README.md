# Native Simulation Core

This directory holds the future compiled simulation runtime for Teamfight.

Current state:
- the native core owns a real match runtime state shape
- the compiled path currently bridges to the reference GDScript oracle for parity
- the project root contains [teamfight_simulation_core.gdextension](/C:/Users/ozani/Desktop/TeamfightGodot/teamfight_simulation_core.gdextension)
- the production runtime will switch to the native path once the library is built and loaded

Expected contract:
- `TeamfightSimulationCore.clear()`
- `TeamfightSimulationCore.run_match(match_input)`
- `TeamfightSimulationCore.run_matches(match_inputs)`

The native implementation must preserve the existing replay contract and parity fixture shapes.
