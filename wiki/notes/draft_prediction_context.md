# Draft Prediction Context - Agent Briefing

## Core Problem

The draft recommender's older batch metric plateaued near 63.5% accuracy, but the Bayes-optimal ceiling is 75.4% (measured via `measure_draft_ceiling.gd`). The corrected verifier shows pairwise features contain more signal than the old 37.5% logistic result suggested.

## Investigation Summary

**Hypothesis tested:** Pairwise interaction data (base/synergy/counter winrates) contains sufficient signal to reach the ceiling.

**Method:** Created `verify_pairwise_signal.gd` to train logistic regression on pairwise features using per-comp true winrates from the ceiling experiment as labeled data.

**Corrected result:** The original 37.5% logistic result was a verifier artifact caused by unstandardized features and a file-order split. After deterministic shuffle + train-standardized features:
- Current pairwise heuristic: 69.0% all / 67.5% test
- Pairwise logistic: 77.5% all / 80.0% test
- Pairwise probability logistic: 77.5% all / 80.0% test

**Conclusion:** Pairwise winrates do contain useful composition signal. The immediate bottleneck is not pairwise feature absence; it is how to turn that signal into robust recommender behavior without overfitting small ceiling samples.

## Current Feature Space Limitations

**Current features:**
1. Historical pairwise winrates (base/synergy/counter) - useful; corrected pairwise logistic reached 80.0% test on the current shuffled split
2. Expanded mechanical signals - weak alone and overfit when combined in this small validation set
3. Role fingerprints - limited granularity

**Catalog data now extracted into `mechanical_signals.csv`:**
- Estimated DPS, effective HP, burst estimate, sustain/sec, CC/sec, attack range, max AOE radius, ability uptime

**Catalog data still not fully modeled:**
- 50+ effect kinds (damage, CC, mobility, sustain, defense, utility, advanced triggers)
- Champion stats: max_hp, attack_damage, attack_range, attack_speed, move_speed, armor, magic_resist, tenacity, life_steal, mana_cost, ability_cd, projectile_speed, projectile_radius
- Advanced mechanics: hp_threshold_damage_multiplier, distance_threshold_multiplier, target_status_multiplier, every_n_attacks_stun, stealth, summon_ally, redirect_damage, reflect_damage
- Effect parameters: radius, duration, damage_ratio, cooldown, mana cost, conditionals

## Relevant Files

**Core investigation tools:**
- `scripts/tools/measure_draft_ceiling.gd` - Measures Bayes ceiling by decoupling comp from sim seed
- `scripts/tools/verify_pairwise_signal.gd` - Tests whether pairwise features contain predictive signal
- `scripts/tools/analyze_signal_variance.gd` - Analyzes signal variance and correlations
- `scripts/tools/generate_draft_probe_signals.gd` - Generates simulation-derived probe features from fixed templates

**Data sources:**
- `scripts/simulation/champion_catalog.gd` - Champion definitions with effect trees and stats
- `scripts/simulation/champion_stats.gd` - Champion stat definitions
- `scripts/simulation/effect_spec.gd` - Effect kind definitions
- `res://stats_output/draft_ceiling.csv` - Per-comp true winrates (output from ceiling experiment)
- `res://stats_output/combat_stats.csv` - Historical base winrates
- `res://stats_output/matchup_with.csv` - Historical synergy winrates
- `res://stats_output/matchup_vs.csv` - Historical counter winrates

**Native recommender:**
- `native/src/simulation/sim_draft_recommender.cpp` - C++ draft recommender implementation
- `native/src/simulation/sim_draft_recommender.hpp` - Draft recommender interface

**Documentation:**
- `wiki/notes/signal_variance_analysis.md` - Full investigation history and results

## Revised Next Steps (Priority Order)

**1. Certified modern default**
- Complete-draft `predict_draft_winner` now defaults to the 5000-comp holdout-certified pairwise probability logistic model (`ScoringMode::CERTIFIED_PAIRWISE_PROBABILITY`)
- Certified validation: 76.1% test accuracy / MSE 0.0484 on `draft_ceiling_holdout_5000.csv`
- Runtime shape: logistic weights/means/stddevs are baked into native code, but feature inputs still come from the active `stats_dir` CSVs (`combat_stats.csv`, `matchup_with.csv`, `matchup_vs.csv`)
- Scope: complete-team win prediction is certified; incomplete-draft probabilities are allowed in the UI but are extrapolated; partial-draft pick recommendation ranking still uses the existing recommender scorer

**2. Archived/unused: expanded mechanical features**
- Latest verifier: mechanical-only 60.0% test; combined label model 67.5% test vs pairwise label 80.0% test
- Mechanical features overfit train and hurt test accuracy in this sample

**3. Archived/unused: simulation-derived probes**
- Smoke run passed functionality: 4 templates x 20 seeds x mirror wrote one row per champion and verifier reported probe feature sets
- Full run: 12 templates x 100 seeds x mirror
- Gate failed: pairwise label 80.0% test vs pairwise+probe label 60.0% test; pairwise+probe probability 72.5% test with worse MSE than pairwise probability
- 5000-comp non-mirror holdout: pairwise label 75.4% test; pairwise+probe label 76.8% test; pairwise probability 76.1% test / MSE 0.0484; pairwise+probe probability 76.6% test / MSE 0.0479
- Interpretation: probe vectors show mild incremental signal at larger scale, but label delta is +1.4 pp, below the +2 pp wiring gate

**4. Archived/unused: composition archetypes**
- Verifier now fits deterministic train-split k-means archetypes and train-only smoothed archetype matchup rates
- Gate failed: pairwise label 80.0% test vs pairwise+archetype label 75.0% test
- Archetype-only reached 65.0% test

**5. Draft-state context**
- Model sequential decision-making (counter-pick timing, synergy building)
- Requires draft simulation framework, not static comp evaluation

**6. Learned embeddings (exploratory)**
- Train embeddings from simulation trajectories (state → action → outcome)
- Requires trajectory collection and ML infrastructure

## Archived / Unused Alternatives

These paths are intentionally recorded but not active defaults:

- **Expanded mechanical features**: archived/unused for prediction. `mechanical_signals.csv` extraction and C++ loading exist, but recommender weights remain `0.0`. Latest gate failed: mechanical-only 60.0% test; combined label 67.5% test vs pairwise 80.0%.
- **Simulation-derived probes**: archived/unused for native recommender. Tooling and CSV generation exist, but probes are not wired into runtime prediction. 5000-comp holdout showed mild signal (`pairwise+probe` label 76.8% vs pairwise label 75.4%), but the +1.4 pp gain missed the +2 pp wiring gate.
- **Composition archetypes**: archived/unused. Validation-only k-means archetypes exist inside `verify_pairwise_signal.gd`; gate failed on the 200-comp split (`pairwise+archetype` 75.0% vs pairwise 80.0%).
- **Combined feature sets** (`combined`, `combined_all`, `mechanical_probe`, `pairwise_archetype`, `pairwise_probe`): verifier-only experiment sets. They remain useful for regression checks, but are not runtime defaults.
- **Draft-state context and learned embeddings**: future research ideas only; no active runtime implementation.

## Key Metrics

- Older batch baseline: 63.5% accuracy
- Current pairwise heuristic on `draft_ceiling.csv`: 69.0% all / 67.5% shuffled test
- Pairwise logistic after verifier fix: 77.5% all / 80.0% shuffled test
- Expanded mechanical gate: failed; combined label test delta vs pairwise label = -12.5 pp
- Archetype gate: failed; pairwise+archetype label test delta vs pairwise label = -5.0 pp
- Simulation probe gate: failed; pairwise+probe label test delta vs pairwise label = -20.0 pp
- Bayes ceiling: 75.4% accuracy (mirror mode, 200 comps x 100 seeds)
- Larger ceiling: 75.8% accuracy (mirror mode, 300 comps x 100 seeds, 60k decisive matches)
- Large verifier: pairwise label 76.7% test; pairwise+archetype label 76.7%; pairwise+probe label 65.0%; combined_all label 70.0%
- 5000-comp non-mirror holdout: pairwise label 75.4% test; pairwise probability 76.1% test / MSE 0.0484; pairwise+probe label 76.8% test; pairwise+probe probability 76.6% test / MSE 0.0479
- Native certified default: pairwise probability logistic (`ScoringMode::CERTIFIED_PAIRWISE_PROBABILITY`)
- Required runtime stats: fresh, sufficiently large `res://stats_output` with `combat_stats.csv`, `matchup_with.csv`, and `matchup_vs.csv`
- Correlation (current vs true p after latest stats regen): 0.5578

## Implementation Notes

- Run ceiling measurement: `.\run_godot.ps1 --measure-draft-ceiling -- --num-comps=200 --seeds-per-comp=100 --mirror --ceiling-output=res://stats_output/draft_ceiling.csv`
- Run larger ceiling measurement: `$env:RUN_GODOT_TIMEOUT_SECONDS='1800'; .\run_godot.ps1 --measure-draft-ceiling -- --num-comps=300 --seeds-per-comp=100 --mirror --ceiling-output=res://stats_output/draft_ceiling_large.csv`
- Run verification: `.\run_godot.ps1 --verify-pairwise-signal -- --ceiling-input=res://stats_output/draft_ceiling.csv --stats-dir=res://stats_output --output=res://stats_output/pairwise_verification.csv`
- Run larger verification: `$env:RUN_GODOT_TIMEOUT_SECONDS='700'; .\run_godot.ps1 --verify-pairwise-signal -- --ceiling-input=res://stats_output/draft_ceiling_large.csv --stats-dir=res://stats_output --probe-input=res://stats_output/draft_probe_signals.csv --output=res://stats_output/pairwise_verification_large.csv`
- Run 5000-comp holdout verification: `$env:RUN_GODOT_TIMEOUT_SECONDS='2400'; .\run_godot.ps1 --verify-pairwise-signal -- --ceiling-input=res://stats_output/draft_ceiling_holdout_5000.csv --stats-dir=res://stats_output --probe-input=res://stats_output/draft_probe_signals.csv "--feature-sets=pairwise,pairwise_probe" --include-overfit-sanity=false --output=res://stats_output/pairwise_verification_holdout_5000.csv`
- Run smoke probe: `.\run_godot.ps1 --generate-draft-probe-signals -- --templates=4 --seeds-per-template=20 --mirror --output=res://stats_output/draft_probe_signals_smoke.csv`
- Run full probe: `.\run_godot.ps1 --generate-draft-probe-signals -- --templates=12 --seeds-per-template=100 --mirror --output=res://stats_output/draft_probe_signals.csv`
- Run archetype validation through verifier: `.\run_godot.ps1 --verify-pairwise-signal -- --ceiling-input=res://stats_output/draft_ceiling.csv --stats-dir=res://stats_output --probe-input=res://stats_output/draft_probe_signals.csv --output=res://stats_output/pairwise_verification.csv`
- Mechanical signals are extracted in `analyze_signal_variance.gd` via `_load_mechanical_signals()` and written to `mechanical_signals.csv` for C++ loading
- C++ recommender loads mechanical signals via `_load_mechanical_signals()` in `sim_draft_recommender.cpp`

## AGENTS.md Guidelines

- Read `wiki/README.md` and `wiki/notes/native_agent_guide.md` before native C++ work
- Use wiki before modifying code
- Make smallest complete change that resolves request
- Do not modify unrelated code
- No speculative changes
