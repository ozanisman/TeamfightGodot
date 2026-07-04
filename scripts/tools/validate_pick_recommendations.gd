extends SceneTree

## Validation-only rollout scorer for draft pick recommendations.

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const ValidatePickRecommendationsCoreScript := preload("res://scripts/tools/validate_pick_recommendations_core.gd")
const DraftAiConfigScript := preload("res://scripts/tools/draft_ai_config.gd")

const DEFAULT_OUTPUT := "res://model_stats/certified_pairwise_testing_250k/pick_recommendation_validation.csv"


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	ChampionCatalogScript.build_catalog()
	ChampionCatalogScript.build_role_configs()
	ChampionCatalogScript.build_passive_registry()
	ChampionCatalogScript.build_minion_catalog()
	ChampionCatalogScript.freeze_built_specs_for_worker_reuse()

	var state_count := maxi(1, int(_extract_argument("--states=", "25")))
	var rollouts_per_candidate := maxi(1, int(_extract_argument("--rollouts-per-candidate=", "20")))
	var draft_depth := clampi(int(_extract_argument("--draft-depth=", "4")), 0, ValidatePickRecommendationsCoreScript.TEAM_SIZE - 1)
	var base_seed := int(_extract_argument("--base-seed=", "70000"))
	var stats_dir := _extract_argument("--stats-dir=", "res://model_stats/certified_pairwise_testing_250k")
	var config_path := _extract_argument("--config-path=", DraftAiConfigScript.DEFAULT_CONFIG_PATH)
	var output_path := _extract_argument("--output=", DEFAULT_OUTPUT)

	print("validate_pick_recommendations: states=%d rollouts/candidate=%d draft_depth=%d stats_dir=%s" % [
		state_count, rollouts_per_candidate, draft_depth, stats_dir
	])

	var result: Dictionary = ValidatePickRecommendationsCoreScript.run({
		"states": state_count,
		"rollouts_per_candidate": rollouts_per_candidate,
		"draft_depth": draft_depth,
		"base_seed": base_seed,
		"stats_dir": stats_dir,
		"config_path": config_path,
		"output_path": output_path,
		"write_csv": true,
	})

	if not result.get("ok", false):
		push_error("validate_pick_recommendations: %s" % result.get("error", "failed"))
		quit(1)
		return

	var mean_regret: float = float(result.get("mean_regret", 0.0))
	var mean_baseline: float = float(result.get("mean_baseline_score", 0.0))
	var mean_rollout: float = float(result.get("mean_rollout_score", 0.0))
	print("")
	print("================ PICK RECOMMENDATION ROLLOUT VALIDATION ================")
	print("states=%d rollouts/candidate=%d draft_depth=%d" % [state_count, rollouts_per_candidate, draft_depth])
	print("baseline_top1_expected_win=%.2f%%" % (mean_baseline * 100.0))
	print("rollout_top1_expected_win=%.2f%%" % (mean_rollout * 100.0))
	print("avg_regret=%.2f pp" % (mean_regret * 100.0))
	print("top1_overlap=%.1f%%" % (float(result.get("top1_overlap_rate", 0.0)) * 100.0))
	print("baseline_top3_contains_rollout_top1=%.1f%%" % (float(result.get("top3_contains_rate", 0.0)) * 100.0))
	print("avg_top3_overlap_count=%.2f" % float(result.get("mean_top3_overlap_count", 0.0)))
	print("recommendation: %s" % ValidatePickRecommendationsCoreScript.recommendation_for_mean_regret(mean_regret))
	print("CSV written to: %s" % output_path)
	print("=======================================================================")
	quit(0)
