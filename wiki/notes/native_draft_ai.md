# Native Draft AI Checkpoint

## Purpose

The native draft AI provides C++-based pick and ban recommendations for the Teamfight autobattler. It replaces the previous GDScript-only prototype with a more performant and extensible scoring system that can handle complex champion interactions at scale.

## Files/Classes Involved

### Native C++ Core
- `native/src/simulation/sim_draft_ai_recommender.hpp` - Draft recommender class declaration
- `native/src/simulation/sim_draft_ai_recommender.cpp` - Pick/ban scoring logic
- `native/src/simulation/sim_draft_ai_stats_database.hpp/cpp` - Stats database for champion win rates
- `native/src/simulation/sim_draft_ai_evaluator.hpp/cpp` - Draft evaluation logic
- `native/src/simulation/sim_draft_ai_types.hpp` - Type definitions
- `native/src/teamfight_simulation_core.cpp` - Godot binding for draft recommendations

### GDScript Integration
- `scripts/simulation/native_simulation_backend.gd` - Godot-C++ bridge
- `scripts/tools/draft_strategy_native.gd` - Native strategy wrapper
- `scripts/tools/draft_strategy_native_picks_random_bans.gd` - Hybrid strategy (native picks, random bans)
- `scripts/tools/draft_strategy_random_picks_native_bans.gd` - Hybrid strategy (random picks, native bans)
- `scripts/tools/full_draft_ablation_test.gd` - Test runner for validation

## Data Inputs Loaded

The native draft AI loads statistics from the draft stats database:

- **Champion win rates** - Overall and per-role performance
- **Synergy scores** - Champion pair performance when on same team
- **Counter scores** - Champion pair performance when on opposing teams
- **Role combinations** - Pre-computed 5-unit comp win rates from `role_combinations.csv`
- **Champion metadata** - Roles, tags, and other static properties

Stats directory: `res://stats_output_100k/` (configurable per strategy)

## Pick Scoring Formula

Pick score is computed as a weighted sum of multiple factors:

```
pick_score = (own_pick_value * w1) + (enemy_synergy * w2) + (counters_enemy * w3) + (fills_role_need * w4) + (comp_fit * w5)
```

Where:
- `own_pick_value` - Win rate of this champion given current allies
- `enemy_synergy` - Synergy with enemy team (negative to avoid helping enemy)
- `counters_enemy` - How well this champion counters enemy picks
- `fills_role_need` - Importance of this role for team composition
- `comp_fit` - How well this pick fits into known strong comps

Weights are phase-specific (see below).

## Ban Scoring Formula

Ban score is computed as a weighted sum targeting enemy threats:

```
ban_score = (enemy_pick_value * w1) + (enemy_synergy * w2) + (counters_my_team * w3) + (fills_enemy_role_need * w4) + (enemy_comp_fit * w5) - (own_pick_value_penalty * w6)
```

Where:
- `enemy_pick_value` - Win rate of this champion for enemy team
- `enemy_synergy` - Synergy with enemy team (ban high-synergy picks)
- `counters_my_team` - How well this champion counters our team
- `fills_enemy_role_need` - Importance of this role for enemy composition
- `enemy_comp_fit` - How well this fits into enemy's likely comps
- `own_pick_value_penalty` - Penalty if we might want to pick this champion

Weights are phase-specific (see below).

## Phase-Aware Weights

Both pick and ban scoring use phase-specific weights that change based on draft step:

### Ban Weights by Phase

**Phase 1 Ban (steps 0-5):** Early game, limited information
- `denial_value_weight`: 0.75
- `enemy_synergy_weight`: 0.15
- `counters_my_team_weight`: 0.20
- `fills_enemy_role_need_weight`: 0.05
- `enemy_comp_fit_weight`: 0.04

**Phase 2 Ban (steps 12-15):** Late game, more information
- `denial_value_weight`: 0.50
- `enemy_synergy_weight`: 0.30
- `counters_my_team_weight`: 0.35
- `fills_enemy_role_need_weight`: 0.15
- `enemy_comp_fit_weight`: 0.10

### Pick Weights

Pick weights are constant across phases (simpler model, validated as effective).

## Side-Aware Ban Fallback Behavior

The ban system includes a side-aware early fallback mechanism:

**Early Fallback (steps 0-5):** When enemy information is limited:
- If `enemy_pick_value` is below threshold, apply fallback multiplier (default 0.30)
- This reduces the weight of enemy-specific signals early in draft
- Prevents over-committing to bans based on incomplete information

**Phase 2 (steps 12-15):** Full enemy-specific scoring
- No fallback applied
- Enemy-specific signals have full weight

**Side Awareness:**
- Blue side: Standard fallback behavior
- Red side: Adjusted fallback to account for Blue's first pick advantage
- Ensures ban scoring is not biased toward either side

## Role Fit and Comp Fit Behavior

### Role Fit
- Computed based on remaining role needs for the team
- Higher weight for roles that are scarce or critical
- Ensures balanced team composition

### Comp Fit
- Uses pre-computed `role_combinations.csv` with 5-unit comp win rates
- Scores picks based on how well they fit into known strong comps
- **Partial role composition scoring (Phase 29):**
  - For partial role sets (< 5 roles), scans all full 5-role comps that contain the partial role multiset
  - Aggregates wins/losses/samples from matching full comps
  - Applies existing Bayesian smoothing to aggregated stats
  - Exact 5-role comps use exact lookup (unchanged behavior)
  - Empty/unknown roles return neutral
  - Multiset matching: role counts matter (e.g., tank + tank only matches comps with 2+ tanks)

## GDScript Integration Points

### Strategy Interface
All draft strategies implement the same interface:

```gdscript
func recommend_next_pick(allies: Array, enemies: Array, available: Array, draft_step: int = -1) -> StringName
func recommend_next_ban(allies: Array, enemies: Array, available: Array, draft_step: int = -1, side: String = "blue", weight_overrides: Dictionary = {}) -> StringName
```

### Strategy Selection
The native strategy is opt-in and must be explicitly selected in test runners:

```gdscript
# In test runners (e.g., full_draft_ablation_test.gd)
var strategy = DraftStrategyNative.new("res://stats_output_100k")
```

Available strategies:
- `DraftStrategyNative` - Full native picks and bans
- `DraftStrategyNativeArchetype` - Experimental native picks and bans with small additive archetype scoring profiles
- `DraftStrategyNativePicksRandomBans` - Native picks, random bans
- `DraftStrategyRandomPicksNativeBans` - Random picks, native bans
- `DraftStrategyRandom` - Full random (baseline)
- `DraftStrategyCertified` - Old GDScript prototype (backward compatibility)

### Native Backend Call
```gdscript
var recommendations = _backend.get_draft_ai_ban_recommendations(
    stats_dir, available, allies, enemies, max_results, draft_step, side, weight_overrides
)
```

### Weight Overrides
Experimental weight profiles can be applied via:
```gdscript
strategy.set_weight_overrides({
    "enemy_synergy_multiplier": 1.25,
    "counters_my_team_multiplier": 1.25,
    "fills_enemy_role_need_multiplier": 1.25
})
```

## Validation Results Summary

### Baseline Performance (50 trials x 25 simulations)

**native_full vs random_full:**
- Blue native vs Red random: 88.4%
- Blue random vs Red native: 85.5%
- Average: 87.0%
- **Conclusion:** Strong native advantage, no significant side bias

**native_full vs native_picks_random_bans:**
- Blue native_full vs Red native_random: 60.0%
- Blue native_random vs Red native_full: 75.3%
- Average: 67.7%
- **Conclusion:** Bans provide meaningful advantage (~15-20% swing)

**random_picks_native_bans vs random_full:**
- Blue random_native vs Red random: 51.7%
- Blue random vs Red random_native: 50.3%
- Average: 51.0%
- **Conclusion:** Bans alone provide slight but measurable advantage

### Validation Metrics
- **Invalid drafts:** 0 across all matchups
- **Side bias:** No significant bias detected (<3% difference between sides)
- **Ban quality:** Bans target appropriate champions (high-value enemy picks, not own picks)
- **Stability:** Consistent results across multiple runs

### Weight Calibration Results

Tested multiple weight profiles to optimize ban scoring:

- **current (baseline):** Default phase-specific weights
- **enemy_specific_heavier:** 1.25x multiplier on enemy-specific signals
- **denial_heavier/lighter:** Variations on denial weight
- **fallback_heavier/lighter:** Variations on early fallback multiplier

**Result:** `enemy_specific_heavier` showed marginal improvements but failed selection rule (slight regression in bans-only contribution). Current baseline weights remain default.

## Current Default Behavior

- **Pick scoring:** Constant weights, validated as effective
- **Ban scoring:** Phase-specific weights with side-aware fallback
- **Archetype scoring:** Experimental and opt-in only through `native_archetype`
- **Weight overrides:** Opt-in only via `set_weight_overrides()`
- **Default profile:** Current baseline (no multipliers applied)
- **Fallback multiplier:** 0.30 (applied in early ban phase)

## Experimental Archetype Scoring (Phase 45)

`native_archetype` adds a small archetype term on top of baseline native scoring. The default `native` strategy does not use this term.

### Weights

- Pick archetype weight: `0.06`
- Ban archetype weight: `0.04`

### Pair Table

Each pair applies at most once per candidate:

| Pair | Raw score |
|------|-----------|
| frontline + backline | +0.030 |
| frontline + poke | +0.020 |
| frontline + protect | +0.015 |
| cc + aoe | +0.020 |
| control + poke | +0.020 |
| dive + mobility | +0.015 |
| dive + protect | +0.015 |
| sustain + frontline | +0.015 |
| protect + backline | +0.015 |

Pick scoring compares candidate tags with ally tags, applies small overstack penalties, clamps the raw score to `[-0.08, +0.08]`, then multiplies by `0.06`.

Ban scoring compares candidate tags with enemy tags, applies the same positive pair table plus a `+0.020` completion bonus when the enemy already has 2+ champions with one candidate major tag, clamps the raw score to `[0.00, +0.08]`, then multiplies by `0.04`.

Major tags for ban completion are:

`dive`, `poke`, `burst`, `sustain`, `cc`, `control`, `protect`, `aoe`

### Debug Output

Pick recommendations include:

- `candidate_tags`
- `archetype_score`
- `archetype_weight`
- `archetype_contribution`
- `archetype_reasons`

Ban recommendations include:

- `candidate_tags`
- `enemy_archetype_score`
- `enemy_archetype_weight`
- `enemy_archetype_contribution`
- `archetype_reasons`

### Phase 45 Validation

- Tag validation: PASS. 26 champions, 0 missing tags, 0 unknown tags, 0 champions with more than 5 tags, 0 `needs_review`.
- Native tag loading validation: PASS. 26 champions, 26 with tags, 0 missing tags, 0 unknown tags.
- Native draft AI tag/debug validation: PASS. Baseline output still exposes `candidate_tags`; `native_archetype` exposes archetype scores, weights, contributions, and readable reasons.
- Full draft validation with `native`: PASS, 0 invalid selections.
- Full draft validation with `native_archetype`: PASS, 0 invalid selections.

### 25x25 A/B Summary

| Blue strategy | Red strategy | Blue win rate | Red win rate | Invalid drafts |
|---------------|--------------|---------------|--------------|----------------|
| native_archetype | random | 89.9% | 10.1% | 0 |
| random | native_archetype | 17.0% | 83.0% | 0 |
| native_archetype | native | 61.8% | 38.2% | 0 |
| native | native_archetype | 68.2% | 31.8% | 0 |
| native_archetype | native_archetype | 68.0% | 32.0% | 0 |
| native | native | 61.3% | 38.7% | 0 |

`native_archetype` beats random from both sides and remains competitive with `native`, but its self-play result shows about +6.7 percentage points more Blue-side win rate than `native` self-play in this screen. Keep it experimental; do not promote it to default without further tuning and validation.

## Tuned Archetype Profiles (Phase 46)

Additional experimental profiles were added to isolate the side-bias source. These are test-only strategy names and are not exposed in production UI.

| Profile | Pick weight | Ban weight |
|---------|-------------|------------|
| `archetype_full` | 0.06 | 0.04 |
| `archetype_light` | 0.03 | 0.02 |
| `archetype_pick_light` | 0.03 | 0.00 |
| `archetype_ban_light` | 0.00 | 0.02 |

### Contribution Diagnostic

Report: `archetype_scoring_diagnostic_report.md`

| Profile | Avg contribution | Max | Min | Top recommendation changes |
|---------|------------------|-----|-----|----------------------------|
| `archetype_full` | 0.001175 | 0.004800 | -0.001200 | 2 of 20 |
| `archetype_light` | 0.000588 | 0.002400 | -0.000600 | 1 of 20 |
| `archetype_pick_light` | 0.000403 | 0.002400 | -0.000600 | 1 of 20 |
| `archetype_ban_light` | 0.000185 | 0.001600 | 0.000000 | 0 of 20 |

### 25x25 Screening

| Blue strategy | Red strategy | Blue win rate | Red win rate | Invalid drafts |
|---------------|--------------|---------------|--------------|----------------|
| native | native | 63.4% | 36.6% | 0 |
| archetype_full | archetype_full | 73.4% | 26.6% | 0 |
| archetype_light | archetype_light | 74.6% | 25.4% | 0 |
| archetype_pick_light | archetype_pick_light | 67.8% | 32.2% | 0 |
| archetype_ban_light | archetype_ban_light | 62.1% | 37.9% | 0 |
| archetype_full | native | 60.8% | 39.2% | 0 |
| native | archetype_full | 68.8% | 31.2% | 0 |
| archetype_light | native | 61.1% | 38.9% | 0 |
| native | archetype_light | 71.5% | 28.5% | 0 |
| archetype_pick_light | native | 57.0% | 43.0% | 0 |
| native | archetype_pick_light | 70.2% | 29.8% | 0 |
| archetype_ban_light | native | 62.7% | 37.3% | 0 |
| native | archetype_ban_light | 59.0% | 41.0% | 0 |
| archetype_light | random | 86.4% | 13.6% | 0 |
| random | archetype_light | 21.1% | 78.9% | 0 |
| archetype_ban_light | random | 89.8% | 10.2% | 0 |
| random | archetype_ban_light | 9.4% | 90.6% | 0 |

### 50x25 Confirmation

`archetype_ban_light` was the only profile promoted to confirmation.

| Blue strategy | Red strategy | Blue win rate | Red win rate | Invalid drafts |
|---------------|--------------|---------------|--------------|----------------|
| native | native | 62.5% | 37.5% | 0 |
| archetype_ban_light | archetype_ban_light | 62.5% | 37.5% | 0 |
| archetype_ban_light | native | 58.3% | 41.7% | 0 |
| native | archetype_ban_light | 61.7% | 38.3% | 0 |
| archetype_ban_light | random | 85.8% | 14.2% | 0 |
| random | archetype_ban_light | 14.4% | 85.6% | 0 |

Recommendation: keep all archetype scoring experimental, but use `archetype_ban_light` as the preferred experimental profile for the next phase. It preserves native self-play bias in the 50x25 confirmation while still beating random from both sides and staying competitive with native. Default `native` remains unchanged.

### Phase 47 Checkpoint

Preferred experimental archetype profile: `archetype_ban_light`.

The weaker profiles remain available only for test comparisons:

- `archetype_full`
- `archetype_light`
- `archetype_pick_light`

They are not recommended options and are not production defaults. Archetype scoring is still opt-in, and no archetype profile is promoted to default. The known native draft-order Blue-side bias remains accepted baseline context; future archetype experiments should compare against that baseline rather than treating absolute side fairness as the goal.

### Phase 51 Closure

Phase 50 audited `native` vs `archetype_ban_light` on 9 representative draft states. The audit found:

- 0 of 9 top recommendation changes.
- Max observed top archetype contribution: 0.001600.
- 0 invalid drafts in full draft validation for both `native` and `archetype_ban_light`.
- No suspicious recommendations.
- No pick top recommendation changes.

Conclusion: `archetype_ban_light` is safe but low-impact. Keep it as the preferred experimental archetype profile and do not promote it to default.

Current strategy status:

| Strategy/profile | Status |
|------------------|--------|
| `native` | Default strategy. |
| `archetype_ban_light` | Preferred experimental archetype profile; opt-in only. |
| `archetype_full` | Test-only comparison profile. |
| `archetype_light` | Test-only comparison profile. |
| `archetype_pick_light` | Test-only comparison profile. |
| Lookahead variants | Quarantined experimental due to side-bias results. |

The repeatable audit script is `scripts/tools/audit_archetype_recommendations.gd`. Its generated root-level markdown report is a disposable local artifact; durable conclusions should live in this wiki note and `wiki/notes/draft_archetype_tags.md`.

## Weight Override Behavior

Weight overrides are applied as multipliers to phase-specific base weights:

```gdscript
# Example: Increase enemy-specific signals by 25%
strategy.set_weight_overrides({
    "enemy_synergy_multiplier": 1.25,
    "counters_my_team_multiplier": 1.25,
    "fills_enemy_role_need_multiplier": 1.25
})
```

**Implementation:**
- Multipliers applied after phase-specific weights are set
- `early_fallback_multiplier` is a direct override (not a multiplier)
- Only used in test runners, not in normal draft flow
- Default: empty dictionary (no overrides)

## Known Limitations

1. **No lookahead/minimax** - Current scoring does not simulate future enemy responses
2. **Ban lookahead** - Bans do not explicitly consider how enemy will respond
3. **Archetypes** - Mechanical tags are implemented; archetype-based scoring is experimental and opt-in only
4. **Backward compatibility** - Old GDScript prototype still exists for comparison
5. **Opt-in strategy** - Native strategy is opt-in in tools/test runner (not default in production)
6. **Synergy depth** - Current synergy model is pairwise (no higher-order synergies)
7. **Side asymmetry in secondary matchup** - native_full vs native_picks_random_bans shows 55.4% vs 71.7% asymmetry (may be draft-order advantage)

## Experimental Lookahead Findings

### Overview
1-ply lookahead was implemented as an experimental feature to improve draft quality by simulating enemy responses. However, all lookahead variants introduced unacceptable side bias and were not promoted to default.

### Variants Tested

**native_lookahead:** Full lookahead on both picks and bans
- Applied enemy-response penalty when next pick is enemy
- Side bias: Up to 42.0% (severe)
- Status: Not promoted

**native_lookahead_pick_only:** Lookahead only for picks
- Same logic as full lookahead but bans use baseline
- Side bias: Similar to full lookahead
- Status: Not promoted

**native_lookahead_ban_only:** Lookahead only for bans
- Ban lookahead with baseline picks
- Side bias: 21-25% (moderate but still exceeds threshold)
- Status: Kept as experimental for future research

**native_lookahead_continuation:** Continuation-aware pick lookahead
- Applied enemy-response penalty when next pick is enemy
- Applied follow-up bonus when next pick is same side
- Equalized lookahead opportunities (5 vs 3)
- Side bias: 42.0% (worse than original)
- Status: Removed - made bias significantly worse

### Key Findings

1. **Pick lookahead is the primary bias source**
   - Ban-only lookahead has moderate bias (21-25%)
   - Full lookahead has severe bias (up to 42%)
   - The opportunity imbalance (Blue gets 3 lookahead picks, Red gets 2) amplifies draft-order advantage

2. **Equalizing opportunities didn't help**
   - Continuation variant gave both sides 5 lookahead adjustments
   - The adjustment logic itself is asymmetric
   - Follow-up bonus on same-side picks amplifies draft-order advantage

3. **Baseline has inherent draft-order bias**
   - native baseline self-play: ~18.4% Blue-side bias
   - This is the structural advantage from snake draft order
   - Cannot be eliminated without changing draft order

4. **All lookahead variants failed acceptance criteria**
   - No variant has side bias < 20%
   - Continuation variant made the problem worse
   - Ban-only is best but still not good enough

### Current Status

**Default strategy:** native (baseline with 18.4% draft-order bias)

**Experimental strategies (kept for research):**
- native_lookahead - Severe bias, kept for reference
- native_lookahead_pick_only - Severe bias, kept for reference
- native_lookahead_ban_only - Moderate bias, may be useful for future tuning

**Removed strategies:**
- native_lookahead_continuation - Made bias worse, removed
- native_baseline - Duplicate of native, removed
- native_baseline_ban_lookahead - Same as ban-only, removed

### Future Options

1. **Draft order redesign** - Symmetric draft order to eliminate structural bias
2. **Side-compensated scoring** - Give Red stronger lookahead adjustments
3. **Deeper but symmetric search** - Minimax with equal depth for both sides
4. **Remove lookahead** - If not useful for production

### References

- **Lookahead experiment reports:**
  - `phase34_lookahead_bias_isolation_report.md` - Bias isolation analysis
  - `phase35_lookahead_trace_report.md` - Turn diagnostic and opportunity analysis
  - `phase36_lookahead_variants_report.md` - Safer variants test results

## Draft Order Decision (Phase 38)

### Current Draft Order
The current snake draft order is retained as the production/default order. All tested alternate draft orders have worse bias.

### Known Structural Advantage
Baseline native self-play has a known ~18-20% Blue-side structural advantage due to the snake draft order:
- Current order native self-play: 59.8% Blue / 40.2% Red (19.6% bias)
- Current order random self-play: 48.7% Blue / 51.3% Red (2.6% bias)

This is:
- Not a bug in the scoring logic
- Structural to the snake draft order
- Expected for this type of draft format
- Manageable and well-understood

### Evaluation Guidelines
When evaluating future strategy changes:
1. Compare against the current 19.6% native self-play bias
2. Do not interpret changes as absolute fairness
3. Consider the structural advantage as a constant
4. Focus on relative improvements, not absolute fairness
5. The goal is to minimize bias, not eliminate it entirely

### References
- `phase38_draft_order_bias_audit_report.md` - Full audit results and analysis
- `wiki/notes/draft_order_bias_audit.md` - Comprehensive audit documentation



## Suggested Future Phases

### Phase 23: Lookahead/Minimax
- Implement 1-2 step lookahead for picks
- Simulate enemy responses to candidate picks
- Consider counter-picks in scoring

### Phase 24: Enhanced Comp Fit
- ~~Add partial comp scoring (2-4 unit combinations)~~ **COMPLETED in Phase 29**
- Implement archetype-based comp detection
- Dynamic comp building based on available champions

### Phase 25: Ban Lookahead
- Simulate enemy response to bans
- Consider ban impact on enemy's likely picks
- Optimize bans to force enemy into weak comps

### Phase 26: Archetype Integration
- Add mechanical tags (e.g., "aoe", "burst", "sustain")
- Implement archetype synergy scoring
- Add archetype counter relationships

### Phase 27: Production Integration
- Make native strategy default in production
- Remove GDScript prototype
- Add telemetry for draft quality monitoring

## References

- **Validation reports:**
  - `ban_weight_calibration_report.md` - Initial calibration (25x25)
  - `enemy_specific_heavier_validation_report.md` - Validation (50x25)
  - `ban_weight_cleanup_report.md` - Cleanup status

- **Test runners:**
  - `scripts/tools/full_draft_ablation_test.gd` - A/B testing
  - `scripts/tools/ban_weight_calibration.gd` - Weight calibration

- **Related wiki:**
  - `wiki/notes/draft_recommender_initial.md` - Initial design notes
  - `wiki/notes/draft_prediction_context.md` - Prediction context
