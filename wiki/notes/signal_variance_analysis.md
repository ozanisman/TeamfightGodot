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

## Implementation Specifics

### Fix Issue 1: Context-Aware Aggregation

**Current Implementation** (`sim_draft_recommender.cpp:98-140`):
```cpp
result.adjusted_average = adjusted_sum / double(result.relationships);
```
Flat average over all partners/enemies causes collapse to base winrate.

**Proposed Aggregation Modes:**

1. **Sample-Weighted Average** (immediate fix):
   - Weight each pairwise winrate by its sample count
   - High-sample pairs get more influence
   - Already have `stat_samples` in `RelationshipAggregate`
   - Implementation:
     ```cpp
     double weighted_sum = 0.0;
     double total_weight = 0.0;
     for (const StringName &other : others) {
         // ... get stat and smoothed value ...
         double weight = stat.samples;
         weighted_sum += weight * smoothed;
         total_weight += weight;
     }
     result.adjusted_average = weighted_sum / total_weight;
     ```

2. **Median Aggregation** (robust to outliers):
   - Use median instead of mean
   - Less sensitive to extreme values
   - Implementation: collect values in vector, sort, take middle

3. **Top-K/Bottom-K Aggregation** (focus on extreme matchups):
   - For counter: only consider worst K enemies (threats)
   - For synergy: only consider best K allies (core synergies)
   - Configurable K parameter
   - Implementation: sort values, take first/last K

4. **Percentile-Based Aggregation**:
   - Use 25th/75th percentiles instead of mean
   - Captures distribution shape
   - Implementation: sort, take percentile positions

**Add to `PredictionConfig`:**
```cpp
enum class AggregationMode {
    FLAT_AVERAGE,        // current (baseline)
    SAMPLE_WEIGHTED,     // weight by sample count
    MEDIAN,              // median of pairwise values
    TOP_K,               // best K for synergy, worst K for counter
    PERCENTILE_25,       // 25th percentile
    PERCENTILE_75        // 75th percentile
};
AggregationMode synergy_aggregation = AggregationMode::FLAT_AVERAGE;
AggregationMode counter_aggregation = AggregationMode::FLAT_AVERAGE;
int top_k = 3;  // for TOP_K mode
```

### Fix Issue 2: Smoothing Mismatch

**Current Implementation** (`sim_draft_recommender.hpp:41`):
```cpp
int confidence_prior_samples = 100;  // C++ legacy
```

**Fix:** Change to match GDScript analysis:
```cpp
int confidence_prior_samples = 10;  // match SMOOTHING_K
```

**Impact:** Will increase signal variance in C++ by ~3.5× (as seen in GDScript analysis).

### Fix Issue 3: Scoring Mode for Correlated Signals

**Current Issue:** Scoring modes assume signal independence.

**Proposed: Decorrelated Scoring Mode**
- Subtract base component from synergy/counter before scoring
- Use residuals (synergy - base, counter - base) instead of raw values
- Implementation in `score_from_signals()`:
  ```cpp
  double synergy_residual = avg_synergy - base_winrate;
  double counter_residual = avg_counter - base_winrate;
  // Use residuals in scoring instead of raw values
  ```

**Add to `PredictionConfig`:**
```cpp
bool use_decorrelated_scoring = false;  // opt-in
```

### Fix Issue 4: Sparse Pairwise Data

**Current Issue:** Average 15 samples per pair, below 20-sample threshold.

**Proposed: Hierarchical Smoothing**
- First smooth individual pairs with Bayesian smoothing
- Then smooth across similar champions (by role, kit similarity)
- Borrow strength from related champions
- Implementation: add champion-level priors based on role averages

**Add to `PredictionConfig`:**
```cpp
bool use_hierarchical_smoothing = false;
double role_prior_weight = 0.1;  // weight for role-level prior
```

### Fix Issue 5: Random Draft Context

**Current Issue:** No sequential decision-making context.

**Proposed: Draft-Aware Scoring**
- Track draft order (pick number)
- Adjust weights based on draft position (early vs late)
- Early picks: weight base winrate higher
- Late picks: weight counter/synergy higher
- Implementation: pass draft_order to evaluation

**Add to `PredictionConfig`:**
```cpp
int draft_position = 0;  // 0 = not specified, 1-5 = pick order
double early_pick_base_weight = 0.7;  // for picks 1-2
double late_pick_counter_weight = 0.4;  // for picks 4-5
```

### Fix Issue 6: Composition Signal Underutilized

**Current Issue:** `composition_weight` defaults to 0.0, role-based only.

**Proposed: Archetype-Based Composition**
- Define composition archetypes (e.g., "protect carry", "all-in dive", "control")
- Compute archetype winrates from historical data
- Match current team to nearest archetype
- Use archetype winrate as composition signal
- Implementation: add archetype clustering to stats generation

**Add to `PredictionConfig`:**
```cpp
bool use_archetype_composition = false;
```

**Implementation Priority:**
1. Fix smoothing mismatch (Issue 2) - trivial, high impact
2. Add sample-weighted aggregation (Issue 1) - medium effort, high impact
3. Add decorrelated scoring mode (Issue 3) - medium effort, medium impact
4. Add draft-aware scoring (Issue 5) - medium effort, medium impact
5. Add hierarchical smoothing (Issue 4) - high effort, medium impact
6. Add archetype composition (Issue 6) - high effort, unknown impact

## Implementation Results

All four structural fixes were implemented and evaluated with 10,000 matches each. Results show **no meaningful improvement**:

### Phase 1: Smoothing Mismatch Fix
- **Change**: `confidence_prior_samples` 100 → 10
- **Result**: 63.5% accuracy (+0.05%), Brier 0.2226 (-0.0004), Log-loss 0.6352 (-0.0010)
- **Impact**: Minimal - expected ~3.5× variance increase did not translate to prediction improvement

### Phase 2: Sample-Weighted Aggregation
- **Change**: Weight pairwise winrates by sample count instead of flat average
- **Synergy-only**: 63.6% accuracy (+0.1%), Brier 0.2226, Log-loss 0.6352
- **Counter-only**: 63.5% accuracy (no change), Brier 0.2226, Log-loss 0.6352
- **Both enabled**: 63.6% accuracy (+0.1%), Brier 0.2226, Log-loss 0.6352
- **Impact**: Minimal - high-sample pairs get more influence but overall prediction unchanged

### Phase 3: Decorrelated Scoring
- **Change**: Use residuals (synergy - base, counter - base) to remove structural correlation
- **Result**: 63.5% accuracy (identical to baseline), Brier 0.2226, Log-loss 0.6352
- **Impact**: Zero - decorrelation did not improve predictions

### Phase 4: Draft-Aware Scoring
- **Change**: Position-based weight adjustment (early picks: higher base weight, late picks: higher counter weight)
- **Position 1 (early)**: 63.4% accuracy (-0.1%), Brier 0.2232 (+0.0006), Log-loss 0.6368 (+0.0016)
- **Position 5 (late)**: 63.7% accuracy (+0.2%), Brier 0.2219 (-0.0007), Log-loss 0.6336 (-0.0016)
- **Impact**: Mixed - late picks show slight improvement, early picks show slight degradation

## Conclusion

**None of the structural aggregation fixes produced meaningful improvement.** The root cause is deeper than aggregation methods:

1. **Signal variance is fundamentally limited** by champion balance (all champions winrate ~50%)
2. **Pairwise data is sparse** (average 15 samples per pair, below 20-sample threshold)
3. **Random draft context** - simulations use random draft order, not strategic counter-picking
4. **Missing strategic context** - no archetype modeling, no composition archetypes

The draft recommender's accuracy ceiling (~63.5%) appears to be a fundamental limit given the current data structure and champion balance. Further improvements require:

- **More data**: Increase sample count per pairwise matchup (need 10× more matches)
- **Strategic context**: Implement real sequential drafting with counter-pick decisions
- **Alternative signals**: Move beyond winrate-based signals to kit-derived strategic signals
- **Archetype modeling**: Define and compute composition archetype winrates

The implemented aggregation modes (sample-weighted, decorrelated, draft-aware) are now available as config options for future use if data quality improves or strategic context is added.

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
