# Algorithmic Optimization Analysis

## Current Performance
- Baseline: 106.68 matches/sec (41.4% improvement from micro-optimizations)
- Goal: 226 matches/sec (3x from original baseline)
- Remaining Gap: ~112% improvement needed

## Attempted Optimizations

### 1. Spatial Grid Caching (FAILED - Parity Issue)
**Current Behavior:**
- Spatial grid is rebuilt 3-5 times per tick for different operations:
  - Obscurance aux grid (line 6871, 6880)
  - Density computation (line 4062)
  - Separation (line 8121)
  - Kiting (line 8528)
- Each rebuild: O(n) where n is alive units
- Grid rebuild involves clearing buckets and re-inserting all units

**Attempted Optimization:**
- Build spatial grid once per tick per team
- Cache and reuse across all operations

**Result:**
- **FAILED** - Broke parity (effect_control_wall fixture)
- Issue: Different operations require different unit sets (frontline, specific teams, etc.)
- The grid is operation-specific, not just team-specific
- Pre-building a single grid doesn't work because each operation needs filtered subsets

### 2. Skip Targeting for CC'd Units (FAILED - Parity Issue)
**Current Behavior:**
- All units evaluate targeting every tick
- Stunned/rooted units can't act but still run targeting logic

**Attempted Optimization:**
- Skip targeting evaluation for stunned/rooted units
- They can't switch targets or attack anyway

**Result:**
- **FAILED** - Broke parity (effect_control_wall fixture, winner changed)
- Issue: Targeting state is needed even for CC'd units
- Used for assist tracking, threat calculations, and other gameplay systems
- Skipping targeting breaks these systems

## High-Impact Opportunities Remaining

### 1. Targeting Score Precomputation (High Impact, Medium Risk)
**Current Behavior:**
- Each attacker scores all enemies independently (O(n*m))
- Bodyguard scoring loops through carry indices for every candidate (line 3519-3535)
- Distance calculations repeated across attackers

**Optimization:**
- Precompute enemy scores once per tick per team
- Cache base scores (distance, HP ratio, role bonuses)
- Each attacker only computes attacker-specific modifiers

**Expected Impact:**
- Changes O(n*m) to O(n) + O(m) where n=attackers, m=enemies
- Eliminates redundant distance and HP calculations
- Estimated: 20-30% improvement

**Risk:**
- Medium - scores depend on attacker strategy
- Functional impact: May change targeting behavior if not carefully implemented
- Requires strategy-specific modifiers to be applied correctly

### 3. Distance Cache Incremental Updates (Medium Impact, Medium Risk)
**Current Behavior:**
- Full O(n²) rebuild every tick (line 2820-2853)
- Already optimized to only compute for alive units

**Optimization:**
- Track which units moved during tick
- Only update distances for moved units
- Use dirty flag per unit

**Expected Impact:**
- Reduces from O(n²) to O(m*d) where m=moved units, d=total units
- Most units don't move significantly each tick
- Estimated: 10-15% improvement

**Risk:**
- Medium - parity concerns with stale distances
- Functional impact: Minimal if movement tracking is accurate

### 4. Effect System Indexing (Low Impact, Low Risk)
**Current Behavior:**
- `_collect_effects` uses if-else chain (line 4160-4190)
- 9 effect kinds, linear lookup

**Optimization:**
- Use array indexing instead of if-else chain
- Map effect kind to array index

**Expected Impact:**
- Eliminates branch mispredictions
- Estimated: 2-3% improvement

**Risk:**
- Low - purely implementation change
- Functional impact: None

## Recommended Approach

**Phase 1: Targeting Score Precomputation (Highest ROI, Medium Risk)**
- Precompute enemy base scores per team
- Cache distance, HP ratio, role bonuses
- Apply attacker-specific modifiers
- Expected: 20-30% improvement
- Note: May require fixture updates if gameplay changes are acceptable

**Phase 2: Distance Cache Incremental Updates**
- If Phase 1 insufficient, implement movement tracking
- Only update distances for moved units
- Expected: Additional 10-15% improvement

## Implementation Priority
1. **Targeting Score Precomputation** - Start here (highest impact)
2. **Distance Cache Incremental Updates** - If more improvement needed
3. **Effect System Indexing** - Low priority, minimal impact

## Current Status Summary

**Performance:**
- Current: 106.68 matches/sec (41.4% improvement from micro-optimizations)
- Goal: 226 matches/sec (3x from original baseline)
- Remaining Gap: ~112% improvement needed

**Failed Algorithmic Optimizations:**
1. Spatial Grid Caching - Parity issue (operation-specific unit sets)
2. Skip Targeting for CC'd Units - Parity issue (targeting state needed for assists/threat)

**Key Insight:**
The codebase is already highly optimized. The remaining optimizations that could achieve 2x+ improvement require accepting gameplay behavior changes. The targeting system is tightly coupled to gameplay mechanics (assists, threat, bodyguard, cluster scoring), and optimizing it significantly would change unit behavior.

**Options for User:**
1. **Accept Gameplay Changes**: Implement targeting score precomputation with simplified scoring, accepting that targeting behavior will change. Update fixtures to match new behavior.
2. **Lower Performance Goal**: Accept that 3x may not be achievable without gameplay changes. Current 41.4% improvement is significant.
3. **Continue Micro-Optimizations**: Look for additional 1-5% optimizations (e.g., effect system indexing, branch prediction improvements) to chip away at the gap.

**Recommendation:**
Given that the user stated "some impact on functionality is acceptable if the performance gains are significant", the best path forward is to implement targeting score precomputation with simplified scoring, accepting the gameplay changes and updating fixtures accordingly. This is the only optimization likely to achieve the remaining 112% improvement needed.
