# Performance Optimization Status

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
