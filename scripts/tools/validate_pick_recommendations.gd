extends SceneTree

## Validation-only rollout scorer for draft pick recommendations.
##
## For each sampled partial draft state, compares the current recommender's top
## pick against a rollout oracle that completes each candidate to 5v5 and scores
## those completions with predict_draft_winner's certified default.

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const DraftAiConfigScript := preload("res://scripts/tools/draft_ai_config.gd")

const TEAM_SIZE: int = 5
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
	var draft_depth := clampi(int(_extract_argument("--draft-depth=", "4")), 0, TEAM_SIZE - 1)
	var base_seed := int(_extract_argument("--base-seed=", "70000"))
	var stats_dir := _extract_argument("--stats-dir=", "res://model_stats/certified_pairwise_testing_250k")
	var config_path := _extract_argument("--config-path=", DraftAiConfigScript.DEFAULT_CONFIG_PATH)
	var output_path := _extract_argument("--output=", DEFAULT_OUTPUT)

	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("validate_pick_recommendations: native simulation backend unavailable")
		quit(1)
		return

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	if champion_ids.size() < TEAM_SIZE * 2:
		push_error("validate_pick_recommendations: not enough champions for distinct 5v5 drafts")
		quit(1)
		return

	print("validate_pick_recommendations: states=%d rollouts/candidate=%d draft_depth=%d stats_dir=%s" % [
		state_count, rollouts_per_candidate, draft_depth, stats_dir
	])

	var csv_lines: Array[String] = [
		"state_index,picking_team,allies,enemies,available_n,baseline_top1,baseline_top1_rollout,rollout_top1,rollout_top1_score,regret,baseline_top3_contains_rollout_top1,top1_overlap,top3_overlap_count"
	]
	var total_regret: float = 0.0
	var total_baseline_score: float = 0.0
	var total_rollout_score: float = 0.0
	var top1_overlap_count: int = 0
	var top3_contains_count: int = 0
	var top3_overlap_total: int = 0

	for state_index in range(state_count):
		var state := _sample_state(champion_ids, draft_depth, base_seed + state_index)
		var allies: Array[StringName] = state["allies"]
		var enemies: Array[StringName] = state["enemies"]
		var available: Array[StringName] = state["available"]
		var picking_team: String = state["picking_team"]

		var baseline := _baseline_recommendations(backend, allies, enemies, available, stats_dir, config_path)
		if baseline.is_empty():
			push_error("validate_pick_recommendations: baseline returned no recommendations for state %d" % state_index)
			quit(1)
			return

		var rollout_ranked := _rollout_rank_candidates(
			backend, allies, enemies, available, rollouts_per_candidate, base_seed, state_index, stats_dir
		)
		if rollout_ranked.is_empty():
			push_error("validate_pick_recommendations: rollout returned no recommendations for state %d" % state_index)
			quit(1)
			return

		var baseline_top1: StringName = baseline[0]
		var baseline_top3: Array[StringName] = baseline.slice(0, mini(3, baseline.size()))
		var rollout_top1: StringName = rollout_ranked[0]["champion"]
		var rollout_top3: Array[StringName] = []
		for i in range(mini(3, rollout_ranked.size())):
			rollout_top3.append(rollout_ranked[i]["champion"])

		var rollout_scores: Dictionary = {}
		for row in rollout_ranked:
			rollout_scores[row["champion"]] = row["score"]
		var baseline_score: float = float(rollout_scores.get(baseline_top1, 0.5))
		var rollout_score: float = float(rollout_ranked[0]["score"])
		var regret: float = rollout_score - baseline_score
		var top1_overlap := baseline_top1 == rollout_top1
		var top3_contains := rollout_top1 in baseline_top3
		var top3_overlap := _overlap_count(baseline_top3, rollout_top3)

		total_regret += regret
		total_baseline_score += baseline_score
		total_rollout_score += rollout_score
		top1_overlap_count += 1 if top1_overlap else 0
		top3_contains_count += 1 if top3_contains else 0
		top3_overlap_total += top3_overlap

		csv_lines.append("%d,%s,%s,%s,%d,%s,%.6f,%s,%.6f,%.6f,%d,%d,%d" % [
			state_index,
			picking_team,
			"|".join(allies),
			"|".join(enemies),
			available.size(),
			String(baseline_top1),
			baseline_score,
			String(rollout_top1),
			rollout_score,
			regret,
			1 if top3_contains else 0,
			1 if top1_overlap else 0,
			top3_overlap,
		])

	var f := FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f == null:
		push_error("validate_pick_recommendations: could not open %s" % output_path)
		quit(1)
		return
	f.store_string("\n".join(csv_lines) + "\n")
	f.close()
	if backend.has_method("clear"):
		backend.call("clear")

	var n: float = float(state_count)
	var avg_baseline := total_baseline_score / n
	var avg_rollout := total_rollout_score / n
	var avg_regret := total_regret / n
	print("")
	print("================ PICK RECOMMENDATION ROLLOUT VALIDATION ================")
	print("states=%d rollouts/candidate=%d draft_depth=%d" % [state_count, rollouts_per_candidate, draft_depth])
	print("baseline_top1_expected_win=%.2f%%" % (avg_baseline * 100.0))
	print("rollout_top1_expected_win=%.2f%%" % (avg_rollout * 100.0))
	print("avg_regret=%.2f pp" % (avg_regret * 100.0))
	print("top1_overlap=%.1f%%" % (float(top1_overlap_count) / n * 100.0))
	print("baseline_top3_contains_rollout_top1=%.1f%%" % (float(top3_contains_count) / n * 100.0))
	print("avg_top3_overlap_count=%.2f" % (float(top3_overlap_total) / n))
	print("recommendation: %s" % _recommendation(avg_regret))
	print("CSV written to: %s" % output_path)
	print("=======================================================================")
	quit(0)


func _sample_state(champion_ids: Array[StringName], draft_depth: int, seed: int) -> Dictionary:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	var pool := champion_ids.duplicate()
	_shuffle(pool, rng)
	var player: Array[StringName] = []
	var enemy: Array[StringName] = []
	for i in range(draft_depth):
		player.append(pool.pop_back())
		enemy.append(pool.pop_back())
	var picking_player := (seed % 2) == 0
	return {
		"allies": player if picking_player else enemy,
		"enemies": enemy if picking_player else player,
		"available": pool,
		"picking_team": "player" if picking_player else "enemy",
	}


func _baseline_recommendations(backend: RefCounted, allies: Array[StringName], enemies: Array[StringName], available: Array[StringName], stats_dir: String, config_path: String) -> Array[StringName]:
	var rows: Array = backend.get_draft_recommendations_with_breakdowns(
		allies, enemies, available, available.size(), stats_dir, 0.50, 0.25, 0.25, 0, 0.7, 0.4, config_path
	)
	var result: Array[StringName] = []
	for row in rows:
		result.append(StringName(Dictionary(row).get("champion", "")))
	return result


func _rollout_rank_candidates(backend: RefCounted, allies: Array[StringName], enemies: Array[StringName], available: Array[StringName], rollouts_per_candidate: int, base_seed: int, state_index: int, stats_dir: String) -> Array[Dictionary]:
	var ranked: Array[Dictionary] = []
	for candidate_index in range(available.size()):
		var candidate: StringName = available[candidate_index]
		var sum: float = 0.0
		for rollout_index in range(rollouts_per_candidate):
			var completion := _complete_with_candidate(
				allies, enemies, available, candidate, base_seed + state_index * 100000 + candidate_index * 1000 + rollout_index
			)
			var prediction: Dictionary = backend.predict_draft_winner(
				completion["allies"], completion["enemies"], stats_dir
			)
			sum += float(prediction.get("team1_prob", 0.5))
		ranked.append({"champion": candidate, "score": sum / float(rollouts_per_candidate)})
	ranked.sort_custom(func(a, b):
		if is_equal_approx(a["score"], b["score"]):
			return String(a["champion"]) < String(b["champion"])
		return a["score"] > b["score"]
	)
	return ranked


func _complete_with_candidate(allies: Array[StringName], enemies: Array[StringName], available: Array[StringName], candidate: StringName, seed: int) -> Dictionary:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	var completed_allies := allies.duplicate()
	var completed_enemies := enemies.duplicate()
	completed_allies.append(candidate)
	var pool: Array[StringName] = []
	for id in available:
		if id != candidate:
			pool.append(id)
	_shuffle(pool, rng)
	while completed_allies.size() < TEAM_SIZE and not pool.is_empty():
		completed_allies.append(pool.pop_back())
	while completed_enemies.size() < TEAM_SIZE and not pool.is_empty():
		completed_enemies.append(pool.pop_back())
	return {"allies": completed_allies, "enemies": completed_enemies}


func _shuffle(values: Array, rng: RandomNumberGenerator) -> void:
	for i in range(values.size() - 1, 0, -1):
		var j := rng.randi_range(0, i)
		var tmp = values[i]
		values[i] = values[j]
		values[j] = tmp


func _overlap_count(a: Array[StringName], b: Array[StringName]) -> int:
	var count := 0
	for item in a:
		if item in b:
			count += 1
	return count


func _recommendation(avg_regret: float) -> String:
	if avg_regret >= 0.01:
		return "rollout scorer clears +1.0 pp gate; plan runtime wiring"
	return "keep current recommender; archive rollout scorer unless follow-up validation improves"
