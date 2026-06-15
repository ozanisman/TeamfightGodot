# Determinism

Simulation must be deterministic: same input + seed + rules_version → same output.

No nondeterminism allowed: time dependence, unordered iteration, uncontrolled randomness. All RNG goes through CPython-compatible Mersenne Twister. Iteration order is deterministic (use indexed loops, not hash iteration).

Parity testing compares GDScript validation path against native production path using golden fixtures. Fixtures include schema signature (contract hash) and fixture signature (per-match hash). Float tolerance accounts for cross-platform precision limits.

Determinism is validated via fixture parity tests (`--fixture-file=res://fixtures/goldens/match_fixtures.json`) and the `--check-only` gate, which compare GDScript and native outputs for identical inputs and seeds.