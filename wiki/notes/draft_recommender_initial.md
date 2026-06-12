# Draft recommender initial native implementation

`native/src/simulation/sim_draft_recommender.{hpp,cpp}` contains the initial teamfight-only draft recommendation module.

The module is split into three small C++ classes:

- `sim::draft::DraftStatsDatabase` loads `combat_stats.csv`, `matchup_with.csv`, and `matchup_vs.csv` from a stats directory and caches base, ally-synergy, and enemy-counter winrates.
- `sim::draft::DraftEvaluator` scores one candidate champion from base winrate, average synergy with allies, and average performance versus enemies. `evaluate_candidate(..., EvalDebug*)` returns the final score and optionally fills base/synergy/counter/final breakdown fields.
- `sim::draft::DraftRecommender` evaluates all available champions, sorts them by descending score, and prints ranked output. When constructed with debug mode enabled, printed recommendations use a multi-line per-champion score decomposition.

The Godot-facing debug entrypoint is `TeamfightSimulationCore.debug_print_draft_recommendations(allies, enemies, available, top_n, stats_dir, base_weight, synergy_weight, counter_weight, debug_mode)`. It defaults to `res://stats_output` and prints through `UtilityFunctions::print` only.

Batch observability uses `TeamfightSimulationCore.run_debug_draft_evaluation_batch(allies, enemies, available, num_runs, stats_dir, base_weight, synergy_weight, counter_weight)`. The batch helper deterministically rotates available champions into empty ally/enemy slots and prints full debug breakdowns per generated state.

Behavioral validation lives in `scripts/tools/run_draft_recommender_tests.gd`. The script defines baseline, enemy-heavy, and ally-synergy fixed draft states, calls native debug recommendation APIs, then reruns one case with shifted weights and prints `UNCHANGED` or `CHANGED` for the top-3 result set.

Missing base or counter data falls back gracefully: missing base winrate uses `0.5`; missing ally/enemy counter rows are skipped, and empty sample sets fall back to the candidate base winrate.

## Historical update: LOGIT default (DEPRECATED), ADDITIVE/MULTIPLICATIVE (DEPRECATED), smoothing_k tuning, evaluation metric change

**LOGIT scoring was the historical recommendation default (DEPRECATED).** `PredictionConfig::scoring_mode` previously used `ScoringMode::LOGIT` for partial-draft recommendation ranking. LOGIT has been deprecated and replaced with `DRAFT_AWARE_PAIRWISE_PROBABILITY` (scoring_mode=1) as the default for all draft prediction tasks.

**ADDITIVE and MULTIPLICATIVE scoring modes (DEPRECATED).** These legacy scoring modes were used as baselines for comparison but have been removed from the codebase. ADDITIVE had poor stability characteristics and MULTIPLICATIVE was rejected for similar reasons. Only `CERTIFIED_PAIRWISE_PROBABILITY` (scoring_mode=0) and `DRAFT_AWARE_PAIRWISE_PROBABILITY` (scoring_mode=1) remain.

**smoothing_k reverted to 100.** `DraftConfig::smoothing_k` was temporarily raised to `500.0` to create differentiation in CONFIDENCE_WEIGHTED smoothing, but validation showed k=500 produced 30% worse mean margins with no Top-3 stability gain over LEGACY. Root cause: pulling scores toward 0.5 compresses candidate separation, which hurts the ranking stability we measure. k=100 makes CONFIDENCE_WEIGHTED and LEGACY effectively identical on this dataset (w≈0.95 for all rows at 600–2500 samples), which is the correct outcome — CW smoothing provides no benefit for this data distribution.

**Primary evaluation metric: Top-3 stability.** Score range and mean margin are no longer acceptance criteria. The single criterion is Top-3 stability improvement ≥10pp vs ADDITIVE baseline. Post-fix result: LOGIT aggregated Top-3 stability 79.83% vs ADDITIVE 62.31% (+17.52pp absolute; 28.12% relative improvement, well above 10pp threshold). Synergy context remains the weakest scenario (LOGIT 73% vs ADDITIVE 40%).

**Signal influence analysis expanded.** The test script now loops over all available champions per test case (not just `available[0]`) to expose per-candidate synergy/matchup deltas for diagnosing synergy_context instability.

## Update: certified complete-draft prediction default

`predict_draft_winner` now defaults to `ScoringMode::CERTIFIED_PAIRWISE_PROBABILITY` for complete-draft winner prediction. This mode uses the 5000-comp holdout-certified pairwise probability logistic model from `wiki/notes/draft_prediction_context.md` (76.1% test accuracy, MSE 0.0484). Its logistic weights are baked into native code, while the base/synergy/counter feature values still come from the active stats directory (`combat_stats.csv`, `matchup_with.csv`, `matchup_vs.csv`). The `LOGIT` scorer has been deprecated and removed from the codebase.

## Update: relationship-count tiebreaker

**Sort tiebreaker changed from alphabetical to relationship count.** `DraftRecommender::recommend` previously broke score ties by champion name (arbitrary). Replaced with: when two candidates score within 0.005 of each other, the candidate with more total data relationships (`synergy_relationships + counter_relationships`) ranks higher. Champions backed by more data are more reliable picks; alphabetical order was epistemically meaningless. `DraftEvaluation` already tracked both counts; no struct changes needed. Final fallback remains alphabetical for exact relationship-count ties.

## Update: strength-weighted aggregation investigated and abandoned

**Hypothesis tested**: replace the flat average of pairwise synergy/counter winrates with a
strength-weighted average (`weight_i = |smoothed_winrate_i − 0.5|`), so a strong counter/synergy
pairing dominates over neutral ones, hoping this would break up the draft-prediction accuracy
report's uniform ~38% miss-rate / ~60% confidence-when-wrong "hotspot" clustering across
unrelated champion archetypes (the signature of a champion-agnostic scoring function).

**Result: abandoned as a documented dead end.** Three weight-formula variants were measured via
full 10,000-match held-out accuracy runs against `stats_output_baseline` — linear `|wr−0.5|`,
sample-confidence-scaled (`× samples/(samples+confidence_prior_samples)`), and `sqrt(|wr−0.5|)`.
All three were flat-or-worse on accuracy/Brier/log-loss (the linear variant made the model
measurably overconfident; sqrt halved that damage but still trailed baseline). Critically, **none
moved hotspot/differentiation spread** — it stayed at 0.5–0.7pp across baseline and all variants,
indistinguishable from noise. Root cause: changing how pairwise signals *aggregate* into a
team-level value doesn't change the *global, role-agnostic weights* (`base_weight=0.50`,
`synergy_weight=0.25`, `matchup_weight=0.25`) applied uniformly to every champion — and that
global blend is what produces the uniform hotspot clustering. (Aside: the sample-confidence
variant produced bit-identical numbers to the plain linear one, because every pairing in
`stats_output_baseline` has 5,900–7,900+ samples, making the shrinkage factor a near-constant
~0.984–0.988 that cancels out of the weighted-average ratio — there's no small-sample noise in
this dataset for that knob to act on.)

**What was kept**: the pairwise synergy/counter aggregation loops — previously duplicated between
`DraftEvaluator::evaluate` and `DraftStatsDatabase::calculate_team_score` (a divergence risk) —
were consolidated into one shared `aggregate_relationship_signal` helper, with the weight formula
reverted to plain flat averaging. Verified bit-for-bit identical to the pre-change baseline
(6277/10000 accuracy, Brier 0.2241, log loss 0.6385, identical hotspot table), so the consolidation
is a pure code-quality win with zero numeric impact. Investigation then proceeded to **role-aware
weighting** (below).

## Update: data-derived role-aware weighting investigated and abandoned

**Hypothesis tested**: instead of blending `base/synergy/matchup/composition` through the same
global weights for every champion, multiply each weight by a per-*role* multiplier derived from how
strongly that signal type actually correlates with real match outcomes for that role (e.g. if
`synergy` is unusually predictive for supports, scale `synergy_weight` up for support champions).
Implemented as an optional `role_signal_weights.csv` (load-if-present, neutral-fallback — same
pattern as `role_combinations.csv`), fed by a new offline correlation-analysis tool.

**Blocker found and fixed first**: the first checkpoint run came back bit-for-bit identical to
baseline. Root cause: `combat_stats.csv` had no `role` column, so the native
`get_champion_role()` always returned empty and multipliers always fell back to neutral — a
structural no-op, not a bug in the multiplier-application code. Fixed by (a) adding a `role` column
to `_build_combat_csv()` (sourced from the aggregator's existing `_role_by_hero` map) and (b)
discovering that `preload_roles()` was only ever wired to *per-worker* aggregators, never the
top-level merging one that writes `combat_stats.csv` — added that call in
`stats_simulation_csv_generator.gd::run()`. Both fixes are small and additive; regenerated fresh
held-out snapshots confirmed roles now resolve correctly end-to-end.

**Result: abandoned as a documented dead end**, same shape as the strength-weighted-aggregation
finding above — but reached this time with a fully *functional* mechanism (confirmed via A/B: a
control run with neutral multipliers vs. a treatment run with real data-derived multipliers,
against identical out-of-sample data). Accuracy/Brier/log-loss nudged marginally better
(63.6%→63.7%, 0.2226→0.2220, 0.6354→0.6339 — within noise), but **calibration measurably
worsened**: the model went from well-calibrated/slightly-underconfident to overconfident in every
bucket, expressing high (70%+) confidence far more often (n=1951→3280) while being *less* often
right when it does. Critically, **hotspot/differentiation spread stayed flat** — 0.4–0.5pp by
top-archetype, ~2.0pp by-role, in both control and treatment — the same uniform ~36–38% miss-rate
clustering across unrelated champion archetypes that this entire investigation was trying to break
up persisted unchanged.

**Conclusion**: two independently-implemented and correctly-measured structural changes (Phase 1's
strength-weighted aggregation and Phase 2's role-aware weighting) both leave the
hotspot/differentiation spread untouched. The bottleneck isn't *how* pairwise signals aggregate or
*how* weights are distributed across roles — it's more likely that base/synergy/matchup/composition
carry similarly limited, role-agnostic predictive power in this dataset. Neither mechanism is
recommended as a default; both remain available as optional, off-by-default data files
(`aggregate_relationship_signal` flat-averages by default; `role_signal_weights.csv` is
load-if-present with a neutral fallback). Full data: `reports/phase1_*_vs_baseline_comparison.txt`,
`reports/draft_prediction_phase1_reverted.txt`, `reports/draft_prediction_phase2_control.txt`,
`reports/draft_prediction_phase2_treatment.txt`.

## Update: investigation closed — neither structural change moves the needle

**Final call**: across baseline → Phase 1 (sqrt strength-weighting) → Phase 2 (role-aware
multipliers), accuracy/Brier/log-loss all stayed within noise of each other and the
hotspot/by-role miss-rate spread never moved (~0.5pp top-archetype, ~2.0–2.3pp by-role, in *every*
configuration tested). Both mechanisms are kept as off-by-default optional infrastructure
(`aggregate_relationship_signal` helper; `role_signal_weights.csv` loader) rather than promoted to
defaults — they're real, working, measured dead ends, not bugs to fix later.

**Net positive side effect**: fixing Phase 2's blocker added a `role` column to `combat_stats.csv`
(`stats_csv_aggregator.gd::_build_combat_csv`, sourced from `_role_by_hero`) and wired
`preload_roles()` onto the top-level merging aggregator (`stats_simulation_csv_generator.gd::run`)
— both were silently missing before, which also meant `get_champion_role()` and
`composition_winrate_for()` (the latter gated by `composition_weight`, currently 0.0) were dormant
for *all* real generated data, not just this experiment's new code path. Future role- or
composition-aware work now has working data to build on.

**Where the bottleneck likely actually is**: not aggregation, not weighting — probably the
underlying base/synergy/matchup/composition signals themselves don't vary enough per champion to be
differentiating. That's the hypothesis worth testing next, if this is revisited.
