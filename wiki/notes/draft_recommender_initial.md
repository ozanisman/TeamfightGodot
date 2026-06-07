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

## Update: LOGIT default, smoothing_k tuning, evaluation metric change

**LOGIT scoring adopted as default.** `DraftConfig::scoring_mode` now defaults to `ScoringMode::LOGIT`. In LOGIT mode, amplification factors are applied in logit space via `log(amplification_factor)` added to the logit before the sigmoid, not multiplied in probability space. This avoids the saturation bug present in earlier experiments where high base winrates (>0.6) produced near-zero amplification deltas regardless of synergy strength. Pre-fix LOGIT/ADDITIVE comparison results are invalid.

**smoothing_k reverted to 100.** `DraftConfig::smoothing_k` was temporarily raised to `500.0` to create differentiation in CONFIDENCE_WEIGHTED smoothing, but validation showed k=500 produced 30% worse mean margins with no Top-3 stability gain over LEGACY. Root cause: pulling scores toward 0.5 compresses candidate separation, which hurts the ranking stability we measure. k=100 makes CONFIDENCE_WEIGHTED and LEGACY effectively identical on this dataset (w≈0.95 for all rows at 600–2500 samples), which is the correct outcome — CW smoothing provides no benefit for this data distribution.

**Primary evaluation metric: Top-3 stability.** Score range and mean margin are no longer acceptance criteria. The single criterion is Top-3 stability improvement ≥10pp vs ADDITIVE baseline. Post-fix result: LOGIT aggregated Top-3 stability 79.83% vs ADDITIVE 62.31% (+17.52pp absolute; 28.12% relative improvement, well above 10pp threshold). Synergy context remains the weakest scenario (LOGIT 73% vs ADDITIVE 40%).

**Signal influence analysis expanded.** The test script now loops over all available champions per test case (not just `available[0]`) to expose per-candidate synergy/matchup deltas for diagnosing synergy_context instability.

## Update: relationship-count tiebreaker

**Sort tiebreaker changed from alphabetical to relationship count.** `DraftRecommender::recommend` previously broke score ties by champion name (arbitrary). Replaced with: when two candidates score within 0.005 of each other, the candidate with more total data relationships (`synergy_relationships + counter_relationships`) ranks higher. Champions backed by more data are more reliable picks; alphabetical order was epistemically meaningless. `DraftEvaluation` already tracked both counts; no struct changes needed. Final fallback remains alphabetical for exact relationship-count ties.
