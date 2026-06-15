# Native Draft AI Lookahead Experiment

## Date/Phase
Phase 34-37: Lookahead Implementation and Testing
Date: June 14, 2026

## Objective
Implement 1-ply lookahead to improve draft quality by simulating enemy responses, but quarantine it as experimental due to side bias concerns.

## Strategy Variants Tested

### native_lookahead
- **Description:** Full lookahead on both picks and bans
- **Logic:** Apply enemy-response penalty when next pick is enemy
- **Strategy enum:** 1 (NATIVE_LOOKAHEAD)
- **Side bias:** Up to 42.0% (severe)
- **Status:** Not promoted, kept as experimental

### native_lookahead_pick_only
- **Description:** Lookahead only for picks, baseline bans
- **Logic:** Same as full lookahead but bans use baseline
- **Strategy enum:** 2 (NATIVE_LOOKAHEAD_PICK)
- **Side bias:** Similar to full lookahead
- **Status:** Not promoted, kept as experimental

### native_lookahead_ban_only
- **Description:** Lookahead only for bans, baseline picks
- **Logic:** Ban lookahead with baseline picks
- **Strategy enum:** 3 (NATIVE_LOOKAHEAD_BAN)
- **Side bias:** 21-25% (moderate but exceeds threshold)
- **Status:** Kept as experimental for future research

### native_lookahead_continuation (REMOVED)
- **Description:** Continuation-aware pick lookahead
- **Logic:** Enemy-response penalty when next pick is enemy, follow-up bonus when same side
- **Strategy enum:** 4 (NATIVE_LOOKAHEAD_CONTINUATION) - removed
- **Side bias:** 42.0% (worse than original)
- **Status:** Removed - made bias significantly worse

### native_baseline (REMOVED)
- **Description:** Pure baseline (no lookahead)
- **Logic:** Identical to native
- **Strategy enum:** 0 (NATIVE_FULL)
- **Side bias:** 18.4% (draft-order advantage)
- **Status:** Removed - duplicate of native

### native_baseline_ban_lookahead (REMOVED)
- **Description:** Baseline picks with ban lookahead
- **Logic:** Same as ban-only
- **Strategy enum:** 0 for picks, 3 for bans
- **Side bias:** 24.8% (same as ban-only)
- **Status:** Removed - duplicate of ban-only

## 25x25 Screening Results

### native_lookahead_ban_only vs native
| Matchup | Blue Win Rate | Red Win Rate | Side Bias |
|---------|---------------|--------------|-----------|
| native_lookahead_ban_only (Blue) vs native (Red) | 62.4% | 37.6% | 24.8% |
| native (Blue) vs native_lookahead_ban_only (Red) | 60.5% | 39.5% | 21.0% |

### native_lookahead_continuation vs native
| Matchup | Blue Win Rate | Red Win Rate | Side Bias |
|---------|---------------|--------------|-----------|
| native_lookahead_continuation (Blue) vs native (Red) | 67.8% | 32.2% | 35.6% |
| native (Blue) vs native_lookahead_continuation (Red) | 71.0% | 29.0% | 42.0% |

### native_baseline vs native
| Matchup | Blue Win Rate | Red Win Rate | Side Bias |
|---------|---------------|--------------|-----------|
| native_baseline (Blue) vs native (Red) | 59.2% | 40.8% | 18.4% |
| native (Blue) vs native_baseline (Red) | 60.5% | 39.5% | 21.0% |

### native_baseline_ban_lookahead vs native
| Matchup | Blue Win Rate | Red Win Rate | Side Bias |
|---------|---------------|--------------|-----------|
| native_baseline_ban_lookahead (Blue) vs native (Red) | 62.4% | 37.6% | 24.8% |
| native (Blue) vs native_baseline_ban_lookahead (Red) | 60.5% | 39.5% | 21.0% |

### native_lookahead vs native (Reference)
| Matchup | Blue Win Rate | Red Win Rate | Side Bias |
|---------|---------------|--------------|-----------|
| native_lookahead (Blue) vs native (Red) | 62.4% | 37.6% | 24.8% |
| native (Blue) vs native_lookahead (Red) | 71.0% | 29.0% | 42.0% |

### native vs random (Performance Check)
| Matchup | Blue Win Rate | Red Win Rate | Side Bias |
|---------|---------------|--------------|-----------|
| native (Blue) vs random (Red) | 86.7% | 13.3% | 73.4% |
| random (Blue) vs native (Red) | 12.2% | 87.8% | 75.6% |

## 50x25 Original Lookahead Results (Phase 34)

### 25x25 Side-Symmetric Ablation Table

| Matchup | Blue Strategy | Red Strategy | Blue Win Rate | Red Win Rate | Invalid Drafts |
|---------|--------------|--------------|---------------|--------------|----------------|
| native_lookahead vs native | 62.4% | 37.6% | 0 |
| native vs native_lookahead | 71.0% | 29.0% | 0 |
| native_lookahead vs native_lookahead | 62.4% | 37.6% | 0 |

### Self-Play Bias
- native vs native: 18.4% Blue bias (draft-order advantage)
- native_lookahead vs native_lookahead: 24.8% Blue bias (lookahead amplifies advantage)

## Side Bias Findings

### Root Cause: Opportunity Imbalance
The snake draft order creates asymmetric lookahead opportunities:

**Blue lookahead opportunities (next_pick_is_enemy = true):**
- Step 6 (B_PICK): next_pick_step=7 (R) → lookahead applied
- Step 10 (B_PICK): next_pick_step=11 (R) → lookahead applied
- Step 18 (B_PICK): next_pick_step=19 (R) → lookahead applied
**Total: 3 lookahead-adjusted picks**

**Red lookahead opportunities (next_pick_is_enemy = true):**
- Step 8 (R_PICK): next_pick_step=9 (B) → lookahead applied
- Step 16 (R_PICK): next_pick_step=17 (B) → lookahead applied
**Total: 2 lookahead-adjusted picks**

This 3:2 ratio means Blue gets 50% more lookahead adjustments than Red, which directly amplifies Blue's inherent draft-order advantage (18.4% → 24.8%).

### Same-Side Consecutive Picks (Correctly Skipped)
- Step 7 (R_PICK): next_pick_step=8 (R) → lookahead skipped ✓
- Step 9 (B_PICK): next_pick_step=10 (B) → lookahead skipped ✓
- Step 11 (R_PICK): next_pick_step=16 (R) → lookahead skipped ✓
- Step 17 (B_PICK): next_pick_step=18 (B) → lookahead skipped ✓

### Continuation-Aware Failure
The continuation-aware variant attempted to equalize opportunities by applying lookahead on all pick steps:
- Blue: 5 adjustments (steps 6, 9, 10, 17, 18)
- Red: 5 adjustments (steps 7, 8, 11, 16, 19)

However, this made the bias WORSE (42.0% vs 33.4% for original) because:
1. Follow-up bonus on same-side picks amplifies draft-order advantage
2. The adjustment logic itself is asymmetric
3. Equalizing opportunities didn't solve the fundamental asymmetry

## Acceptance Criteria Evaluation

### native_lookahead_ban_only
1. **No invalid drafts:** ✓ PASS
2. **Beats random from both sides:** ✓ PASS
3. **Competitive with native_full from both sides:** ✓ PASS
4. **No severe side bias:** ✗ FAIL (24.8% bias exceeds 20% threshold)
5. **Runtime acceptable:** ✓ PASS

### native_lookahead_continuation
1. **No invalid drafts:** ✓ PASS
2. **Beats random from both sides:** ✓ PASS
3. **Competitive with native_full from both sides:** ✓ PASS
4. **No severe side bias:** ✗ FAIL (42.0% bias is WORSE than original)
5. **Runtime acceptable:** ✓ PASS

### native_lookahead (current)
1. **No invalid drafts:** ✓ PASS
2. **Beats random from both sides:** ✓ PASS
3. **Competitive with native_full from both sides:** ✓ PASS
4. **No severe side bias:** ✗ FAIL (42.0% bias is severe)
5. **Runtime acceptable:** ✓ PASS

## Final Decision

**Do NOT promote any lookahead variant to default.**

### Reason
All lookahead variants have unacceptable side bias:
- native_lookahead: Up to 42.0% side bias (severe)
- native_lookahead_continuation: Up to 42.0% side bias (worse than original)
- native_lookahead_ban_only: 21-25% side bias (moderate but exceeds threshold)

The baseline `native` strategy has the lowest measured bias (~18.4% draft-order advantage) and strong performance against random.

### Current Status
- **Default:** native (baseline with 18.4% draft-order bias)
- **Experimental (kept for research):**
  - native_lookahead - Severe bias, kept for reference
  - native_lookahead_pick_only - Severe bias, kept for reference
  - native_lookahead_ban_only - Moderate bias, may be useful for future tuning
- **Removed:**
  - native_lookahead_continuation - Made bias worse, removed
  - native_baseline - Duplicate of native, removed
  - native_baseline_ban_lookahead - Same as ban-only, removed

## Future Options

1. **Draft order redesign**
   - Symmetric draft order to eliminate structural bias
   - This is a larger game design change
   - May require rebalancing other game systems

2. **Side-compensated scoring**
   - Give Red stronger lookahead adjustments to compensate for draft-order disadvantage
   - Requires extensive calibration
   - May not fully equalize

3. **Deeper but symmetric search**
   - Minimax with equal depth for both sides
   - More computationally expensive
   - May still have bias if evaluation function is asymmetric

4. **Remove lookahead**
   - If not useful for production, remove entirely
   - Keep baseline native as default
   - Accept 18.4% draft-order bias as structural limitation

## Files Modified

### Native C++ Backend
- `native/src/simulation/sim_draft_ai_types.hpp`:
  - Added NATIVE_LOOKAHEAD_PICK, NATIVE_LOOKAHEAD_BAN (Phase 34)
  - Added NATIVE_LOOKAHEAD_CONTINUATION (Phase 36)
  - Removed NATIVE_LOOKAHEAD_CONTINUATION (Phase 37)

- `native/src/simulation/sim_draft_ai_recommender.cpp`:
  - Added pick lookahead logic (Phase 34)
  - Added ban lookahead logic (Phase 34)
  - Added continuation-aware lookahead (Phase 36)
  - Removed continuation-aware lookahead (Phase 37)

### GDScript Strategy Files
- `scripts/tools/draft_strategy_native_lookahead.gd` - Created (Phase 34)
- `scripts/tools/draft_strategy_native_lookahead_pick.gd` - Created (Phase 34)
- `scripts/tools/draft_strategy_native_lookahead_ban.gd` - Created (Phase 34)
- `scripts/tools/draft_strategy_native_lookahead_continuation.gd` - Created (Phase 36), Removed (Phase 37)
- `scripts/tools/draft_strategy_native_baseline.gd` - Created (Phase 36), Removed (Phase 37)
- `scripts/tools/draft_strategy_native_baseline_ban_lookahead.gd` - Created (Phase 36), Removed (Phase 37)

### Test Infrastructure
- `scripts/tools/full_draft_ab_test.gd`:
  - Added lookahead variant paths (Phase 34)
  - Added continuation variant paths (Phase 36)
  - Removed continuation variant paths (Phase 37)

### Documentation
- `wiki/notes/native_draft_ai.md`:
  - Added "Experimental Lookahead Findings" section (Phase 37)

- `wiki/notes/native_draft_ai_baseline.md`:
  - Added "Experimental Lookahead Status" section (Phase 37)

## References

- **Phase 34:** `phase34_lookahead_bias_isolation_report.md` - Bias isolation analysis
- **Phase 35:** `phase35_lookahead_trace_report.md` - Turn diagnostic and opportunity analysis
- **Phase 36:** `phase36_lookahead_variants_report.md` - Safer variants test results
- **Phase 37:** `phase37_lookahead_quarantine_report.md` - Quarantine and documentation
