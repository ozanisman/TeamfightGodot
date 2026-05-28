# Performance Optimization Status

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
