# Draft recommender initial native implementation

`native/src/simulation/sim_draft_recommender.{hpp,cpp}` contains the initial teamfight-only draft recommendation module.

The module is split into three small C++ classes:

- `sim::draft::DraftStatsDatabase` loads `combat_stats.csv`, `matchup_with.csv`, and `matchup_vs.csv` from a stats directory and caches base, ally-synergy, and enemy-matchup winrates.
- `sim::draft::DraftEvaluator` scores one candidate champion from base winrate, average synergy with allies, and average performance versus enemies. `evaluate_candidate(..., EvalDebug*)` returns the final score and optionally fills base/synergy/matchup/final breakdown fields.
- `sim::draft::DraftRecommender` evaluates all available champions, sorts them by descending score, and prints ranked output. When constructed with debug mode enabled, printed recommendations use a multi-line per-champion score decomposition.

The Godot-facing debug entrypoint is `TeamfightSimulationCore.debug_print_draft_recommendations(allies, enemies, available, top_n, stats_dir, base_weight, synergy_weight, matchup_weight, debug_mode)`. It defaults to `res://stats_output` and prints through `UtilityFunctions::print` only.

Batch observability uses `TeamfightSimulationCore.run_debug_draft_evaluation_batch(allies, enemies, available, num_runs, stats_dir, base_weight, synergy_weight, matchup_weight)`. The batch helper deterministically rotates available champions into empty ally/enemy slots and prints full debug breakdowns per generated state.

Behavioral validation lives in `scripts/tools/run_draft_recommender_tests.gd`. The script defines baseline, enemy-heavy, and ally-synergy fixed draft states, calls native debug recommendation APIs, then reruns one case with shifted weights and prints `UNCHANGED` or `CHANGED` for the top-3 result set.

Missing base or matchup data falls back gracefully: missing base winrate uses `0.5`; missing ally/enemy matchup rows are skipped, and empty sample sets fall back to the candidate base winrate.
