# Python-shaped relics in the native Godot sim core

This document lists implementation patterns that were intentionally copied from the Python oracle to achieve parity, but which are **not ideal long-term** for a shipping Godot/GDExtension runtime. Keep them until fixture parity is stable; then refactor while preserving behavior.

## 1) Strategy config as `Dictionary` / `Variant` everywhere

- **Where**
  - `native/src/teamfight_simulation_core.cpp`: `_strategy_for_unit()`, `_score_enemy_target()`, `_select_enemy_target()`, `_score_ally_target()`, `_select_ally_target()`
  - `native/src/teamfight_simulation_core.hpp`: strategy-related constants + `Dictionary`/`Variant` use in interfaces
- **Why it exists (Python parity)**j
  - Python uses a dataclass (`TargetingStrategy`) with many knobs and defaults; porting as a `Dictionary` preserved flexibility while behavior was in flux.
- **Why it’s a poor fit in Godot/native**
  - Repeated `Dictionary.get(...)` and `Variant` conversions are slower and harder to profile.
  - Higher risk of silent type/shape mistakes (missing keys, unexpected `Variant` types).
  - Makes hot-loop code “data-driven” in the slowest possible way.
- **Suggested replacement**
  - Replace `Dictionary strategy` with a typed POD:

    - `struct Strategy { double distance_weight; double hp_weight; ... Array bucket_order; ... }`
    - Prebuild per-role `Strategy` instances once (or at match init).
  - Keep a mapping layer that can still ingest JSON/`Dictionary` at init time, but **never** do `Dictionary.get` inside scoring/select loops.

## 2) Density cache semantics copied from Python (`_last_density_count`)

- **Where**
  - `native/src/teamfight_simulation_core.hpp`: `UnitState.last_density_count`
  - `native/src/teamfight_simulation_core.cpp`: `_refresh_target_pressure()` (density cache), `_score_enemy_target()` (cluster scoring)
  - Python reference: `Python/world.py` (sets `u._last_density_count`), `Python/targeting.py` (reads it)
- **Why it exists (Python parity)**
  - Python computes and stores a density value once per tick, then scoring reads that cached value.
- **Why it’s a poor fit in Godot/native**
  - It couples targeting math to a cached side-channel value that must be updated in exactly the right phase.
  - It’s easy to desync if future optimizations change when/where caches are populated.
- **Suggested replacement**
  - Prefer one of:
    - **Explicit per-tick prepass caches** in a dedicated “tick context” struct (not per-unit fields), e.g. `TickContext.density_count[unit_index]`.
    - A spatial grid query performed only when needed (and only for units/strategies that require it), with clear ownership and lifetime.
  - If you keep caching, make it an intentional subsystem with a single entry point: `build_targeting_caches_for_tick(...)`.

## 3) Debug-only fields/logging woven into sim state

- **Where**
  - `native/src/teamfight_simulation_core.cpp`: gated `PREACT`, `MOVE`, `KITE`, combat trace prints
  - `native/src/teamfight_simulation_core.hpp`: debug flags (`_debug_targeting`, `_debug_combat_trace`, `_debug_fixture_name`)
  - Python reference: “just attach attributes” pattern used in debug scripts + runtime
- **Why it exists (Python parity)**
  - The parity workflow needs tight, timestamped traces to align timelines and branch signatures.
- **Why it’s a poor fit in Godot/native**
  - Debug printing in hot loops can dominate runtime and distort timings in large batch runs.
  - Mixing debug conditionals into core logic increases maintenance cost and obscures the “real” rules.
- **Suggested replacement**
  - Introduce a structured **trace sink**:
    - ring buffer of small POD events (`TraceEvent { t, kind, src, tgt, ... }`)
    - optionally flushed to Godot logs when requested
  - Keep a single `if (trace_enabled)` guard at call sites, but keep core logic readable.

## 4) “Oracle-driven” phase ordering (World.step / Unit.update mirroring)

- **Where**
  - `native/src/teamfight_simulation_core.cpp`: `_step_tick()` and `_update_unit()` ordering
  - Python reference: `Python/world.py::World.step()`, `Python/unit.py::Unit.update()`
- **Why it exists (Python parity)**
  - Exact phase order (time advance, projectile resolution, pressure refresh, per-unit update, final pressure refresh) is critical to match the oracle timeline.
- **Why it’s a poor fit in Godot/gameplay (sometimes)**
  - Godot gameplay may prefer different integration points (e.g., `_physics_process` cadence, pause/slowmo, rollback, client prediction).
  - Oracle-driven ordering can feel unintuitive to engine-centric developers.
- **Suggested replacement**
  - Keep the internal sim phases as-is (determinism), but wrap them in a Godot-facing adapter:
    - `SimRunner.step(dt)` accumulates and calls `sim.step_tick()` fixed-step.
    - Keep UI/FX in a separate layer consuming emitted events.

## 5) Float-heavy scoring tuned for Python

- **Where**
  - `native/src/teamfight_simulation_core.cpp`: `_score_enemy_target()` (many float math heuristics)
  - Python reference: `Python/targeting.py`
- **Why it exists (Python parity)**
  - The oracle uses float scoring; we mirrored it to achieve identical choices.
- **Why it’s a poor fit long-term**
  - Float heuristics are harder to reason about, harder to keep stable across tuning changes, and can be sensitive to minor geometry differences.
- **Suggested replacement**
  - After parity: consider re-expressing some heuristics in:
    - fixed-point or integer scoring components,
    - explicit priority buckets with fewer continuous terms,
    - or precomputed tables for role matchups.
  - Only do this once you have confidence tests, because it will intentionally change behavior.

## 6) “Everything computed every tick” approach

- **Where**
  - `native/src/teamfight_simulation_core.cpp`: repeated per-unit computations (strategy fetch, distances, scans)
  - Python reference: Python can accept higher per-tick overhead due to lower sim scale
- **Why it’s a poor fit for 100k match batch runs**
  - O(n²) scans (even when small n) and frequent recomputation add up.
  - Prevents easy parallelization and cache-friendly layouts.
- **Suggested replacement**
  - Introduce a tick context with cached aggregates (team centers, backliner lists, etc.).
  - Move toward SoA layouts for hot fields (pos/hp/team/role) and avoid `Dictionary stats` lookups in inner loops.

## Safe refactor sequence (after parity)

1. **Freeze behavior**: keep fixtures passing for N runs.
2. Replace strategy `Dictionary` with typed `Strategy` while preserving values/keys.
3. Move caches (density, team center) into a `TickContext` built once per tick.
4. Replace trace prints with a structured trace sink.
5. Optimize data layout (SoA) and reduce `Dictionary stats` in hot loops.

