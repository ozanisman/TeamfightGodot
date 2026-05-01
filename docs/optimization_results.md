# Native Optimization Results & Next Steps

## Tested Optimizations (May 1, 2026)

### 1. Math::pow() Optimizations
**Changes Made:**
- Replaced `Math::pow(x, 1.5)` with `x * Math::sqrt(x)` in distance scoring
- Replaced `Math::pow(x, 1.5)` with `x * Math::sqrt(x)` in spacing awareness
- **Theory:** pow() uses log/exp operations, sqrt+mul should be faster

**Results:** No measurable performance improvement
- Baseline: ~527-537 matches/sec
- After optimization: ~525-537 matches/sec
- **Conclusion:** Compiler already optimizes simple exponents; optimization ineffective

### 2. Distance Calculation Optimizations
**Changes Made:**
- Pre-computed `dist_sq` before `Math::sqrt()` in selection loops
- Cached current target distance using `current_target_dist_for_switch`
- **Theory:** Avoid redundant sqrt operations

**Results:** No measurable performance improvement
- **Conclusion:** Most distances already passed as parameters; redundant calculations minimal

### 3. Division Operation Optimizations
**Changes Made:**
- Pre-computed `1.0 / BODYGUARD_RADIUS` constant
- Pre-computed `1.0 / len_prod` in flanking calculations
- Cached `1.0 / dist` in kite threat loops (both spatial and non-spatial)
- **Theory:** Division is expensive, multiplication is cheap

**Results:** No measurable performance improvement
- **Conclusion:** Modern CPUs handle division efficiently; caching overhead outweighs benefits

### 4. Distance Caching System
**Changes Made:**
- Implemented tick-based distance matrix cache
- Pre-computed all unit-to-unit distances each tick
- Replaced 15-20 Math::sqrt() calls per unit with cache lookups
- **Theory:** Eliminate redundant distance calculations with O(n²) pre-computation

**Results:** 4% performance regression (533 → 512 matches/sec)
- **Conclusion:** Cache rebuild overhead (100 sqrt + memory allocation) exceeded savings
- **Issue:** Tick-based rebuild is too expensive when units move every tick
- **Lesson:** Caching strategies must account for update frequency vs lookup frequency

## Performance Summary

| Optimization | Expected Gain | Actual Gain | Status |
|--------------|---------------|-------------|---------|
| Math::pow() replacement | 10-15% | 0% | ❌ Failed |
| Distance caching | 8-12% | -4% | ❌ Failed (regression) |
| Division optimization | 3-5% | 0% | ❌ Failed |
| Target stickiness | 15-25% | 0.6% | ✅ Minor gain |

**Overall Performance:** Remained at ~527-537 matches/sec throughout all optimizations

## Key Learnings

1. **Micro-optimizations are ineffective** when the algorithm is already well-optimized
2. **Modern compilers are aggressive** in optimizing simple mathematical operations
3. **Profiling is essential** - don't optimize without measurement
4. **Code complexity vs performance** - added complexity without benefit is technical debt

## Next Focus Areas

### High-Impact Algorithmic Improvements

1. **Candidate Reduction in Targeting Loops**
   - Implement distance-based pruning (skip candidates > 2x attack range)
   - Add early termination when "good enough" target found
   - Use spatial partitioning more effectively

2. **Spatial Grid Enhancements**
   - Improve broad-phase activation thresholds
   - Optimize bucket-based candidate filtering
   - Reduce O(n²) scanning in targeting

3. **Target Stickiness Optimization**
   - Reduce frequency of target re-evaluation
   - Cache target scores between ticks
   - Implement hysteresis for target switching

4. **Vectorization/SIMD Operations**
   - Batch distance calculations
   - Parallelize scoring operations
   - Use CPU vector instructions

5. **Data Structure Improvements**
   - Better spatial indexing (quadtree/k-d tree)
   - Pre-computed distance matrices
   - Cache-friendly data layout

### Implementation Strategy

1. **Start with algorithmic changes** that reduce computational complexity
2. **Profile each change** with at least 3 benchmark runs
3. **Require 5%+ improvement** before accepting optimizations
4. **Maintain exact parity** with existing behavior
5. **Document all changes** with performance impact

## Testing Methodology

- **Command:** `.\run_godot.ps1 --check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=3`
- **Baseline:** ~527-537 matches/sec (median ~533)
- **Acceptance:** 5%+ sustained improvement across multiple runs
- **Validation:** Parity tests must pass after each change

## Conclusion

The micro-optimization approach failed because the targeting algorithm is already well-optimized by the compiler. Future efforts should focus on algorithmic improvements that reduce the fundamental computational complexity rather than micro-optimizing individual operations.
