# Match Telemetry

> **Native implementation:** `simulation_types.hpp` → `UnitStateRare`, `sim_match.cpp`

Per-unit telemetry counters tracked across a match.

## Cast counters

- `auto_attacks` — incremented when an auto-attack resolves.
- `abilities` — incremented when an ability cast starts.
- `ultimates` — incremented when an ultimate cast starts, regardless of whether the cast later resolves successfully.

Ultimate mana is deducted at cast start. If the cast is aborted or the projectile is evaded, the mana is refunded in `resolve_cast`, but the `ultimates` counter remains incremented.
