# Native Draft AI Checkpoint

## Purpose

The native draft AI provides C++-based pick and ban recommendations for the Teamfight autobattler. It replaces the previous GDScript-only prototype with a more performant and extensible scoring system that can handle complex champion interactions at scale.

## Files/Classes Involved

### Production Recommender (Current Default)
- `native/src/simulation/sim_draft_ai_recommender.hpp/cpp` (namespace `sim::draft_ai`) — Current production recommender for pick/ban recommendations
  - Routes through `get_draft_ai_pick_recommendations()` and `get_draft_ai_ban_recommendations()`
  - Strategies (`DraftStrategy`): `NATIVE_FULL` (default); `NATIVE_LOOKAHEAD`, `NATIVE_LOOKAHEAD_PICK`, `NATIVE_LOOKAHEAD_BAN` (quarantined experimental)
- `native/src/simulation/sim_draft_recommender.hpp/cpp` (namespace `sim::draft`) — Winner prediction and analysis tools (kept for analysis, not pick/ban routing)
  - `ScoringMode::CERTIFIED_PAIRWISE_PROBABILITY` — complete-draft winner prediction
  - `ScoringMode::DRAFT_AWARE_PAIRWISE_PROBABILITY` — draft-aware pick/ban ranking
- `native/src/teamfight_simulation_core.cpp` — Godot binding for draft recommendations

### Production Data/Scoring Layer (sim::draft_ai)
- `native/src/simulation/sim_draft_ai_stats_database.hpp/cpp` — Stats database for champion win rates
- `native/src/simulation/sim_draft_ai_evaluator.hpp/cpp` — Draft evaluation logic
- `native/src/simulation/sim_draft_ai_types.hpp` — Type definitions

### GDScript Integration
- `scripts/simulation/native_simulation_backend.gd` - Godot-C++ bridge
- `scripts/tools/draft_strategy_native.gd` - Native strategy wrapper (uses `sim::draft_ai`)
- `scripts/tools/draft_strategy_native_picks_random_bans.gd` - Hybrid strategy (native picks, random bans)
- `scripts/tools/draft_strategy_random_picks_native_bans.gd` - Hybrid strategy (random picks, native bans)
- `scripts/tools/full_draft_ablation_test.gd` - Test runner for validation

**Note:** Main recommendation methods (`get_draft_recommendation_names`, `get_draft_recommendations_with_breakdowns`) now route to `sim::draft_ai` system by default. `sim::draft` system is retained for winner prediction and analysis tools only.

## API Breaking Changes (Phase 71)

### Field Mapping Changes
The routing to `sim::draft_ai` system introduced field name mapping changes in `get_draft_recommendations_with_breakdowns()`:

- `candidate` → `champion` (output field name)
- `base_power` → `base` (output field name)  
- `ally_synergy` → `synergy` (output field name)
- `enemy_counter_value` → `counter` (output field name)
- `total_score` → `final` (output field name)

### Variance Fields
Variance fields (`synergy_variance`, `counter_variance`) are currently set to 0.0 as the `sim::draft_ai` system does not provide variance calculations. This is a temporary state - either variance calculation will be implemented in draft_ai or these fields will be removed entirely.

### Parameter Changes
- Weight parameters (`base_weight`, `synergy_weight`, `counter_weight`, etc.) are currently ignored by the routing methods (kept for API compatibility)
- `draft_position` parameter maps to `draft_step` in the underlying draft_ai system
- Default strategy is set to NATIVE (strategy parameter = 0)

### Error Handling
Both routing methods now check for empty results from the draft_ai system and log warnings when database load failures occur. Callers should handle empty array returns as potential failure conditions.

## Data Inputs Loaded

The native draft AI loads statistics from the draft stats database:

- **Champion win rates** - Overall and per-role performance
- **Synergy scores** - Champion pair performance when on same team
- **Counter scores** - Champion pair performance when on opposing teams
- **Role combinations** - Pre-computed 5-unit comp win rates from `role_combinations.csv`
- **Champion metadata** - Roles, tags, and other static properties

Stats directory: `res://model_stats/stats_output_100k/` (configurable per strategy)

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
var strategy = DraftStrategyNative.new("res://model_stats/stats_output_100k")
```

Available strategies:
- `DraftStrategyNative` - Full native picks and bans (default)
- `DraftStrategyNativePicksRandomBans` - Native picks, random bans
- `DraftStrategyRandomPicksNativeBans` - Random picks, native bans
- `DraftStrategyRandom` - Full random (baseline)
- `DraftStrategyCertified` - Certified pairwise probability model (complete-draft default)

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
- **Weight overrides:** Opt-in only via `set_weight_overrides()`
- **Default profile:** Current baseline (no multipliers applied)
- **Fallback multiplier:** 0.30 (applied in early ban phase)

## Archetype Scoring (Removed)

The archetype tag scoring experiment (phases 45-51) has been removed. Champion tags remain in the schema as inactive metadata, but the native draft AI no longer applies archetype scoring to picks or bans. The previous profiles (`archetype_full`, `archetype_light`, `archetype_pick_light`, `archetype_ban_light`) are no longer available.

Current strategy status:

| Strategy/profile | Status |
|------------------|--------|
| `native` | Default strategy. |
| Lookahead variants | Quarantined experimental due to side-bias results. |

## Trusted A/B Tooling (Phase 70)

### Correct Stats Path
All A/B test tools must use `res://model_stats/stats_output_100k/` as the native stats directory. The old path `res://stats_output_100k/` was fixed across 13 files in Phase 70.

### Missing-Stats Guard
A/B tools now validate required CSVs (`combat_stats.csv`, `matchup_with.csv`, `matchup_vs.csv`) at startup. If any are missing, the tool prints a clear error and exits with code 1 instead of silently falling back to weak/random-like behavior.

### Trusted Commands

**Head-to-head (recommended for controlled pairwise comparisons):**
```powershell
godot --headless --path . --script res://scripts/tools/compare_draft_models_head_to_head.gd -- --pair=native,native --seeds=50 --sims-per-draft=10 --output=res://model_stats/draft_h2h_native_selfplay.csv
```

**Full draft A/B (for multi-matchup reports):**
```powershell
godot --headless --path . --script res://scripts/tools/full_draft_ab_test.gd -- --trials=50 --sims-per-draft=10 --matchups="native:native,native:random,random:native"
```

**Note:** Both tools now produce consistent results (native self-play blue win rate ~60%). Prior to Phase 70, `full_draft_ab_test` produced ~47% blue due to the broken stats path.

## Current A/B Baseline (Phase 65)

- **Report path:** `draft_ai_current_ab_baseline_report.md`
- **Date:** 2026-06-15
- **Strategy matchups tested:** native vs random, random vs native, native vs native
- **Native self-play bias:** 59.8% Blue / 40.2% Red (~19.6 pp)
- **Default changed:** No. `native` remains default. Archetype scoring removed.

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
3. **Archetypes** - Champion tags remain as inactive metadata; archetype-based scoring has been removed
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



## Validation Suite

### Validation Suite Script

The standardized validation suite is `scripts/tools/run_draft_ai_validation_suite.gd`. It runs all available validation checks and generates a comprehensive report.

### File-Based Status

The validation suite is file-based and does not depend on stdout parsing. Each validation script writes a report file with a `STATUS: PASS` or `STATUS: FAIL` line. The suite reads these files to determine check results.

**Headless Execution Caveat:** When running Godot headless scripts, use direct Godot executable invocation with `--headless --path . --script`. The PowerShell wrapper (`run_godot.ps1`) runs the main scene instead of the specified script when `run/main_scene` is set in `project.godot`.

### Running the Suite

```bash
godot --headless --script res://scripts/tools/run_draft_ai_validation_suite.gd
```

### Required Checks

The suite runs the following required checks:

1. **Native Draft AI Tag Validation** (`validate_native_draft_ai_tags.gd`)
   - Report: `native_draft_ai_tag_path_report.md`
   - Validates native draft AI tag output
   - Compares candidate tags to schema

2. **Full Draft Validation (native)** (`full_draft_validation.gd --strategy=native`)
   - Report: `full_draft_validation_report_native.txt`
   - Simulates complete snake draft with native baseline strategy
   - Checks for duplicate picks, duplicate bans, pick/ban overlap
   - Validates all selections are from available pool

3. **Native Recommendation Explanations Audit** (`audit_native_recommendation_explanations.gd`)
   - Report: `native_recommendation_explanations_audit_report.md`
   - Audits native recommendation debug output for completeness and validity
   - Checks for missing required debug fields, suspicious zero fields, invalid values
   - Covers representative draft states (empty draft, early bans, first picks, mid draft, phase-2 bans, late picks, near-complete draft)

4. **Native Ban Quality Audit** (`audit_native_ban_quality.gd`)
   - Report: `native_ban_quality_audit_report.md`
   - Audits native ban recommendations for quality and suspicious patterns
   - Checks for self-denial risk, denial ratio, suspicious value patterns
   - Covers representative ban states (empty draft, early bans, phase 1/2 bans, enemy team shells, ally vulnerability)

### Report Output

The suite generates `draft_ai_validation_suite_report.md` with:

- Backend availability status
- Individual check results (PASS/FAIL/SKIPPED) with report file paths
- Overall result (PASS/FAIL)
- Timestamp

### Exit Codes

- Exit code 0: All required checks passed
- Exit code 1: One or more required checks failed, or backend unavailable

## Suggested Future Phases

### Completed
- ~~Partial comp scoring (2-4 unit combinations)~~ — **COMPLETED in Phase 29**

### Quarantined (not promoted to default)
- ~~Lookahead/Minimax~~ — Implemented in Phases 34–37; all variants introduced unacceptable side bias (>20%). See [native_draft_ai_lookahead_experiment.md](native_draft_ai_lookahead_experiment.md).
- ~~Ban Lookahead~~ — Same quarantine as above.

### Open
- Dynamic comp building based on available champions
- Make `native` strategy default in production UI
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
