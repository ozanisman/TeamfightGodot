# Draft Order Bias Audit

> **Historical snapshot — Phase 38 (June 2026).** Production decision and current metrics: [native_draft_ai.md](native_draft_ai.md).
>
> **UPDATE 2026-07-01 — the "structural constant" conclusion below is incomplete, see
> "Stats-Sensitivity Finding" section near the end.** The bias reversed sign and grew in magnitude
> after a stats refresh, and diverges by selection policy (deterministic `native_full` vs. the
> actually-shipped stochastic `native_softmax`) even with the exact same, unchanged
> `DRAFT_SEQUENCE`. Order still matters (confirmed again below), but it is not the whole story —
> do not treat any single bias percentage as a fixed constant.

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

- **Phase 38 report:** `res://logs/full_draft_ablation_report.txt` from `full_draft_ablation_test.gd` (generated on run, not committed)
- **Test infrastructure:** `scripts/tools/full_draft_ablation_test.gd` - Draft order testing framework

## Stats-Sensitivity Finding (2026-07-01)

While investigating why `native_draft_ai.md`'s baseline had drifted (see its "Stats Refresh
(2026-07-01)" section), the native self-play side bias was found to have reversed sign entirely
(Blue-favored → Red-favored) versus the ~19.6% figure this document treats as a structural
constant — **without any change to `DRAFT_SEQUENCE`**.

**Verified `DRAFT_SEQUENCE` is unchanged:** `scripts/simulation/sim_constants.gd` byte-matches the
"current" order described above (bans B/R/B/R/B/R, picks B/R/R/B/B/R, bans R/B/R/B, picks
R/B/B/R). This rules out a sequence/labeling bug as the explanation.

**New measurements (n=2500/matchup, `native_draft_validation_harness.gd`, fresh stats per
`native_draft_ai.md`'s stats refresh):**

| Policy | Blue | Red | Bias |
|---|---|---|---|
| `native_full` (deterministic top-1) | 35.7% | 64.3% | 28.6pp **Red**-favored |
| `native_softmax` (actual shipped gameplay policy, temp=0.5/top-5) | 55.8% | 44.2% | 11.6pp **Blue**-favored |

The two policies — reading identical stats, using the identical unchanged draft order — produce
**opposite-signed** bias. This falsifies the "bias is purely structural/order-driven" conclusion
in this document (Key Finding #1/#2 above): if it were purely a function of draft-order slot
value, both policies should show the same sign, since neither one changes which side occupies
which slot. Instead, the deterministic policy appears to converge on the same "objectively best"
pick chain from both sides until forced to diverge, and which side that chain favors depends on
which champions the current stats snapshot rates as strongest — i.e. the "structural" bias
documented here was actually **entangled with the specific stats snapshot in effect at the time**,
not a pure property of the draft order.

**Revised guidance (supersedes "Baseline for Future Work" above):**
1. Order still has *a* real, reproducible effect (the original current/mirrored/red-first/no-final-pair
   comparisons above are still valid evidence of that, at the stats snapshot they were measured on).
2. But no single bias percentage — 19.6%, 28.6%, or 11.6% — should be treated as a fixed constant.
   It must be re-measured whenever stats are regenerated, and separately for whichever policy is
   actually shipped (`native_softmax`), not just the deterministic validation policy (`native_full`).
3. The `native_softmax` number (11.6pp) is the one that matters for actual player-facing fairness
   today; it is smaller than the historically "accepted" 19.6% but opposite in sign, so past
   statements like "Blue has the structural advantage" are currently backwards.
4. This is a concrete instance of `draft_ai_improvement_plan.md` Guiding Principle #1 ("measure the
   policy you ship") and strengthens the case for that plan's Workstream E (self-play Elo ladder /
   continuous regression gate) — a one-time audit is not sufficient for a quantity this volatile.
5. Re-running the original current/mirrored/red-first/no-final-pair order comparisons against
   current stats and both policies was done next; results are below.

## Order Comparison Re-Run (2026-07-01)

After the stats refresh, the original four draft-order variants were re-tested with the current
`model_stats/stats_output_100k/` snapshot and **both** selection policies
(`native_full` deterministic top-1 and `native_softmax` shipped stochastic policy),
using `full_draft_ablation_test.gd` with `--include-native-softmax`.

**Method:** 100 trials × 25 sims/draft per matchup (n=2500/matchup), current production stats,
no weight overrides. Report files: `logs/full_draft_ablation_{current,mirrored,red_first,no_final_pair}_100x25.txt`.

### `native_full` Self-Play Bias by Order

| Draft Order | Blue Win Rate | Red Win Rate | Side Bias |
|-------------|---------------|--------------|-----------|
| current | 35.8% | 64.2% | 28.4pp **Red**-favored |
| mirrored_current | 36.8% | 63.2% | 26.4pp **Red**-favored |
| red_first_pick_variant | 30.6% | 69.4% | 38.8pp **Red**-favored |
| no_final_blue_pair_variant | 46.2% | 53.8% | 7.6pp **Red**-favored |

### `native_softmax` Self-Play Bias by Order

| Draft Order | Blue Win Rate | Red Win Rate | Side Bias |
|-------------|---------------|--------------|-----------|
| current | 54.8% | 45.2% | 9.6pp **Blue**-favored |
| mirrored_current | 47.1% | 52.9% | 5.8pp **Red**-favored |
| red_first_pick_variant | 50.8% | 49.2% | 1.6pp **Blue**-favored |
| no_final_blue_pair_variant | 51.9% | 48.1% | 3.8pp **Blue**-favored |

### Random Self-Play Bias by Order

| Draft Order | Blue Win Rate | Red Win Rate | Side Bias |
|-------------|---------------|--------------|-----------|
| current | 51.2% | 48.8% | 2.4pp Blue |
| mirrored_current | 50.5% | 49.5% | 1.0pp Blue |
| red_first_pick_variant | 50.5% | 49.5% | 1.0pp Blue |
| no_final_blue_pair_variant | 48.6% | 51.4% | 2.8pp Red |

### Key Takeaways from the Re-Run

1. **The deterministic policy is not order-robust.** With current stats, `native_full` is strongly
   Red-favored for `current`, `mirrored_current`, and `red_first_pick_variant`, and only modestly
   Red-favored for `no_final_blue_pair_variant`. The original Phase 38 conclusion that `current` is
   the most balanced order is **no longer true** for `native_full` under the refreshed stats.
2. **The shipped stochastic policy is far more balanced.** `native_softmax` self-play biases are
   single-digit across all four orders, with `red_first_pick_variant` nearly dead-even (1.6pp). This
   confirms that the actual player-facing behavior is much less order-sensitive than the deterministic
   validation metric suggested.
3. **Order effects are real but smaller than the stats/policy interaction.** Random self-play stays
   within ~3pp for all orders, so the order itself is not inherently broken. The large native_full bias
   swings come from which deterministic pick chain the current stats snapshot favors, not from a
   fixed slot advantage.
4. **`no_final_blue_pair_variant` is now the most balanced order for the deterministic policy.** Under
   current stats it cuts the `native_full` Red bias from ~28pp to ~8pp. Under `native_softmax` it is
   also balanced (3.8pp Blue). This is the opposite of the original Phase 38 finding, again showing
   the result is stats-dependent.

### Revised Recommendation

- Do **not** treat any draft-order bias number as a fixed structural constant. It must be re-measured
  after every stats regeneration and separately for the shipped policy (`native_softmax`).
- The shipped `native_softmax` policy is already reasonably balanced across all four orders; the
  immediate priority is **not** a draft-order redesign but finishing the statistical A/B regression
  gate (Workstream E) so future changes are measured against the actual shipped policy.
- If a future bias reduction pass is needed, `no_final_blue_pair_variant` is the most promising order
  candidate under current stats, but it should be validated at larger N and with both policies before
  any production change, because the optimal order has already flipped once across stats snapshots.

