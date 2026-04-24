# Optimization Action Plan

**Purpose:** Detailed, code-level recommendations for performance improvements without affecting simulation fidelity.

---

## Optimization #1: Memoize Enemy Team Center (HIGH PRIORITY)

### Current Implementation

**File:** `native/src/teamfight_simulation_core.cpp`  
**Location:** Lines 2046-2050 (approx, `_score_advanced_tactics` in flanking section)

```cpp
// Current: Recalculated for EVERY scoring evaluation
if (strategy.flanking_weight > 0 && enemies is not None) {
    enemy_list = [e for e in enemies if e.is_alive()]
    if enemy_list:
        center_x = sum(e.pos.x for e in enemy_list) / len(enemy_list)
        center_y = sum(e.pos.y for e in enemy_list) / len(enemy_list)
        // ... use center for flanking calculation
}
```

### Problem

- **50 scoring evaluations per tick** × **10 operations per center** = **500 redundant calculations**
- Each calculation iterates through all enemy positions
- Enemy positions are **fixed within a tick** (no movement during target scoring phase)

### Proposed Solution

**Step 1:** Add center points to `TickContext` struct

```cpp
// In TickContext struct definition
struct TickContext {
    // ... existing fields ...
    double player_center_x = 0.0;
    double player_center_y = 0.0;
    double enemy_center_x = 0.0;
    double enemy_center_y = 0.0;
};
```

**Step 2:** Compute centers once during `_prepare_tick_context`

```cpp
void TeamfightSimulationCore::_prepare_tick_context(TickContext &ctx) {
    // ... existing code ...
    
    // Compute team centers
    double player_cx = 0.0, player_cy = 0.0;
    double enemy_cx = 0.0, enemy_cy = 0.0;
    int player_count = 0, enemy_count = 0;
    
    for (const auto &unit : _units) {
        if (!unit.alive) continue;
        if (unit.team == sn_player()) {
            player_cx += unit.pos_x;
            player_cy += unit.pos_y;
            player_count++;
        } else {
            enemy_cx += unit.pos_x;
            enemy_cy += unit.pos_y;
            enemy_count++;
        }
    }
    
    if (player_count > 0) {
        ctx.player_center_x = player_cx / player_count;
        ctx.player_center_y = player_cy / player_count;
    }
    if (enemy_count > 0) {
        ctx.enemy_center_x = enemy_cx / enemy_count;
        ctx.enemy_center_y = enemy_cy / enemy_count;
    }
}
```

**Step 3:** Use precomputed centers in `_score_advanced_tactics`

```cpp
// In _score_advanced_tactics or flanking section
if (strategy.flanking_weight > 0) {
    double center_x = (enemy.team == sn_player()) ? ctx.player_center_x : ctx.enemy_center_x;
    double center_y = (enemy.team == sn_player()) ? ctx.player_center_y : ctx.enemy_center_y;
    
    // Use center_x, center_y directly without recomputation
}
```

### Expected Impact

- **Eliminates ~500 center calculations per tick**
- **Saves ~5-10% of targeting time** (~650 ns per tick)
- **Zero behavior change** — same calculation, just happens earlier

### Validation

- ✅ Run `--fixture-file` to verify golden parity
- ✅ Benchmark before/after on same machine
- ✅ No edge cases (dead units handled in filtering)

---

## Optimization #2: Precompute Bodyguard & Obscurance Relationships (MEDIUM PRIORITY)

### Current Implementation

**File:** `native/src/teamfight_simulation_core.cpp`  
**Location:** `_score_enemy_target` lines 1785-1827 (bodyguard), Python `targeting.py` lines 645-655

```cpp
// Current: For EVERY candidate enemy, iterate all carries
for (int64_t ally_index : carry_indices) {
    const UnitState &ally = _units[ally_index];
    if (!ally.alive) continue;
    double adx = ally.pos_x - enemy.pos_x;
    double ady = ally.pos_y - enemy.pos_y;
    double ally_dist_sq = adx * adx + ady * ady;
    if (ally_dist_sq < bodyguard_r2) {
        double guard_bonus = (1.0 - (ally_dist / BODYGUARD_RADIUS)) * bodyguard_weight;
        score -= guard_bonus;  // Lower is better, so negative bonus is good
    }
}
```

### Problem

- In a 5v5 with 2-3 carries and bodyguard_weight > 0 (tanks, support):
  - **5 scorers × 5 targets × 3 carries = 75 distance checks per tick**
  - Each check: 2 subtractions, 1 multiply, 1 add, 1 sqrt, 1 scale
  - **Same carry relationships checked repeatedly for different enemies**

### Proposed Solution

Create a **threat matrix** precomputed once per tick:

**Step 1:** Add threat matrix to `TickContext`

```cpp
// In TickContext struct
struct TickContext {
    // ... existing fields ...
    // For each enemy unit (by index), store which carries are nearby
    std::vector<int64_t> carries_near_player_targets;  // threat for player targets
    std::vector<int64_t> carries_near_enemy_targets;   // threat for enemy targets
};
```

**Step 2:** Compute threat matrix in `_prepare_tick_context`

```cpp
void TeamfightSimulationCore::_prepare_tick_context(TickContext &ctx) {
    // ... existing code ...
    
    const double bodyguard_r2 = BODYGUARD_RADIUS * BODYGUARD_RADIUS;
    
    // For each enemy, find which carries are nearby
    for (int64_t enemy_idx : ctx.enemy_indices) {
        const UnitState &enemy = _units[enemy_idx];
        if (!enemy.alive) continue;
        
        ctx.carries_near_enemy_targets.clear();
        for (int64_t carry_idx : ctx.player_carry_indices) {
            const UnitState &carry = _units[carry_idx];
            if (!carry.alive) continue;
            
            double cdx = carry.pos_x - enemy.pos_x;
            double cdy = carry.pos_y - enemy.pos_y;
            double dist_sq = cdx * cdx + cdy * cdy;
            
            if (dist_sq < bodyguard_r2) {
                ctx.carries_near_enemy_targets.push_back(carry_idx);
            }
        }
        // Store which carries are near this enemy
        ctx.enemy_target_nearby_carries[enemy_idx] = ctx.carries_near_enemy_targets;
    }
}
```

**Step 3:** Use precomputed matrix in scoring

```cpp
// In _score_enemy_target
if (bodyguard_weight > 0.0) {
    const auto &nearby_carries = ctx.enemy_target_nearby_carries[enemy_idx];
    for (int64_t carry_idx : nearby_carries) {
        const UnitState &ally = _units[carry_idx];
        double adx = ally.pos_x - enemy.pos_x;
        double ady = ally.pos_y - enemy.pos_y;
        double ally_dist_sq = adx * adx + ady * ady;
        
        double ally_dist = Math::sqrt(ally_dist_sq);
        double guard_bonus = (1.0 - (ally_dist / BODYGUARD_RADIUS)) * bodyguard_weight;
        score -= guard_bonus;
    }
}
```

### Expected Impact

- **Reduces carry iteration from 3 per target to ~0.5 average**
- **Saves ~8-12% of targeting time** (~1.2 µs per tick)
- **Trade-off:** Additional 200-300 ns during prepare phase (well worth it)

### Validation

- ⚠️ More complex precomputation; validate matrix against direct calculation
- ✅ Run `--fixture-file --debug-fixture-name=<name>` on specific case
- ✅ Spot-check bodyguard behavior in trace logs

---

## Optimization #3: Early Exit on Stickiness (LOW-MEDIUM PRIORITY)

### Current Implementation

**File:** `native/src/teamfight_simulation_core.cpp`  
**Location:** Lines 2640-2668 (retarget logic)

```cpp
// Current: Evaluate ALL candidates before deciding to switch
for (int64_t enemy_index : enemy_indices) {
    UnitState &candidate = _units[enemy_index];
    double dist = _distance_between(unit, candidate);
    double raw = _score_enemy_target(unit, candidate, strategy, ctx, dist, profile_sim);
    // ... compute adjusted score, compare to best ...
}
// After evaluating all:
if (!_should_switch(unit, current_score, best_raw, strategy)) {
    return current_target;  // Was worth staying
}
```

### Problem

- Units with **high stickiness** will rarely switch
- But we still evaluate **all 5 candidates** to find if there's a better one
- If current target is already "good enough," we waste 4 more score calculations

### Proposed Solution

**Score current target first, early exit if sticky:**

```cpp
// Step 1: Score current target immediately
double current_dist = _distance_between(unit, *current_target);
double current_score = _score_enemy_target(unit, *current_target, strategy, ctx, current_dist, profile_sim);

// Step 2: If current target is "locked in" by stickiness, skip evaluation
double stickiness_threshold = current_score - (Math::max(1.0, strategy.distance_weight) * strategy.stickiness_bonus);
bool can_switch = false;

// Step 3: Only evaluate other candidates if switching is possible
for (int64_t enemy_index : enemy_indices) {
    if (enemy_index == current_target->instance_id) continue;  // Skip current
    
    UnitState &candidate = _units[enemy_index];
    double dist = _distance_between(unit, candidate);
    double raw = _score_enemy_target(unit, candidate, strategy, ctx, dist, profile_sim);
    
    // Check if we'd actually switch to this candidate
    if (raw < stickiness_threshold - switch_margin) {
        can_switch = true;
        // ... proceed with full candidate comparison ...
        break;  // Can exit early once we find ONE candidate worth switching to
    }
}

if (!can_switch) {
    return current_target;  // No better option found
}
```

### Expected Impact

- **3-8% improvement** for sticky units (particularly useful for tanks, supports)
- **Only effective** if stickiness_bonus is high or distance_weight is low
- **No behavior change** — same decision, just decided earlier

### Risk

- ⚠️ Requires careful implementation to avoid off-by-one errors
- ⚠️ Must match original decision exactly (validate with fixtures)
- ✅ Low risk if validated properly

---

## Optimization #4: Lazy Obscurance Calculation

### Current Implementation

**File:** `Python/targeting.py:657-690`

```python
# Current: Check obscurance for EVERY candidate
if strategy.obscurance_weight > 0 and enemies is not None:
    blockage_count = 0
    for other_enemy in enemies:
        if other_enemy.instance_id == enemy.instance_id: continue
        if other_enemy.stats.role in ("tank", "fighter"):
            if other_enemy.pos.distance_to(unit.pos) >= dist:
                continue
            line_dist_sq = self._point_segment_distance_sq(
                other_enemy.pos, unit.pos, enemy.pos
            )
            if line_dist_sq <= data.OBSCURANCE_LINE_RADIUS ** 2:
                blockage_count += 1
    score += blockage_count * strategy.obscurance_weight
```

### Problem

- For each target, iterate all other enemies to check line-of-sight
- In 5v5: **5 scorers × 5 targets × 5 checks = 125 geometry calculations**
- Many of these are **redundant** (same attacker-blocker pairs checked multiple times)

### Proposed Solution

**Precompute obscurance map:**

```python
# In TickContext / prepare phase
obscurance_map = {}  # (attacker_id, blocker_id) -> bool

for attacker in alive_units:
    for blocker in frontline_units:
        if blocker.instance_id == attacker.instance_id:
            continue
        # For each target, check if blocker is "between" attacker and target
        # This can be precomputed once per tick
        obscurance_map[(attacker.instance_id, blocker.instance_id)] = \
            blocker.pos.distance_to(attacker.pos) < some_threshold
```

Then in scoring:

```python
if strategy.obscurance_weight > 0:
    blockage_count = 0
    for blocker_id in precomputed_blocker_list:
        if (unit.instance_id, blocker_id) in obscurance_map:
            blockage_count += 1
    score += blockage_count * strategy.obscurance_weight
```

### Expected Impact

- **2-5% improvement** (mostly benefit for marksman/mage heavy comps)
- **More complex** than other optimizations
- **Higher risk** if not validated properly

---

## Optimization #5: Conditional Role List Updates

### Current Implementation

**File:** `native/src/teamfight_simulation_core.cpp:_prepare_tick_context`

```cpp
// Current: Regenerate EVERY tick
ctx.player_carry_indices.clear();
ctx.enemy_carry_indices.clear();
// ... iterate and categorize ALL units ...
```

### Problem

- Role-based lists only change when:
  - A unit dies (removed from appropriate list)
  - A unit spawns (added to appropriate list)
  - A unit's role changes (rare/never in core gameplay)

- But we regenerate them **every single tick** even if nothing changed

### Proposed Solution

**Lazy update with dirty flag:**

```cpp
struct TickContext {
    bool role_lists_dirty = true;  // Set to true only on death/spawn
};

void TeamfightSimulationCore::_prepare_tick_context(TickContext &ctx) {
    if (!ctx.role_lists_dirty && ctx.last_death_tick == _current_tick - 1) {
        return;  // Skip regeneration
    }
    
    // ... regenerate lists ...
    ctx.role_lists_dirty = false;
}
```

### Expected Impact

- **1-3% improvement** (mainly noticeable in 1v1 and early game)
- **Low complexity**
- **Medium risk** if not validated (must ensure lists stay in sync)

---

## Optimization #6: Squared Distance for Spatial Checks

### Current Implementation

**Files:** Multiple locations

```cpp
// Current: Full sqrt for every distance check
double dist = Math::sqrt(dx * dx + dy * dy);
if (dist <= BODYGUARD_RADIUS) {  // then use squared
    double guard_bonus = (1.0 - (dist / BODYGUARD_RADIUS)) * weight;
}
```

### Problem

- `sqrt` is expensive (~20-50 cycles)
- Many checks use **squared distances first** for comparison, then compute `sqrt` only if needed

### Proposed Solution

**Use squared distances for all threshold checks:**

```cpp
// Optimized
double dist_sq = dx * dx + dy * dy;
if (dist_sq <= BODYGUARD_RADIUS_SQ) {  // Compare squared
    double dist = Math::sqrt(dist_sq);  // Only compute sqrt when needed
    double guard_bonus = (1.0 - (dist / BODYGUARD_RADIUS)) * weight;
}
```

### Expected Impact

- **1-2% improvement** (already partially done in code)
- **Very low risk** (widespread best practice)
- **Zero behavior change**

---

## Implementation Priority & Effort Estimate

| Optimization | Priority | Effort | Risk | Est. Impact | Cumulative |
|--------------|----------|--------|------|------------|-----------|
| 1. Memoize team center | 🔴 High | 30 min | Low | 5-10% | 5-10% |
| 2. Bodyguard matrix | 🟠 Medium | 2 hrs | Low | 8-12% | 13-20% |
| 3. Early exit stickiness | 🟠 Medium | 1 hr | Medium | 3-8% | 16-26% |
| 4. Lazy obscurance | 🟡 Low | 2 hrs | Medium | 2-5% | 18-30% |
| 5. Conditional role lists | 🟡 Low | 1 hr | Medium | 1-3% | 19-32% |
| 6. Squared distance checks | 🟡 Low | 30 min | Low | 1-2% | 20-33% |

---

## Testing & Validation Checklist

Before committing any optimization:

- [ ] **Run `--check-only`** — no syntax errors after changes
- [ ] **Golden parity:** `--fixture-file=res://fixtures/goldens/match_fixtures.json`
- [ ] **Benchmark baseline:** `--check-benchmark --batch-count=2000 --team-size=5 --bench-skip-summaries --workers=1`
  - [ ] Run 3× and take median
  - [ ] Compare vs known baseline on same machine
- [ ] **Optional:** `--sim-profile` to verify targeting time actually decreased
- [ ] **Code review:** Another pair of eyes on logic changes
- [ ] **Documentation:** Update comments explaining the optimization

---

## References

- **Current baseline:** 290 m/s (5v5, 2000 matches, Release build, single worker)
- **Profile data:** Aggregated from 500 matches with `--sim-profile` enabled
- **Golden fixtures:** 22 test cases in `fixtures/goldens/match_fixtures.json`
- **Measurement gate:** `benchmark_rundown.md` section 3

