# Determinism

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md)

Simulation must be deterministic: same input + seed + rules_version → same output.

No nondeterminism allowed: time dependence, unordered iteration, uncontrolled randomness. All match RNG goes through the native CPython-compatible Mersenne Twister. Iteration order is deterministic (use indexed loops, not hash iteration).

All runtime combat runs through the native backend (`TeamfightSimulationCore` via `native_simulation_backend.gd`). There is no GDScript simulation implementation.

Golden fixture parity compares native match output against committed expected summaries in `fixtures/goldens/match_fixtures.json`. Fixtures include schema signature (contract hash) and fixture signature (per-match hash). Float tolerance accounts for cross-platform precision limits.

Determinism is validated via:
- `--fixture-file=res://fixtures/goldens/match_fixtures.json` — golden parity (17 fixtures)
- `--check-determinism` — reruns fixture inputs twice on native and compares summaries
- `--check-only` — GDScript preload/compile gate only (not a parity test)

See also [schema_contract.md](schema_contract.md), [rng_streams.md](rng_streams.md).
