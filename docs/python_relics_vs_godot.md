# Python-shaped relics in the native Godot sim core

This document tracks patterns inherited from an external **Python oracle** (not shipped in this repo). Parity is enforced with [fixtures/goldens/match_fixtures.json](../fixtures/goldens/match_fixtures.json) and the headless runner. Items marked **resolved** describe what the native core does today; **open** items are intentional follow-up for a shipping GDExtension.

## 1) Strategy config as `Dictionary` / `Variant` everywhere — **resolved**

- **Current implementation**
  - [`native/src/teamfight_simulation_core.hpp`](../native/src/teamfight_simulation_core.hpp): `struct UnitStrategy` (POD) with `double`/`bool` fields, `std::array<StringName, 8> bucket_order`, `bucket_order_len`, and **`std::map<StringName, double>`** for `role_priorities` / `ally_role_priorities` (MSVC has no `std::hash<godot::StringName>` for `unordered_map`).
  - `_build_role_strategy_cache()` runs at match init; `_strategy_for_unit()` returns `const UnitStrategy&` from `_role_strategy_cache` or `_default_strategy`.
  - Scoring and selection use struct fields, not `Dictionary.get` in the hot path.
- **Python reference**
  - Dataclass-style `TargetingStrategy` with many knobs; the native cache mirrors those values per role.

## 2) Density cache on each unit (`_last_density_count`) — **resolved**

- **Current implementation**
  - `struct TickContext` holds `density_by_unit_index` (per unit index), plus team centers and backliner index lists.
  - `_prepare_tick_context()` runs after projectiles each tick; `_refresh_target_pressure()` fills density into `_tick_ctx` when any alive unit has `cluster_weight > 0`.
  - `UnitState` no longer stores a per-unit density field.
- **Python reference**
  - `world.py` / `targeting.py` cache density once per tick; native matches that phase ordering.

## 3) Debug-only logging in the hot loop — **mostly resolved**

- **Current implementation**
  - Hot-loop `UtilityFunctions::print` traces were removed.
  - `TraceEvent`, `_trace_buffer`, `_emit_trace()`, `get_trace_events()`, and match input flag `debug_combat_trace` provide a structured sink.
  - **Product vs debug:** Full match timelines for replay/UI are **not** yet serialized into `MatchReplaySummary`; use `get_trace_events()` after a match when `debug_combat_trace` is true (see [scripts/simulation/match_replay_input.gd](../scripts/simulation/match_replay_input.gd)). The `record_events` flag is reserved for a future replay-grade event list.
- **Suggested follow-up**
  - Add more `_emit_trace` kinds at stable lifecycle points (target change, cast, death, projectile spawn) as needed for tooling.

## 4) Oracle-driven phase ordering — **resolved (wrapped)**

- **Current implementation**
  - [`native/src/teamfight_simulation_core.cpp`](../native/src/teamfight_simulation_core.cpp): `_step_tick()` / `_update_unit()` order matches the oracle timeline (tick time, projectiles, tick context + target pressure, unit updates, final pressure pass).
  - [`scripts/simulation/sim_runner.gd`](../scripts/simulation/sim_runner.gd): `step(delta)` accumulates fixed-step `advance_one_tick()` for gameplay-style driving without changing internal phases.

## 5) Float-heavy scoring — **open (incremental)**

- **Where**
  - `_score_enemy_target()` and related targeting math.
- **Current mitigation**
  - Distance term: `norm_gap` is quantized (20-bit style) before `pow` to reduce cross-platform drift.
- **Suggested replacement**
  - Further fixed-point or bucketed terms only behind new goldens / explicit version bumps.

## 6) `Dictionary stats` and per-tick work — **partially addressed**

- **Current implementation**
  - `UnitState` still owns `Dictionary stats` for champion JSON and tooling, but a **`CombatStats` snapshot** (numeric fields read in combat/movement/targeting) is populated in `_build_unit_state()` so inner loops avoid repeated `stats.get` for those keys.
  - `TickContext` already caches team centers, backliners, and density.
- **Open**
  - SoA layouts and stripping rare stats from the dictionary entirely if batch scale demands it.
  - **Spatial grids:** native broad-phase for targeting/density/kite/obscurance only when either team has **≥ 6** alive (standard 5v5 at full roster uses brute). Separation uses a grid only at the same **≥ 6** alive-per-team threshold for custom large teams.

## Safe refactor sequence (status)

1. Freeze behavior — goldens + headless parity.
2. ~~Typed `UnitStrategy` at match init~~ — done.
3. ~~`TickContext` + density off `UnitState`~~ — done.
4. ~~Structured trace sink + SimRunner adapter~~ — done; trace call sites can grow over time.
5. ~~Combat stat snapshot for hot paths~~ — done (extend fields if new stats enter the hot loop).
6. Optional: SoA, richer `record_events` / summary events; spatial indexing is partial (see Open above).

## Typo note

Earlier revisions referenced `Python/world.py` paths; the oracle lives outside this repository. Use goldens as the contract unless you mount a Python tree for differential testing.
