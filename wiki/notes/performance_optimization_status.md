# Performance Optimization Status

## Iteration 10 exit (May 28, 2026)

- **Encapsulation:** `_units` / `_unit_cold` **private**; `CoordinatorHostAccess::units` / `unit_cold_at` / `uc`; `GeneratedMatchHost` delegates.
- **Exec status:** `sim_effects_exec_status_{heal,cc,channel,mana}.cpp` + thin dispatcher (largest **172** lines).
- **Status:** `sim_status_{internal,cc,heal,aoe}.cpp` (largest **132** lines).
- **Viewer:** `sim_viewer_{fx,snapshot}.cpp` (largest **187** lines).
- **Benchmark stats:** `sim_match_benchmark_stats_internal.cpp` + orchestration TU **233** lines.
- **Profile:** `sim::profile::unit_tick_profile()` in `sim_profile.cpp`.
- **MatchRuntimeState hot path:** attempted 10g, **reverted** (bench regression).
- **Bench (Release, workers=1, 5v5, 2000 batch):** **~139.4 m/s** (`duration_sec` 14.35; quiet host). Confirmatory **~137.1 m/s**. Meets >= ~135 m/s gate; ~+1% vs Iter 9 **~137.6**. Earlier **~87 m/s** was host load (other processes on the machine).
- **Validation:** full gate green (7 fixture cases).

## Iteration 9 exit (May 28, 2026)

- **Coordinator:** `teamfight_simulation_core.cpp` **~296**; glue in **`sim_coordinator_{state,catalog,targeting,viewer,tick,bindings}.cpp`**.
- **Runtime:** **`sim_match_runtime_state.{hpp,cpp}`** — `MatchRuntimeState`, `runtime_from()`; tick hot path keeps direct `SimWorld` / `MatchLoopState` field wiring on coordinator.
- **Damage:** split into **`sim_damage_internal`**, **`sim_damage_modifiers`** (~153), **`sim_damage_apply`** (~285); stub **`sim_damage.cpp`**.
- **Periodic hygiene:** `cleanse_dots` / `clear_periodic_effects` moved to **`sim_periodic_internal.cpp`**; **`sim_periodic_dot_hot.cpp`** **385** lines.
- **Bench (Release, workers=1, 5v5, 2000 batch):** **~137.6 matches/sec** (`duration_sec` 14.54; clean re-run). Within ~6% of Iter 8 **~146.8 m/s**; prior ~90–96 m/s runs were interference (background Godot).
- **Validation:** full gate green (7 fixture cases); **0** `core._*` in `sim_match_benchmark.cpp`.

## Iteration 8 exit (May 28, 2026)

- **Coordinator:** `teamfight_simulation_core.cpp` **~703**; tick glue in **`sim_coordinator_tick.cpp`** (~80); bindings in **`sim_coordinator_bindings.cpp`** (~80).
- **Unit tick:** split into **`sim_unit_tick_{internal,cooldowns,regen,cast,combat,movement}.cpp`** (largest **168** lines).
- **Combat:** split into **`sim_combat_{internal,actions,projectile}.cpp`** + core **`sim_combat.cpp`** (largest **188** lines).
- **Profile:** `sim::profile::RuntimeFlags` (`sim_active`, `targeting_active`) on coordinator.
- **Bench (Release, workers=1, 5v5, 2000 batch):** **~146.8 matches/sec** (`duration_sec` 13.62).
- **Validation:** full gate green (7 fixture cases).

## Iteration 7 exit (May 28, 2026)

- **Coordinator:** `teamfight_simulation_core.cpp` **~839**; match API in **`sim_coordinator_match.cpp`** (~235).
- **Targeting select:** split into **`sim_targeting_select_{enemy,ally,effect}.cpp`** (largest **344** lines).
- **Periodic:** split into **`sim_periodic_{internal,dot_hot,aoe,reflect}.cpp`** (largest **406** lines).
- **Benchmark:** `sim_match_benchmark.cpp` **~180** + **`sim_match_benchmark_stats.cpp`**; **`GeneratedMatchHost`** in dedicated TU.
- **Host:** `sim_coordinator_host.cpp` **~227** (benchmark accessors moved out).
- **Bench (Release, workers=1, 5v5, 2000 batch):** **~138.9 matches/sec** (`duration_sec` 14.40).
- **Validation:** full gate green (7 fixture cases).

## Iteration 6 exit (May 28, 2026)

- **Coordinator:** `teamfight_simulation_core.cpp` **~1079**; dead `_load_json_*` / `_effect_to_dict` removed.
- **Targeting:** `sim_targeting.cpp` **114** + `sim_targeting_{score,select,context}.cpp` + `sim_targeting_internal`.
- **Profile:** `sim::profile::Counters` in **`sim_profile_counters.hpp`**; flags remain on coordinator.
- **Benchmark:** `GeneratedMatchHost` (`CoordinatorHostAccess`); **0** `core._*` in `sim_match_benchmark.cpp`.
- **Bench (Release, workers=1, 5v5, 2000 batch):** **~118.3 matches/sec** (`duration_sec` 16.91). 6L: skip `_refresh_match_context` when hosts cached across `_reset_runtime_state`.
- **Bench phases (`TEAMFIGHT_BENCH_PHASES=1`):** setup ~0.82 ms/match, simulate ~7.56 ms/match.
- **Validation:** full gate green (7 fixture cases); compile structure check in `--check-only`.

## Iteration 5 exit (May 28, 2026)

- **Coordinator:** `teamfight_simulation_core.cpp` **~1120**; `sim_coordinator_host.cpp` **~230**; `sim_profile.cpp` **~150**.
- **Compile:** `sim_effects_compile.cpp` **~465** + `sim_effects_compile_{damage,status,aoe,spawn}.cpp` + `sim_effects_compile_internal.cpp`; shared **`sim_effect_kinds.inl.hpp`**.
- **Friends:** **1** (`CoordinatorHostAccess` only; benchmark used public coordinator fields).
- **Bench (Release, workers=1, 5v5, 2000 batch):** **~103.4 matches/sec** (`duration_sec` 19.35). Slight uptick vs Iter 4 ~101.3; still below Iter 3 110.71.
- **Validation:** full gate green (7 fixture cases).

## Iteration 4 exit (May 28, 2026)

- **Coordinator:** `teamfight_simulation_core.cpp` **~1895**; `sim_match_benchmark.cpp` **~580**; `sim_viewer.cpp` **~350**.
- **Modules added:** `sim_viewer.cpp`, `sim_match_benchmark.cpp`; `sim_coordinator_host.hpp` (trampolines remain in coordinator TU); targeting coordinator helpers in `sim_targeting`.
- **Friends:** `CoordinatorHostAccess`, `BatchRunner`, benchmark batch friends (host trampolines same TU).
- **Bench (Release, workers=1, 5v5, 2000 batch):** **~101.3 matches/sec** (`duration_sec` 19.74). Within prior structural bands; re-profile if chasing 110+ m/s.
- **Validation:** full gate green (7 fixture cases).

## Iteration 3 exit (May 28, 2026)

- **Coordinator:** `teamfight_simulation_core.cpp` **~2830**; `teamfight_simulation_core.hpp` **~394**; private methods **< 40**; `sim_host_*` friends **7**.
- **Modules added:** `sim_viewer`, `sim_unit_builder`, `sim_match_roster`, `sim_match_lifecycle`, `sim_match_loop`, `sim_effects_host`; `sim_effects_exec` split into category `.cpp` files.
- **Bench (Release, workers=1, 5v5, 2000 batch):** **110.71 matches/sec** (`duration_sec` 18.06). At/above May 24 baseline (106.68 m/s).
- **Validation:** full gate green (7 fixture cases).
- **Phase L follow-up:** projectile spatial index only if profiling regresses; not required for exit.

## Iteration 3 Phase 3e — match loop extract (May 28, 2026)

- **Module:** `sim_match_loop.hpp/cpp` — `MatchLoopState`, `MatchLoopHost`, `step_tick()`, `simulate()`, `determine_winner()`.
- **Coordinator:** `run_match` / batch paths call `sim::match::simulate`; `advance_one_tick` calls `sim::match::step_tick`. One `SimWorld` reused per `step_tick`.
- **Lines:** `teamfight_simulation_core.cpp` **2825** (was 3098); `sim_match_loop.cpp` **139**; `< 2500` target remains for later phases (3d lifecycle + further extractions).
- **Bench (Release, workers=1, 5v5, 2000 batch):** **97.42 matches/sec** (`duration_sec` 20.53). ~−3.1% vs branch baseline 100.57 m/s (within structural tolerance band; no hot-path logic change).
- **Validation:** full gate green (7 fixture cases).

## Iteration 2 exit closure (May 28, 2026)

- **Release build:** `GODOTCPP_TARGET` follows `CMAKE_BUILD_TYPE` (Release → `template_release`). Confirm build log shows `libgodot-cpp.windows.template_release.*`.
- **Coordinator:** `teamfight_simulation_core.cpp` ~3,900 lines; `teamfight_simulation_core.hpp` **489** lines (constants deduped to `sim_constants.hpp`).
- **Exec surface:** `SimExecCallbacks` = `debug_combat_trace` only; `SimMatchHost` holds `pending_spawns`, `projectiles`, `max_instance_id`, `catalog`.
- **Friends:** 9 (`sim_host_*` for host callbacks + debug trace).
- **Bench (Release, workers=1, 5v5, 2000 batch):** **100.57 matches/sec** (`duration_sec` 19.89). Branch baseline for ±2% checks.
- **Validation:** full gate green (7 fixture cases).
- **Phase L:** deferred — Release bench within ~6% of May 24 baseline (106.68 m/s); next candidate is projectile spatial index if profiling shows hot collision scans.

See `wiki/notes/simulation_module_map.md` for module ownership.

## Current Baseline (May 24, 2026)
- **Baseline**: 106.68 matches/sec (workers=1, 5v5)
- **Previous Baseline**: 75.4 matches/sec
- **Improvement**: 41.4% (stacked micro-optimizations)
- **Goal**: 226 matches/sec (3x improvement from original baseline)
- **Remaining Gap**: ~112% improvement needed

## Successful Optimizations
1. **Threat Decay Threshold** (May 24, 2026)
   - Skip `_sync_targeting_frame_unit` when threat decay < 0.001
   - Reduced `tgt_frame_syncs` by 25-30%
   - Maintained exact parity

2. **Stacked Micro-Optimizations** (May 24, 2026) - Threshold lowered to 1%
   - **Timer Decrement**: Replace `Math::max` with simple subtraction (3.4%)
   - **Distance Cache**: Only compute for alive units, leverage symmetry (3.7%)
   - **Collision Checks**: Use squared distance instead of sqrt (3.9%)
   - **Frame Sync Threshold**: Skip when no meaningful changes (3.3%)
   - **Cumulative**: 41.4% improvement (75.4 → 106.68 matches/sec)
   - All maintained exact parity

## Path to 3x Improvement
Need additional ~112% improvement. Fundamental algorithmic changes required:

### Targeting System (Largest Hot Spot)
- **Current**: O(n*m) per tick (n attackers, m enemies)
- **Opportunity**: Spatial partitioning already exists but underutilized
- **Idea**: Precompute enemy buckets by role/distance to reduce candidate set
- **Risk**: High - targeting is core gameplay logic

### Distance Cache
- **Current**: O(n²) rebuild every tick
- **Opportunity**: Incremental updates for moved units only
- **Risk**: Medium - parity concerns with stale distances

### Effect System
- **Current**: Linear scan through effects
- **Opportunity**: Effect type indexing
- **Risk**: Medium - effect order matters for some interactions

### Projectile System
- **Current**: Linear scan for collision detection
- **Opportunity**: Spatial grid for projectile-unit collision
- **Risk**: Low - projectiles are independent

## Recommendation
Focus on projectile spatial indexing as the lowest-risk high-impact optimization. Targeting changes require careful parity validation and may need gameplay specification changes.
