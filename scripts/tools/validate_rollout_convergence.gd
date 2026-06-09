extends SceneTree

## Rollout convergence validation for pick recommendations.
##
## Tests whether 100 rollouts per candidate provides stable rankings by
## comparing rankings and regret across different rollout counts.

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

const TEAM_SIZE: int = 5
const DEFAULT_OUTPUT := "res://stats_output/rollout_convergence.csv"


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

	var state_count := maxi(1, int(_extract_argument("--states=", "50")))
	var rollout_counts_str := _extract_argument("--rollout-counts=", "10,25,50,100,200,500")
	var rollout_counts := _parse_rollout_counts(rollout_counts_str)
	var base_seed := int(_extract_argument("--base-seed=", "70000"))
	var stats_dir := _extract_argument("--stats-dir=", "res://stats_output")
	var output_path := _extract_argument("--output=", DEFAULT_OUTPUT)

	# Handle PowerShell argument splitting issue
	if rollout_counts.size() == 1 and rollout_counts[0] == 10:
		# PowerShell may have split the argument, try to reconstruct
		var full_counts_str := ""
		for a in OS.get_cmdline_user_args():
			var arg: String = str(a)
			if arg.begins_with("--rollout-counts="):
				full_counts_str = arg.substr("--rollout-counts=".length())
				break
		if full_counts_str != "":
			rollout_counts = _parse_rollout_counts(full_counts_str)

	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("validate_rollout_convergence: native simulation backend unavailable")
		quit(1)
		return

	var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	if champion_ids.size() < TEAM_SIZE * 2:
		push_error("validate_rollout_convergence: not enough champions for distinct 5v5 drafts")
		quit(1)
		return

	print("validate_rollout_convergence: states=%d rollout_counts=%s stats_dir=%s" % [
		state_count, rollout_counts_str, stats_dir
	])

	var csv_lines: Array[String] = [
		"depth,state_index,rollout_count,baseline_top1,baseline_score,rollout_top1,rollout_score,regret,top1_overlap,top3_overlap,spearman_corr"
	]

	# Aggregated metrics per depth
	var depth_metrics: Dictionary = {}

	for depth in range(TEAM_SIZE):
		print("Processing depth %d..." % depth)
		depth_metrics[depth] = {
			"top1_overlap": {},
			"top3_overlap": {},
			"spearman": {},
			"regret_variance": {},
			"score_cv": {}
		}

		for rollout_count in rollout_counts:
			depth_metrics[depth]["top1_overlap"][rollout_count] = 0
			depth_metrics[depth]["top3_overlap"][rollout_count] = 0
			depth_metrics[depth]["spearman"][rollout_count] = 0.0
			depth_metrics[depth]["regret_variance"][rollout_count] = 0.0
			depth_metrics[depth]["score_cv"][rollout_count] = 0.0

		for state_index in range(state_count):
			var state := _sample_state(champion_ids, depth, base_seed + state_index)
			var allies: Array[StringName] = state["allies"]
			var enemies: Array[StringName] = state["enemies"]
			var available: Array[StringName] = state["available"]

			var baseline := _baseline_recommendations(backend, allies, enemies, available, stats_dir)
			if baseline.is_empty():
				push_error("validate_rollout_convergence: baseline returned no recommendations for state %d" % state_index)
				quit(1)
				return

			var baseline_top1: StringName = baseline[0]
			var baseline_ranked := _baseline_rank_with_scores(backend, allies, enemies, available, stats_dir)
			var baseline_score: float = 0.0
			for row in baseline_ranked:
				if row["champion"] == baseline_top1:
					baseline_score = row["score"]
					break

			# Run rollout ranking at each rollout count
			var rollout_rankings := {}
			for rollout_count in rollout_counts:
				var ranked := _rollout_rank_candidates(
					backend, allies, enemies, available, rollout_count, base_seed, state_index, stats_dir
				)
				rollout_rankings[rollout_count] = ranked

			# Compare each rollout count to baseline and to 100-rollout reference
			for rollout_count in rollout_counts:
				var ranked: Array[Dictionary] = rollout_rankings[rollout_count]
				if ranked.is_empty():
					continue

				var rollout_top1: StringName = ranked[0]["champion"]
				var rollout_score: float = ranked[0]["score"]
				var regret: float = rollout_score - baseline_score
				var top1_overlap: bool = baseline_top1 == rollout_top1

				# Compare to 100-rollout reference if available
				var spearman_corr: float = 0.0
				var top3_overlap: int = 0
				if rollout_rankings.has(100) and rollout_count != 100:
					var ref_ranked: Array[Dictionary] = rollout_rankings[100]
					spearman_corr = _spearman_correlation(ranked, ref_ranked)
					var rollout_top3: Array[StringName] = []
					for i in range(mini(3, ranked.size())):
						rollout_top3.append(ranked[i]["champion"])
					var ref_top3: Array[StringName] = []
					for i in range(mini(3, ref_ranked.size())):
						ref_top3.append(ref_ranked[i]["champion"])
					top3_overlap = _overlap_count(rollout_top3, ref_top3)

				# Compute score coefficient of variation
				var score_cv := _compute_score_cv(ranked)

				csv_lines.append("%d,%d,%d,%s,%.6f,%s,%.6f,%.6f,%d,%d,%.4f" % [
					depth,
					state_index,
					rollout_count,
					String(baseline_top1),
					baseline_score,
					String(rollout_top1),
					rollout_score,
					regret,
					1 if top1_overlap else 0,
					top3_overlap,
					spearman_corr
				])

				# Aggregate metrics
				depth_metrics[depth]["top1_overlap"][rollout_count] += 1 if top1_overlap else 0
				depth_metrics[depth]["top3_overlap"][rollout_count] += top3_overlap
				depth_metrics[depth]["spearman"][rollout_count] += spearman_corr
				depth_metrics[depth]["regret_variance"][rollout_count] += regret * regret
				depth_metrics[depth]["score_cv"][rollout_count] += score_cv

			if (state_index + 1) % 10 == 0:
				print("  State %d/%d at depth %d" % [state_index + 1, state_count, depth])

	# Normalize aggregated metrics
	for depth in range(TEAM_SIZE):
		var n := float(state_count)
		for rollout_count in rollout_counts:
			depth_metrics[depth]["top1_overlap"][rollout_count] /= n
			depth_metrics[depth]["top3_overlap"][rollout_count] /= n
			depth_metrics[depth]["spearman"][rollout_count] /= n
			depth_metrics[depth]["regret_variance"][rollout_count] = sqrt(depth_metrics[depth]["regret_variance"][rollout_count] / n)
			depth_metrics[depth]["score_cv"][rollout_count] /= n

	var f := FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f == null:
		push_error("validate_rollout_convergence: could not open %s" % output_path)
		quit(1)
		return
	f.store_string("\n".join(csv_lines) + "\n")
	f.close()
	if backend.has_method("clear"):
		backend.call("clear")

	print("")
	print("================ ROLLOUT CONVERGENCE VALIDATION ================")
	print("states=%d rollout_counts=%s" % [state_count, rollout_counts_str])
	print("")
	print("Convergence Analysis (vs 100-rollout reference):")
	for depth in range(TEAM_SIZE):
		print("Depth %d:" % depth)
		for rollout_count in rollout_counts:
			if rollout_count == 100:
				continue
			print("  %d rollouts: top1_overlap=%.1f%% top3_overlap=%.1f%% spearman=%.3f regret_std=%.3f pp score_cv=%.3f" % [
				rollout_count,
				depth_metrics[depth]["top1_overlap"][rollout_count] * 100.0,
				depth_metrics[depth]["top3_overlap"][rollout_count] * 100.0,
				depth_metrics[depth]["spearman"][rollout_count],
				depth_metrics[depth]["regret_variance"][rollout_count] * 100.0,
				depth_metrics[depth]["score_cv"][rollout_count]
			])
	print("")
	print("Convergence Criteria:")
	print("  Top-1 overlap >= 90% between 100 and 200 rollouts")
	print("  Spearman correlation >= 0.95 between 100 and 200 rollouts")
	print("  Regret variance < 0.1 pp between 100 and 200 rollouts")
	print("  Score CV < 0.05 at 100 rollouts")
	print("")
	_check_convergence_criteria(depth_metrics, rollout_counts)
	print("CSV written to: %s" % output_path)
	print("=======================================================================")
	quit(0)


func _parse_rollout_counts(str: String) -> Array[int]:
	var result: Array[int] = []
	for part in str.split(","):
		result.append(int(part.strip_edges()))
	return result


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


func _baseline_recommendations(backend: RefCounted, allies: Array[StringName], enemies: Array[StringName], available: Array[StringName], stats_dir: String) -> Array[StringName]:
	var rows: Array = backend.get_draft_recommendations_with_breakdowns(
		allies, enemies, available, available.size(), stats_dir, 0.50, 0.25, 0.25
	)
	var result: Array[StringName] = []
	for row in rows:
		result.append(StringName(Dictionary(row).get("champion", "")))
	return result


func _baseline_rank_with_scores(backend: RefCounted, allies: Array[StringName], enemies: Array[StringName], available: Array[StringName], stats_dir: String) -> Array[Dictionary]:
	var rows: Array = backend.get_draft_recommendations_with_breakdowns(
		allies, enemies, available, available.size(), stats_dir, 0.50, 0.25, 0.25
	)
	var result: Array[Dictionary] = []
	for row in rows:
		result.append({
			"champion": StringName(Dictionary(row).get("champion", "")),
			"score": float(Dictionary(row).get("score", 0.5))
		})
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


func _spearman_correlation(a: Array[Dictionary], b: Array[Dictionary]) -> float:
	# Build rank maps
	var rank_a := {}
	var rank_b := {}
	for i in range(a.size()):
		rank_a[a[i]["champion"]] = i
	for i in range(b.size()):
		rank_b[b[i]["champion"]] = i

	# Compute correlation on common champions
	var common_champions := []
	for champ in rank_a:
		if rank_b.has(champ):
			common_champions.append(champ)

	if common_champions.size() < 2:
		return 0.0

	var n := float(common_champions.size())
	var sum_rank_diff_sq := 0.0
	for champ in common_champions:
		var diff := float(rank_a[champ]) - float(rank_b[champ])
		sum_rank_diff_sq += diff * diff

	# Spearman correlation: 1 - (6 * sum(d^2)) / (n * (n^2 - 1))
	return 1.0 - (6.0 * sum_rank_diff_sq) / (n * (n * n - 1.0))


func _compute_score_cv(ranked: Array[Dictionary]) -> float:
	if ranked.is_empty():
		return 0.0

	var scores := []
	for row in ranked:
		scores.append(row["score"])

	var n := float(scores.size())
	var sum := 0.0
	for s in scores:
		sum += s
	var mean := sum / n

	var variance := 0.0
	for s in scores:
		variance += (s - mean) * (s - mean)
	variance /= n

	var stddev := sqrt(variance)
	if mean == 0:
		return 0.0
	return stddev / mean


func _check_convergence_criteria(depth_metrics: Dictionary, rollout_counts: Array[int]) -> void:
	var converged: bool = true
	for depth in range(TEAM_SIZE):
		var top1_overlap: float = depth_metrics[depth]["top1_overlap"].get(200, 0.0)
		var spearman: float = depth_metrics[depth]["spearman"].get(200, 0.0)
		var regret_std: float = depth_metrics[depth]["regret_variance"].get(200, 0.0)
		var score_cv: float = depth_metrics[depth]["score_cv"].get(100, 0.0)

		var depth_converged: bool = true
		if top1_overlap < 0.9:
			print("  Depth %d: FAILED - Top-1 overlap %.1f%% < 90%%" % [depth, top1_overlap * 100.0])
			depth_converged = false
		if spearman < 0.95:
			print("  Depth %d: FAILED - Spearman %.3f < 0.95" % [depth, spearman])
			depth_converged = false
		if regret_std > 0.001:
			print("  Depth %d: FAILED - Regret std %.3f pp > 0.1 pp" % [depth, regret_std * 100.0])
			depth_converged = false
		if score_cv > 0.05:
			print("  Depth %d: FAILED - Score CV %.3f > 0.05" % [depth, score_cv])
			depth_converged = false

		if depth_converged:
			print("  Depth %d: PASSED - All convergence criteria met" % depth)
		else:
			converged = false

	if converged:
		print("Conclusion: 100 rollouts per candidate is SUFFICIENT for stable rankings")
	else:
		print("Conclusion: 100 rollouts per candidate is INSUFFICIENT - consider increasing rollout count")
