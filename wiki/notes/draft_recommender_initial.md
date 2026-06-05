# Draft recommender initial native implementation

`native/src/simulation/sim_draft_recommender.{hpp,cpp}` contains the initial teamfight-only draft recommendation module.

The module is split into three small C++ classes:

- `sim::draft::DraftStatsDatabase` loads `combat_stats.csv`, `matchup_with.csv`, and `matchup_vs.csv` from a stats directory and caches base, ally-synergy, and enemy-matchup winrates.
- `sim::draft::DraftEvaluator` scores one candidate champion from base winrate, average synergy with allies, and average performance versus enemies.
- `sim::draft::DraftRecommender` evaluates all available champions, sorts them by descending score, and prints ranked output.

The Godot-facing debug entrypoint is `TeamfightSimulationCore.debug_print_draft_recommendations(allies, enemies, available, top_n, stats_dir, base_weight, synergy_weight, matchup_weight)`. It defaults to `res://stats_output` and prints through `UtilityFunctions::print` only.

Missing base or matchup data falls back gracefully: missing base winrate uses `0.5`; missing ally/enemy matchup rows are skipped, and empty sample sets fall back to the candidate base winrate.
