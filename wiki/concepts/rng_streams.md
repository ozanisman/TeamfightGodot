# RNG Streams

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md)

Python-compatible Mersenne Twister for deterministic simulation.

Native core implements `CPythonRandom` (`python_random.hpp`) matching Python 3's `_random.Random`. Seeds via `seed_int64(int64_t)` using little-endian limb encoding. Generates `random_random()` returning 53-bit double in [0, 1). Verified in C++ code: `_rng.seed_int64(_seed)` called on match initialization.

Determinism requires: same seed + same rules_version + same input → same output. No time dependence, no unordered iteration, no uncontrolled randomness. All match RNG calls go through the single `_rng` instance per match in the native core.

GDScript tooling does not drive match RNG. Draft research scripts and UI use Godot's RNG only outside the hot simulation path.

See also [determinism.md](determinism.md).
