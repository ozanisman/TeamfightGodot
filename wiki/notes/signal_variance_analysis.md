# Signal Variance Analysis

## Purpose
Tests the hypothesis that limited signal variance is the bottleneck preventing draft prediction differentiation. Analyzes variance and correlations across base/synergy/counter winrate signals from historical match data.

## How to Run
```powershell
.\run_godot.ps1 --generate-stats --analyze-signal-variance --signal-variance-output=res://stats_output/signal_variance.csv --out-dir=res://stats_output --team-sizes=5 --matches-per-size=10000 --base-seed=1
```

**Flags:**
- `--generate-stats`: Generate simulation stats first
- `--analyze-signal-variance`: Run signal variance analysis after stats generation
- `--signal-variance-output=...`: Path for the CSV output file
- `--out-dir=...`: Directory for stats files
- `--team-sizes=5`: Analyze 5v5 matches
- `--matches-per-size=10000`: Generate 10,000 matches per team size
- `--base-seed=1`: Random seed for reproducibility

## Implementation
- `scripts/tools/analyze_signal_variance.gd` - Main analysis script
- `scripts/tools/generate_simulation_stats.gd` - CLI integration (lines 186-195)
- `native/src/simulation/sim_draft_recommender.cpp` - C++ recommender (variance computed live per draft state)

## Findings (10,000 matches, K=10)

**Signal Variance (stddev):**

| Signal | K=100 | K=10 | Level |
|--------|-------|------|-------|
| base_smoothed | 0.0488 | 0.0488 | medium |
| avg_synergy_smoothed | ~0.0099 | 0.0349 | medium |
| avg_counter_smoothed | ~0.0099 | 0.0349 | medium |

K=10 increased synergy/counter stddev ~3.5× vs K=100. Smoothing was suppressing signal magnitude.

**Signal Correlations:**

| Pair | Correlation |
|------|------------|
| base vs avg_synergy | 1.0 |
| base vs avg_counter | 1.0 |
| synergy vs counter | 1.0 |
| base vs synergy_variance | 0.30 |
| base vs counter_variance | -0.07 |
| synergy_variance vs counter_variance | 0.05 |

**Flat Profiles:** 20/26 champions (77%) have all smoothed signals within ±0.05 of 0.5

## Root Cause (Confirmed Structural)

`avg_synergy ≈ base_winrate` by construction: averaging pairwise winrates over all partners collapses toward overall win tendency. Same for `avg_counter`. Reducing K does not fix the 1.0 correlation — it only changes the magnitude. More data also does not fix this.

**Variance signals are independent:** `synergy_variance` (corr 0.30 with base) and `counter_variance` (corr -0.07) measure context-dependence, not win tendency. These are orthogonal to base and to each other.

## Current State

- `SMOOTHING_K = 10.0` in `analyze_signal_variance.gd` (changed from 100, better calibrated)
- `synergy_variance` and `counter_variance` are:
  - Computed globally in the GDScript analysis tool
  - Included in the GDScript correlation matrix
  - Computed per-draft-state in `sim_draft_recommender.cpp` (`aggregate_relationship_signal()`)
  - Exposed in `DraftEvaluation` struct fields and `get_draft_recommendations_with_breakdowns()` dict
  - **Wired into scoring** via `variance_weight` in `PredictionConfig` (default 0.0). Sweep below shows no reliable gain at any positive value; default stays 0.0.

C++ recommender uses LEGACY Bayesian smoothing (`confidence_prior_samples = 100`), independent of the GDScript `SMOOTHING_K`.

## Mechanical Signals (Added)

Kit-derived signals extracted from ability/ultimate effect trees in `champion_catalog.gd`. Independent of historical match outcomes by construction.

| Signal | Source | Corr with base_smoothed |
|--------|--------|------------------------|
| `cc_score` | CC effect count (normalized), recursive over multi_effect/splash | **-0.133** |
| `mobility_score` | Has self_dash or auto_dodge | 0.027 |
| `sustain_score` | Has heal/heal_over_time/damage_based_heal or life_steal > 0 | 0.167 |

All < 0.5 → independent signals confirmed. `cc_score` vs `avg_synergy_smoothed` = -0.123.

**Implementation:** `scripts/tools/analyze_signal_variance.gd`
- `_load_mechanical_signals()` — loads catalog, counts CC/mobility/sustain per champion
- `_count_cc_in_effect()`, `_has_mobility_in_effect()`, `_has_sustain_in_effect()` — recursive effect tree traversal
- Constants: `CC_KINDS`, `MOBILITY_KINDS`, `SUSTAIN_KINDS`
- Added to profile dict, correlation matrix, and CSV output

**Now in C++ scoring** — loaded from `mechanical_signals.csv`, exposed in `DraftEvaluation`, added to scoring via `cc_weight`, `mobility_weight`, `sustain_weight` in `PredictionConfig` (all default 0.0).

## Mechanical Signal Sweep Results (10000 matches, hold-out stats)

| Signal | Weight | Accuracy | Brier | Log-loss | vs Baseline |
|--------|--------|----------|-------|----------|-------------|
| Baseline | 0.0 | 63.5% | 0.2230 | 0.6362 | — |
| cc_weight | 0.05 | 63.3% | 0.2235 | 0.6374 | -0.2% |
| cc_weight | 0.1 | 63.0% | 0.2251 | 0.6411 | -0.5% |
| cc_weight | 0.2 | 61.3% | 0.2318 | 0.6559 | -2.2% |
| mobility_weight | 0.05 | 63.6% | 0.2231 | 0.6366 | +0.1% |
| mobility_weight | 0.1 | 63.0% | 0.2237 | 0.6379 | -0.5% |
| sustain_weight | 0.05 | 63.0% | 0.2239 | 0.6382 | -0.5% |

**Conclusion:** No mechanical signal shows meaningful improvement. Best result is `mobility_weight=0.05` with +0.1% accuracy gain, but Brier/log-loss are slightly worse. CC signal degrades accuracy significantly at higher weights. All mechanical signal weights should remain at 0.0 (disabled).

## Counter-Pick Specificity (Added)

Min/max winrates vs enemy team to capture specific counter-pick opportunities instead of flat averages.

**Implementation:** Added `best_counter`, `worst_counter` to `DraftEvaluation` and `best_counter_weight`, `worst_counter_weight` to `PredictionConfig`. Modified `RelationshipAggregate` to track min/max smoothed values.

**Evaluation Results (10000 matches, hold-out stats):**

| Signal | Weight | Accuracy | Brier | Log-loss | vs Baseline |
|--------|--------|----------|-------|----------|-------------|
| Baseline | 0.0 | 63.5% | 0.2230 | 0.6362 | — |
| best_counter_weight | 0.05 | 63.5% | 0.2230 | 0.6362 | 0.0% |
| best_counter_weight | 0.2 | 63.5% | 0.2230 | 0.6362 | 0.0% |
| worst_counter_weight | 0.2 | 63.5% | 0.2230 | 0.6362 | 0.0% |

**Conclusion:** Zero impact. Metrics identical to baseline at all weights. Root cause: flat champion profiles mean min/max/avg are all similar.

## Synergy Specificity (Added)

Min/max winrates vs ally team to capture specific synergy opportunities instead of flat averages.

**Implementation:** Added `best_synergy`, `worst_synergy` to `DraftEvaluation` and `best_synergy_weight`, `worst_synergy_weight` to `PredictionConfig`.

**Evaluation Results (10000 matches, hold-out stats):**

| Signal | Weight | Accuracy | Brier | Log-loss | vs Baseline |
|--------|--------|----------|-------|----------|-------------|
| Baseline | 0.0 | 63.5% | 0.2230 | 0.6362 | — |
| best_synergy_weight | 0.2 | 63.5% | 0.2230 | 0.6362 | 0.0% |

**Conclusion:** Zero impact. Same root cause as counter-pick specificity.

## Remaining Signals (Skipped)

Skipped evaluation of:
- Team composition archetypes
- Draft phase advantage signals
- Historical performance trends

**Rationale:** ~~All three would suffer from the same root cause: flat champion profiles (77% of champions have all smoothed signals within ±0.05 of 0.5). Without meaningful variance in base winrates, no signal can provide meaningful differentiation. The bottleneck is champion balance, not signal design.~~

**Updated Rationale:** Champion balance is by design (champions are supposed to be relatively equal). The real bottleneck is structural signal redundancy, not flat profiles.

## Root Cause Analysis (Updated)

**Issue 1: Structural Signal Redundancy**
- `avg_synergy` and `avg_counter` are perfectly correlated (1.0) with `base_winrate`
- This is by construction: averaging pairwise winrates over all partners collapses toward overall win tendency
- The aggregation method (flat average) is the problem, not the data
- Synergy/counter signals provide no independent information beyond base winrate

**Issue 2: Smoothing Mismatch**
- C++ recommender uses `confidence_prior_samples = 100` (legacy)
- GDScript analysis uses `SMOOTHING_K = 10.0`
- This mismatch means C++ signals are more heavily smoothed than analysis suggests
- C++ synergy/counter variance is suppressed further than GDScript shows

**Issue 3: Scoring Mode Limitations**
- Current scoring modes (multiplicative, logit, additive) all assume independence between signals
- With perfect correlation, these modes don't capture real synergistic/counter effects
- The interaction_weight in logit mode is insufficient to compensate for structural redundancy

**Issue 4: Sparse Pairwise Data**
- 26 champions × 26 champions = 676 possible pairwise matchups
- With 10,000 matches, average samples per pair is ~15 (assuming uniform distribution)
- Below the 20-sample threshold used in analysis
- Many pairs have insufficient data to reliably estimate true effects

**Issue 5: Random Draft Context**
- Simulation uses random draft order, not strategic drafting
- Real drafting involves counter-picking and synergy-building based on opponent picks
- The model doesn't see the sequential decision-making context that creates true counter-pick value

**Issue 6: Composition Signal Underutilized**
- Role fingerprint winrates exist but composition_weight defaults to 0.0
- Team composition archetypes (e.g., "protect the carry", "all-in dive") are not modeled
- Current role-based composition may not capture strategic archetypes

**Recommendation:** Focus on signal aggregation methods (not champion balance). Replace flat averages with context-aware aggregation that captures specific matchup/synergy opportunities rather than collapsing to overall win tendency.

## variance_weight Sweep Results (2000 matches, hold-out stats)

| variance_weight | Accuracy | Brier | Log-loss | Calib gap |
|---|---|---|---|---|
| 0 (baseline) | 64.8% | 0.2243 | 0.6403 | +8.1% |
| 5 | 64.9% | 0.2247 | 0.6412 | +8.9% |
| 10 | 65.0% | 0.2252 | 0.6422 | +8.8% |
| 20 | 64.0% | 0.2265 | 0.6450 | +8.5% |
| 50 | 62.3% | 0.2329 | 0.6582 | +3.8% |
| 100 | 56.8% | 0.2502 | 0.6957 | -5.8% |

**Conclusion:** Accuracy nudges +0.2% at w=10 but Brier/log-loss worsen consistently throughout. Variance signal is not informative at this sample size/champion diversity. `variance_weight` default stays 0.0.

## Next Steps (Priority Order)

1. **Improve champion balance** — 77% flat-profile champions indicates homogeneous kit design. Real variance in base winrate is a prerequisite for meaningful synergy/counter signal.

2. ~~Wire mechanical signals into C++ recommender~~ — **COMPLETED**. Mechanical signals (cc, mobility, sustain) were integrated into C++ scoring and evaluated. No meaningful improvement found; all weights remain at 0.0 (disabled).

## Parameters
- `SMOOTHING_K`: 10.0 in GDScript analysis tool (was 100)
- `PRIOR_WINRATE`: 0.5
- `FLAT_PROFILE_THRESHOLD`: 0.05
- Minimum samples: 20
- C++ `confidence_prior_samples`: 100 (LEGACY smoothing, separate from GDScript K)
