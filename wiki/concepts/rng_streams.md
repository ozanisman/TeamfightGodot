# RNG Streams

Python-compatible Mersenne Twister for deterministic simulation.

Native core implements `CPythonRandom` (python_random.hpp) matching Python 3's `_random.Random`. Seeds via `seed_int64(int64_t)` using little-endian limb encoding. Generates `random_random()` returning 53-bit double in [0, 1). Verified in C++ code: `_rng.seed_int64(_seed)` called on match initialization.

Determinism requires: same seed + same rules_version + same input → same output. No time dependence, no unordered iteration, no uncontrolled randomness. All RNG calls go through the single `_rng` instance per match.

GDScript validation path uses Godot's RNG; native production path uses CPython-compatible implementation. Parity tests ensure both paths produce identical outputs for given seeds.
