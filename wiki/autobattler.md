# Autobattler Project

Deterministic teamfight simulation system with Godot 4 and C++.

Architecture: Native C++ GDExtension core (TeamfightSimulationCore) for production performance, GDScript reference implementation for validation. Simulation runs in fixed 0.1s ticks with 60s match duration.

Core systems: Effect system (modular ability/ultimate/passive execution), targeting AI (scoring-based enemy/ally selection with role strategies), combat pipeline (tick-based loop), champion system (stats, abilities, passives, roles), RNG streams (CPython-compatible Mersenne Twister), schema contract (parity testing framework).

Data flow: Match input (seed, team compositions) → simulation → match summary (winner, duration, unit stats). Fixtures provide golden outputs for regression testing. Balance patches override champion stats/kits via versioned contracts.

Tooling: Headless CLI for batch simulation, fixture testing, benchmarking. Simulation viewer for visual debugging. Stats dashboard for balance analysis.