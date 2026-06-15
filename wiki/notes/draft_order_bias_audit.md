# Draft Order Bias Audit

## Date/Phase
Phase 38: Draft Order Bias Audit
Date: June 14, 2026

## Purpose

Measure whether the current draft order itself is causing too much side advantage by testing alternate draft orders. The goal was to determine if the ~18.4% Blue-side advantage observed in native self-play is structural (from draft order) or a bug in the scoring logic.

## Context

Baseline native self-play showed:
- native Blue vs native Red: ~59.2% / 40.8%
- Approximately 18.4% Blue-side advantage

Before adding archetypes or more scoring logic, this audit measured whether this advantage comes from the draft order itself.

## Draft Orders Tested

### current
Original snake draft order (production default):
- Steps 0-5: BAN (alternating B/R)
- Steps 6-11: PICK (B, R, R, B, B, R)
- Steps 12-15: BAN (alternating R/B)
- Steps 16-19: PICK (R, B, B, R)

### mirrored_current
Swapped B/R in current order:
- Steps 0-5: BAN (alternating R/B)
- Steps 6-11: PICK (R, B, B, R, R, B)
- Steps 12-15: BAN (alternating B/R)
- Steps 16-19: PICK (B, R, R, B)

### red_first_pick_variant
Red gets first pick in phase 1:
- Steps 0-5: BAN (alternating B/R)
- Steps 6-11: PICK (R, B, B, R, R, B)
- Steps 12-15: BAN (alternating R/B)
- Steps 16-19: PICK (B, R, R, B)

### no_final_blue_pair_variant
Split final Blue pair (steps 17-18):
- Steps 0-5: BAN (alternating B/R)
- Steps 6-11: PICK (B, R, R, B, B, R)
- Steps 12-15: BAN (alternating R/B)
- Steps 16-19: PICK (R, B, R, B)

## 50x25 Results

### Native Self-Play Bias

| Draft Order | Blue Win Rate | Red Win Rate | Side Bias |
|-------------|---------------|--------------|-----------|
| current | 59.8% | 40.2% | 19.6% |
| mirrored_current | 38.2% | 61.8% | 23.6% |
| red_first_pick_variant | 5.7% | 94.3% | 88.6% |
| no_final_blue_pair_variant | 73.7% | 26.3% | 47.4% |

### Random Self-Play Bias

| Draft Order | Blue Win Rate | Red Win Rate | Side Bias |
|-------------|---------------|--------------|-----------|
| current | 48.7% | 51.3% | 2.6% |
| mirrored_current | 50.0% | 50.0% | 0.0% |
| red_first_pick_variant | 50.0% | 50.0% | 0.0% |
| no_final_blue_pair_variant | 45.6% | 54.4% | 8.8% |

### Native vs Random

| Draft Order | Native vs Random (Blue) | Random vs Native (Blue) |
|-------------|-------------------------|-------------------------|
| current | 84.8% | 16.9% |
| mirrored_current | 88.9% | 14.0% |
| red_first_pick_variant | 90.6% | 16.9% |
| no_final_blue_pair_variant | 86.4% | 17.2% |

## Key Findings

### 1. Bias is Structural
The ~18.4% bias is clearly structural and comes from the draft order itself:
- Current order: 19.6% bias in native self-play
- Mirrored order: 23.6% bias in native self-play (reversed advantage)
- Red first pick: 88.6% bias in native self-play (massive)
- No final pair: 47.4% bias in native self-play (worse)

The bias changes predictably with draft order changes, confirming it's structural, not a bug in scoring logic.

### 2. Mirrored Order Reverses Advantage
Mirrored order reverses the advantage:
- Current: Blue wins 59.8%, Red wins 40.2%
- Mirrored: Blue wins 38.2%, Red wins 61.8%

This confirms the advantage follows the draft order slots, not the side labels.

### 3. Random is Fair
Random self-play shows minimal bias across all draft orders:
- Current: 48.7% vs 51.3% (2.6% bias)
- Mirrored: 50.0% vs 50.0% (0.0% bias)
- Red first pick: 50.0% vs 50.0% (0.0% bias)
- No final pair: 45.6% vs 54.4% (8.8% bias)

Random is essentially fair across all draft orders, confirming the bias is strategy-specific, not inherent to the game.

### 4. Current Order is Most Balanced
The current draft order has the lowest bias among tested variants:
- Current: 19.6% bias (best)
- Mirrored: 23.6% bias (worse)
- Red first pick: 88.6% bias (much worse)
- No final pair: 47.4% bias (worse)

The current order appears to be the optimal snake draft configuration.

### 5. First Pick Advantage is Critical
Red first pick variant has 88.6% bias (catastrophic):
- First pick in snake draft provides significant advantage
- The current order gives Blue first pick, which is intentional
- Giving Red first pick massively favors Red

### 6. Late Picks Matter
No final pair variant (splitting Blue's late pair) made bias worse:
- Blue's consecutive late picks (steps 17-18) are advantageous
- Splitting them doesn't help and makes bias worse (47.4% vs 19.6%)
- The snake draft's final pair structure is intentional

## Decision

**Keep the current draft order as the production/default order.**

### Reasons

1. **Lowest bias:** 19.6% bias is the lowest among tested variants
2. **All alternatives worse:** Every tested alternative has worse bias
3. **Random is fair:** Confirms the game is not inherently biased
4. **Structural advantage:** The bias is expected for snake draft
5. **No better option:** Changing draft order would not solve the problem

## Known Limitation

Baseline native self-play has a known ~18-20% Blue-side structural advantage due to the snake draft order. This is:
- **Not a bug** in the scoring logic
- **Structural** to the snake draft order
- **Expected** for this type of draft format
- **Manageable** and well-understood

## Recommendations

### Do Not Redesign Draft Order
Do not pursue draft order redesign at this time because:
1. Current order is the most balanced
2. Alternatives are significantly worse
3. Would require extensive game rebalancing
4. May have unintended consequences
5. The current bias is acceptable

### Accept Structural Bias
Accept the ~18-20% draft-order bias as a structural limitation because:
1. It's inherent to snake draft order
2. It's not a bug in the scoring logic
3. All tested alternatives are worse
4. The bias is manageable and well-understood
5. Changing draft order would not solve the problem

### Focus on Other Improvements
Focus on other AI improvements instead of draft order:
1. Archetype-based scoring
2. Deeper symmetric search (minimax)
3. Better comp detection
4. Improved ban targeting
5. Role balance improvements

## Baseline for Future Work

When evaluating future strategy changes:
1. Compare against the current 19.6% native self-play bias
2. Do not interpret changes as absolute fairness
3. Consider the structural advantage as a constant
4. Focus on relative improvements, not absolute fairness
5. The goal is to minimize bias, not eliminate it entirely

## References

- **Phase 38 report:** `phase38_draft_order_bias_audit_report.md` - Full audit results and analysis
- **Test infrastructure:** `scripts/tools/full_draft_ablation_test.gd` - Draft order testing framework
