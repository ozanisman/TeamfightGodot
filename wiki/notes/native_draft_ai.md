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

The archetype tag scoring experiment (phases 45-51) has been removed. The native draft AI no longer applies archetype scoring to picks or bans. The previous profiles (`archetype_full`, `archetype_light`, `archetype_pick_light`, `archetype_ban_light`) are no longer available.

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

## Current A/B Baseline (Phase 65) — SUPERSEDED, see Stats Refresh below

- **Report path:** `res://logs/full_draft_ab_test_report.txt` (generated on run, not committed)
- **Date:** 2026-06-15
- **Strategy matchups tested:** native vs random, random vs native, native vs native
- **Native self-play bias:** 59.8% Blue / 40.2% Red (~19.6 pp)
- **Default changed:** No. `native` remains default. Archetype scoring removed.

## Stats Refresh (2026-07-01) — IMPORTANT: baseline numbers above are stale

While validating the Draft AI Improvement Plan's Workstream 0.1 (unify selection policy), the
Phase 65 baseline above was found to no longer match runtime behavior of `model_stats/stats_output_100k/`
against the current simulation engine:

- **Symptom:** `native_draft_validation_harness.gd` and `full_draft_ab_test.gd` (independently,
  both using `res://model_stats/stats_output_100k/`) agreed with each other but *not* with the
  documented Phase 65 numbers: native self-play measured 42.2% Blue / 57.8% Red (bias reversed
  from documented), native vs random measured ~70%/58% (far below documented ~89%/89%).
- **Root cause confirmed:** regenerated `combat_stats.csv` from scratch with the current
  simulation engine (`generate_simulation_stats.gd --team-sizes=5 --matches-per-size=20000`) and
  diffed per-champion win rates against the production CSV. Deltas of 9-16 percentage points on
  many champions (e.g. ninja -15.6pp, paladin -12.7pp, wraith -11.1pp, guardian +11.2pp,
  necromancer +9.5pp) — far beyond sampling noise at this scale (95% CI ~±1.1pp). The production
  stats snapshot had drifted out of sync with the live combat/effect resolution engine (likely via
  the recent damage/status/periodic-effect refactors) and was never regenerated to match. Champion
  roster count matched (26/26) so this was not a missing-champion issue.
- **Action taken:** regenerated `combat_stats.csv`, `matchup_with.csv`, `matchup_vs.csv`,
  `role_combinations.csv`, `role_stats.csv`, `summary_stats.csv` under
  `model_stats/stats_output_100k/` at 20,000 matches/team-size-5 (base seed 500000). Old files
  backed up to `model_stats/stats_output_100k_backup_20260701_220202/` (gitignored, local only).
- **New measured baseline** (`native_draft_validation_harness.gd`, 25 trials x 25 sims/draft,
  base seed 300000):
  - native_full vs random: **96.8% Blue / 90.2% Red-as-native** (up sharply from stale-data ~70%)
  - native_full self-play (mirror): **38.6% Blue / 61.4% Red** — bias not just smaller than
    documented, but reversed direction and *larger* in magnitude (22.8pp Red-favored) than the old
    19.6pp Blue-favored figure.
  - native_softmax (see below) vs random: 86.9% Blue / 89.9% Red-as-native.

**Root cause of the side-bias reversal, diagnosed 2026-07-01 (see
`wiki/notes/draft_order_bias_audit.md` "Stats-Sensitivity Finding" for full writeup):** confirmed
`SimConstantsScript.DRAFT_SEQUENCE` is byte-identical to the historical "current" order documented
in the audit (not accidentally swapped to "mirrored_current") — this is not a sequence/code bug.
At larger sample size (n=2500/matchup) the bias is even more pronounced and diverges sharply by
selection policy:
- `native_full` (deterministic top-1) self-play: **28.6pp Red-favored** (35.7%/64.3%)
- `native_softmax` (the actual shipped gameplay policy) self-play: **11.6pp Blue-favored**
  (55.8%/44.2%) — opposite sign and roughly 2.5x smaller magnitude than `native_full`.

**Conclusion:** the previously-documented "~19.6% Blue advantage is structural (from draft order)
and safe to treat as a constant" is falsified. The effect is dominated by an interaction between
the deterministic argmax recommender and the specific champion-power snapshot in
`combat_stats.csv` (which pick chain the deterministic policy converges to depends on which
champions are currently strongest), not by the fixed snake order alone — the same order produces
opposite-signed bias under the stochastic policy. This is a direct, concrete confirmation of
Guiding Principle #1 in `draft_ai_improvement_plan.md` ("measure the policy you ship"): the
historical bias figure was measured against `native_full`, a policy nobody actually plays against,
and does not predict the sign or magnitude of the bias players actually experience via
`native_softmax`. Any future side-bias regression gate must track `native_full` and
`native_softmax` bias separately — they are not interchangeable — and should not assume either
figure is a fixed constant given it has now moved substantially across two stats regenerations.
- **Implication for the improvement plan:** the "Known baseline numbers to preserve as regression
  anchors" in `wiki/notes/draft_ai_improvement_plan.md` §6 are stale and should not be used as-is;
  they should be re-derived from this refreshed stats snapshot. This also confirms the plan's own
  §5.4/§0.3 diagnosis (no stats provenance/versioning, no regeneration loop) is a live, active
  problem, not just a theoretical risk.

## Certified Holdout / Bayes Ceiling Re-Verification (2026-07-01)

The old "certified holdout ≈ 76.1% acc / MSE 0.0484; Bayes ceiling ≈ 75.4%" numbers from
`draft_ai_improvement_plan.md` §6 were re-measured against the refreshed stats and current engine.

- **Bayes ceiling:** `measure_draft_ceiling.gd` (mirror mode, 500 comps × 100 seeds, 100,000 decisive
  matches) reports **78.6%** split-half ceiling (95% CI ±0.4%, n=50,000). Raw-max cross-check: 79.0%.
  This is higher than the old 75.4%/75.8% figures, reflecting the current simulation engine's larger
  comp imbalance distribution.
- **Pairwise logistic holdout:** `verify_pairwise_signal.gd` on the same side-debiased holdout, with
  `--stats-dir=res://model_stats/stats_output_100k`, reports the retrained pairwise logistic at
  **78.0% test accuracy / MSE 0.0401**. The handcoded current heuristic baseline is 77.0% / MSE 0.0804.
  The pairwise model is therefore within ~0.6pp of the mirror Bayes ceiling, leaving little
  complete-team prediction signal on the table.
- **Interpretation:** the certified winner-predictor is not the bottleneck. The remaining headroom is
  in the draft policy (greedy, deterministic-vs-softmax, no search), not in the quality of the
  pairwise probability signal.

## Softmax Validation Strategy (`native_softmax`)

Added `scripts/tools/draft_strategy_native_softmax.gd` and `scripts/tools/draft_policy.gd` to
close the Section 3.5 determinism-vs-gameplay-softmax mismatch: gameplay
(`simulation_viewer_base.gd::_try_enemy_draft_ai`) requests top-5 recommendations and samples via
softmax (temperature=0.5, scale=100), but the validation harness previously only ever exercised
deterministic top-1 (`native_full`). `native_softmax` reproduces the exact shipped policy so the
harness can measure the policy players actually face. `simulation_viewer_base.gd::_softmax_select`
now delegates to the shared `DraftPolicy.softmax_select()` used by both (behavior-preserving
refactor, verified bit-identical via git-stash diff on `native_full`/`random` matchups).

## Difficulty tiers (Workstream A.1)

Enemy draft AI difficulty is controlled by softmax temperature + top-k presets in
`scripts/tools/draft_ai_config.gd` (`easy` / `normal` / `hard`). Normal reproduces the
prior shipped policy. The draft screen bottom bar exposes an **AI Difficulty** selector
(wired through `DraftScreenShell.ai_difficulty_changed` → `simulation_viewer_base.gd`).

Validation strategies `native_softmax_easy`, `native_softmax`, and `native_softmax_hard`
mirror gameplay presets. `native_draft_tier_gate.gd` checks monotonic blue win rate vs
random (default min gap 2pp between adjacent tiers).

Optional JSON override (`model_stats/draft_ai_config.json`):

```json
{
  "difficulty_tiers": {
    "easy": { "temperature": 2.0, "top_k": 8 },
    "normal": { "temperature": 0.5, "top_k": 5 },
    "hard": { "temperature": 0.15, "top_k": 3 }
  }
}
```

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
3. **Backward compatibility** - Old GDScript prototype still exists for comparison
4. **Opt-in strategy** - Native strategy is opt-in in tools/test runner (not default in production)
5. **Synergy depth** - Current synergy model is pairwise (no higher-order synergies)
6. **Side asymmetry in secondary matchup** - native_full vs native_picks_random_bans shows 55.4% vs 71.7% asymmetry (may be draft-order advantage)

## Lookahead experiments (historical)

Quarantined strategies; did not improve bias. Signal ceiling and failed feature gates: [draft_prediction_context.md](draft_prediction_context.md).

## Draft order bias (Phase 38 / post-refresh update)

The old ~19.6% Blue-advantage figure is **no longer valid** after the stats refresh. Current
snapshot behavior: [draft_order_bias_audit.md](draft_order_bias_audit.md). Phase 29 baseline metrics:
[native_draft_ai_baseline.md](native_draft_ai_baseline.md).

## Validation Suite

Quantitative harness, analyzer, Elo/quantitative/tier gates, self-play stats, and suite aggregation: [draft_ai_validation_gate.md](draft_ai_validation_gate.md).

### Validation Suite Script

The standardized validation suite is `scripts/tools/run_draft_ai_validation_suite.gd`. It runs all available validation checks and generates a comprehensive report.

### File-Based Status

The validation suite is file-based and does not depend on stdout parsing. Each validation script writes a report file with a `STATUS: PASS` or `STATUS: FAIL` line. The suite reads these files to determine check results.

**Headless execution:** Use [`run_godot.ps1`](../../run_godot.ps1) for wired draft flags (logging to `logs/godot.log`, timeouts). Example: `.\run_godot.ps1 -- --validate-full-draft`. For scripts not wired in the launcher (e.g. `run_draft_ai_validation_suite.gd`), invoke Godot directly with `--headless --path . --script`.

### Running the Suite

```bash
godot --headless --script res://scripts/tools/run_draft_ai_validation_suite.gd
```

### Required Checks

The suite runs the following required checks:

1. **Full Draft Validation (native)** (`full_draft_validation.gd --strategy=native`)
   - Report: `res://logs/full_draft_validation_report_native.txt` (generated on run, not committed)
   - Simulates complete snake draft with native baseline strategy
   - Checks for duplicate picks, duplicate bans, pick/ban overlap
   - Validates all selections are from available pool

2. **Native Recommendation Explanations Audit** (`audit_native_recommendation_explanations.gd`)
   - Report: `res://logs/native_recommendation_explanations_audit_report.md` (generated on run, not committed)
   - Audits native recommendation debug output for completeness and validity
   - Checks for missing required debug fields, suspicious zero fields, invalid values
   - Covers representative draft states (empty draft, early bans, first picks, mid draft, phase-2 bans, late picks, near-complete draft)

3. **Native Ban Quality Audit** (`audit_native_ban_quality.gd`)
   - Report: `res://logs/native_ban_quality_audit_report.md` (generated on run, not committed)
   - Audits native ban recommendations for quality and suspicious patterns
   - Checks for self-denial risk, denial ratio, suspicious value patterns
   - Covers representative ban states (empty draft, early bans, phase 1/2 bans, enemy team shells, ally vulnerability)

### Report Output

The suite generates `res://logs/draft_ai_validation_suite_report.md` (generated on run, not committed) with:

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

### Quarantined (validation-only; not in gameplay)
- ~~Lookahead/Minimax~~ — Phases 34–37 (`NATIVE_LOOKAHEAD`, `NATIVE_LOOKAHEAD_PICK`, `NATIVE_LOOKAHEAD_BAN`). Legacy harness name `native_lookahead` uses `opponent_model=top1` (`fixtures/draft_ai/draft_ai_config_legacy_lookahead.json`). Original quarantine reason: **>20pp** side bias with deterministic opponent + top-8 pool truncation.
- **B.2 revival (2026-07-04):** `native_lookahead_softmax` — policy-aware opponent (softmax expectation) + `DraftPolicy.softmax_select`; passes `native_draft_lookahead_gate.gd` at **13.9pp** self-play bias (n=50). Remains validation-only until explicit promotion.
- ~~Ban Lookahead~~ — Same quarantine/revival path as pick lookahead.

### Open
- Dynamic comp building based on available champions
- Make `native` strategy default in production UI
- Add telemetry for draft quality monitoring

## References

- **Validation reports** (ad-hoc; generated on run, not committed):
  - `user://ban_weight_calibration_report.txt` — from `ban_weight_calibration.gd`
  - `res://logs/full_draft_ablation_report.txt` — from `full_draft_ablation_test.gd`

- **Test runners:**
  - `scripts/tools/full_draft_ablation_test.gd` - A/B testing
  - `scripts/tools/ban_weight_calibration.gd` - Weight calibration

- **Related wiki:**
  - [draft_prediction_context.md](draft_prediction_context.md) — Prediction signal and limitations
