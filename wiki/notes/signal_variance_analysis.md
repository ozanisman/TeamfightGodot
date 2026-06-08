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

## Current Findings (10,000 matches)
**Signal Variance:**
- Base winrate: 0.0477 (medium) - meaningful differentiation
- Synergy: 0.0098 (low) - nearly flat
- Counter: 0.0098 (low) - nearly flat

**Signal Correlations:**
- Base vs Synergy: 1.0 (perfect redundancy)
- Base vs Counter: 1.0 (perfect redundancy)
- Synergy vs Counter: 1.0 (perfect redundancy)

**Flat Profiles:** 20/26 champions (77%) have all signals within ±0.05 of 0.5

**Conclusion:** Historical signals (base/synergy/counter winrates) provide minimal differentiation. Synergy and counter signals are perfectly correlated with base winrate and have extremely low variance. The draft recommender is essentially operating on a single signal with minimal champion differentiation.

## Next Steps (Priority Order)
1. **Reduce Bayesian smoothing** - Lower k from 100 to 10-20 in `analyze_signal_variance.gd` (line 17) to see if real differences emerge. This is the easiest change and quick to test.

2. **Increase sample size** - Generate 50,000+ matches for more reliable pairwise statistics.

3. **Add draft-state awareness** - Implement signals that depend on current draft state (role balance, team composition needs, counter-pick availability). This would be a new signal type not based on historical data.

4. **Improve champion balance** - Review champion kits to create more meaningful power differentiation.

5. **Add mechanical counter signals** - Use champion ability interactions (CC vs immobile, burst vs sustain) instead of historical winrates.

## Parameters
- `SMOOTHING_K`: 100.0 (Bayesian smoothing parameter)
- `PRIOR_WINRATE`: 0.5 (Prior winrate for Bayesian smoothing)
- `FLAT_PROFILE_THRESHOLD`: 0.05 (Signals within ±0.05 of 0.5 are considered flat)
- Minimum samples: 20 (Champions with fewer than 20 matches are filtered out)
