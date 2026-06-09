extends SceneTree

## Verify draft-prediction feature signal against per-comp ceiling labels.
##
## This is a validation tool, not runtime recommender code. It compares the current
## pairwise heuristic against standardized logistic models over pairwise,
## mechanical, and combined feature sets.

const SMOOTHING_K: float = 10.0
const PRIOR_WINRATE: float = 0.5
const LEARNING_RATE: float = 0.05
const MAX_ITERATIONS: int = 5000
const CONVERGENCE_TOLERANCE: float = 1e-7
const L2_REGULARIZATION: float = 0.001
const TRAIN_FRACTION: float = 0.8
const SPLIT_SEED: int = 1337
const DEFAULT_SPLIT_REPEATS: int = 0
const REPEATED_SPLIT_FEATURE_SETS: Array[String] = [
	"pairwise",
	"pairwise_archetype",
	"pairwise_probe",
	"combined_all",
]
const ARCHETYPE_ITERATIONS: int = 25
const ARCHETYPE_SMOOTHING_K: float = 3.0
const MODEL_LABEL: String = "label"
const MODEL_PROBABILITY: String = "probability"
const MECHANICAL_COLUMNS: Array[String] = [
	"cc_score",
	"mobility_score",
	"sustain_score",
	"estimated_dps",
	"effective_hp",
	"burst_estimate",
	"sustain_per_sec",
	"cc_per_sec",
	"attack_range",
	"max_aoe_radius",
	"ability_uptime",
]


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _parse_feature_set_filter(raw: String) -> Array[String]:
	var result: Array[String] = []
	for part in raw.split(",", false):
		var feature_set := part.strip_edges()
		if not feature_set.is_empty():
			result.append(feature_set)
	return result


func _requests_archetype_features(feature_sets: Array[String]) -> bool:
	for feature_set in feature_sets:
		if feature_set.find("archetype") >= 0:
			return true
	return false


func _filter_feature_sets(available: Array[String], requested: Array[String]) -> Array[String]:
	var result: Array[String] = []
	for feature_set in requested:
		if feature_set in available:
			result.append(feature_set)
		else:
			print("verify_pairwise_signal: requested feature set unavailable: %s" % feature_set)
	return result


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	var ceiling_input := _extract_argument("--ceiling-input=", "res://stats_output/draft_ceiling.csv")
	var stats_dir := _extract_argument("--stats-dir=", "res://stats_output")
	var probe_input := _extract_argument("--probe-input=", stats_dir + "/draft_probe_signals.csv")
	var output_path := _extract_argument("--output=", "res://stats_output/pairwise_verification.csv")
	var archetype_clusters := maxi(2, int(_extract_argument("--archetype-clusters=", "6")))
	var split_repeats := maxi(0, int(_extract_argument("--split-repeats=", str(DEFAULT_SPLIT_REPEATS))))
	var requested_feature_sets := _parse_feature_set_filter(_extract_argument("--feature-sets=", ""))
	var include_overfit_sanity := _extract_argument("--include-overfit-sanity=", "true") != "false"

	print("verify_pairwise_signal: loading ceiling data from %s" % ceiling_input)
	var ceiling_data := _load_ceiling_csv(ceiling_input)
	if ceiling_data.is_empty():
		push_error("verify_pairwise_signal: failed to load ceiling data")
		quit(1)
		return

	print("verify_pairwise_signal: loading stats from %s" % stats_dir)
	var combat_stats := _load_combat_stats(stats_dir + "/combat_stats.csv")
	var synergy_stats := _load_matchup_stats(stats_dir + "/matchup_with.csv")
	var counter_stats := _load_matchup_stats(stats_dir + "/matchup_vs.csv")
	var mechanical_signals := _load_mechanical_signals(stats_dir + "/mechanical_signals.csv")
	var probe_signals := _load_probe_signals(probe_input)
	print("verify_pairwise_signal: loaded %d comps, %d champions, %d synergy maps, %d counter maps, %d mechanical rows, %d probe rows" % [
		ceiling_data.size(), combat_stats.size(), synergy_stats.size(), counter_stats.size(), mechanical_signals.size(), probe_signals.size()
	])

	var feature_data := _extract_features(ceiling_data, combat_stats, synergy_stats, counter_stats, mechanical_signals, probe_signals)
	var split := _split_indices(feature_data.size(), SPLIT_SEED)
	if requested_feature_sets.is_empty() or _requests_archetype_features(requested_feature_sets):
		_add_archetype_features(feature_data, split, archetype_clusters)
	var baseline := _evaluate_baselines(feature_data, split)
	var current_corr := _compute_correlation(feature_data, "current_pred", "true_p")

	var model_reports: Array[Dictionary] = []
	var feature_sets: Array[String] = ["pairwise", "mechanical", "combined"]
	if _feature_set_available(feature_data, "archetype"):
		feature_sets.append_array(["archetype", "pairwise_archetype"])
	if _feature_set_available(feature_data, "probe"):
		feature_sets.append_array(["probe", "pairwise_probe", "mechanical_probe", "combined_all"])
	if not requested_feature_sets.is_empty():
		feature_sets = _filter_feature_sets(feature_sets, requested_feature_sets)
	for feature_set in feature_sets:
		for target_mode in [MODEL_LABEL, MODEL_PROBABILITY]:
			model_reports.append(_train_and_evaluate(feature_data, split, feature_set, target_mode, false))
		if include_overfit_sanity:
			model_reports.append(_train_and_evaluate(feature_data, split, feature_set, MODEL_LABEL, true))
	var repeat_reports := _run_repeated_split_evaluation(feature_data, feature_sets, archetype_clusters, split_repeats)

	_write_results(output_path, feature_data, model_reports)
	_print_report(baseline, model_reports, repeat_reports, current_corr, output_path)
	quit(0)


func _load_ceiling_csv(path: String) -> Array:
	var result: Array = []
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		push_error("Failed to open %s" % path)
		return result
	var header: PackedStringArray = f.get_csv_line()
	var comp_index_col: int = header.find("comp_index")
	var team_a_col: int = header.find("team_a")
	var team_b_col: int = header.find("team_b")
	var p_col: int = header.find("p_team_a_win")
	var imbalance_col: int = header.find("abs_imbalance")
	if comp_index_col < 0 or team_a_col < 0 or team_b_col < 0 or p_col < 0:
		push_error("Missing required columns in ceiling CSV")
		f.close()
		return result
	while not f.eof_reached():
		var line: PackedStringArray = f.get_csv_line()
		if line.size() <= 1:
			continue
		result.append({
			"comp_index": int(line[comp_index_col]),
			"team_a": Array(line[team_a_col].split("|")),
			"team_b": Array(line[team_b_col].split("|")),
			"true_p": float(line[p_col]),
			"abs_imbalance": float(line[imbalance_col]) if imbalance_col >= 0 else 0.0,
		})
	f.close()
	return result


func _load_combat_stats(path: String) -> Dictionary:
	var result: Dictionary = {}
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		push_error("Failed to open %s" % path)
		return result
	var header: PackedStringArray = f.get_csv_line()
	var hero_col: int = header.find("hero")
	var winrate_col: int = header.find("win_rate")
	var total_games_col: int = header.find("total_games")
	if hero_col < 0 or winrate_col < 0 or total_games_col < 0:
		push_error("Missing required columns in combat_stats.csv")
		f.close()
		return result
	while not f.eof_reached():
		var line: PackedStringArray = f.get_csv_line()
		if line.size() <= 1:
			continue
		result[line[hero_col]] = {"winrate": float(line[winrate_col]), "samples": int(line[total_games_col])}
	f.close()
	return result


func _load_matchup_stats(path: String) -> Dictionary:
	var result: Dictionary = {}
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		push_error("Failed to open %s" % path)
		return result
	var header: PackedStringArray = f.get_csv_line()
	var champion_col: int = header.find("champion")
	var other_col: int = header.find("ally")
	if other_col < 0:
		other_col = header.find("opponent")
	var winrate_col: int = header.find("winrate")
	var wins_col: int = header.find("wins")
	var losses_col: int = header.find("losses")
	if champion_col < 0 or other_col < 0 or winrate_col < 0:
		push_error("Missing required columns in matchup CSV")
		f.close()
		return result
	while not f.eof_reached():
		var line: PackedStringArray = f.get_csv_line()
		if line.size() <= 1:
			continue
		var champion: String = line[champion_col]
		var other: String = line[other_col]
		var samples: int = int(line[wins_col]) + int(line[losses_col]) if wins_col >= 0 and losses_col >= 0 else 0
		if not result.has(champion):
			result[champion] = {}
		result[champion][other] = {"winrate": float(line[winrate_col]), "samples": samples}
	f.close()
	return result


func _load_mechanical_signals(path: String) -> Dictionary:
	var result: Dictionary = {}
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		print("verify_pairwise_signal: optional mechanical_signals.csv missing")
		return result
	var header: PackedStringArray = f.get_csv_line()
	var champion_col: int = header.find("champion")
	if champion_col < 0:
		f.close()
		return result
	while not f.eof_reached():
		var line: PackedStringArray = f.get_csv_line()
		if line.size() <= 1:
			continue
		var row: Dictionary = {}
		for col in MECHANICAL_COLUMNS:
			var idx: int = header.find(col)
			row[col] = float(line[idx]) if idx >= 0 and idx < line.size() else 0.0
		result[line[champion_col]] = row
	f.close()
	return result


func _load_probe_signals(path: String) -> Dictionary:
	var result: Dictionary = {}
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		print("verify_pairwise_signal: optional draft_probe_signals.csv missing at %s" % path)
		return result
	var header: PackedStringArray = f.get_csv_line()
	var champion_col: int = header.find("champion")
	if champion_col < 0:
		f.close()
		return result
	var probe_cols: Array[int] = []
	var probe_names: Array[String] = []
	for i in range(header.size()):
		if i == champion_col:
			continue
		probe_cols.append(i)
		probe_names.append(header[i])
	while not f.eof_reached():
		var line: PackedStringArray = f.get_csv_line()
		if line.size() <= 1:
			continue
		var values := PackedFloat32Array()
		for col_index in probe_cols:
			values.append(float(line[col_index]) if col_index < line.size() else 0.0)
		result[line[champion_col]] = values
	f.close()
	return result


func _extract_features(ceiling_data: Array, combat_stats: Dictionary, synergy_stats: Dictionary, counter_stats: Dictionary, mechanical_signals: Dictionary, probe_signals: Dictionary) -> Array:
	var result: Array = []
	for comp in ceiling_data:
		var team_a: Array = comp["team_a"]
		var team_b: Array = comp["team_b"]
		var pairwise_a := _extract_pairwise_team_features(team_a, team_b, combat_stats, synergy_stats, counter_stats)
		var pairwise_b := _extract_pairwise_team_features(team_b, team_a, combat_stats, synergy_stats, counter_stats)
		var mech_a := _extract_mechanical_team_features(team_a, mechanical_signals)
		var mech_b := _extract_mechanical_team_features(team_b, mechanical_signals)
		var probe_a := _extract_probe_team_features(team_a, probe_signals)
		var probe_b := _extract_probe_team_features(team_b, probe_signals)
		var pairwise_features := PackedFloat32Array()
		pairwise_features.append_array(pairwise_a)
		pairwise_features.append_array(pairwise_b)
		var mechanical_features := PackedFloat32Array()
		mechanical_features.append_array(mech_a)
		mechanical_features.append_array(mech_b)
		var combined_features := PackedFloat32Array()
		combined_features.append_array(pairwise_features)
		combined_features.append_array(mechanical_features)
		var probe_features := PackedFloat32Array()
		probe_features.append_array(probe_a)
		probe_features.append_array(probe_b)
		var pairwise_probe_features := PackedFloat32Array()
		pairwise_probe_features.append_array(pairwise_features)
		pairwise_probe_features.append_array(probe_features)
		var mechanical_probe_features := PackedFloat32Array()
		mechanical_probe_features.append_array(mechanical_features)
		mechanical_probe_features.append_array(probe_features)
		var combined_all_features := PackedFloat32Array()
		combined_all_features.append_array(pairwise_features)
		combined_all_features.append_array(mechanical_features)
		combined_all_features.append_array(probe_features)
		var team_profile_a := PackedFloat32Array()
		team_profile_a.append_array(pairwise_a)
		team_profile_a.append_array(mech_a)
		team_profile_a.append_array(probe_a)
		var team_profile_b := PackedFloat32Array()
		team_profile_b.append_array(pairwise_b)
		team_profile_b.append_array(mech_b)
		team_profile_b.append_array(probe_b)
		result.append({
			"comp_index": comp["comp_index"],
			"team_a": team_a,
			"team_b": team_b,
			"true_p": comp["true_p"],
			"pairwise": pairwise_features,
			"mechanical": mechanical_features,
			"combined": combined_features,
			"probe": probe_features,
			"pairwise_probe": pairwise_probe_features,
			"mechanical_probe": mechanical_probe_features,
			"combined_all": combined_all_features,
			"team_profile_a": team_profile_a,
			"team_profile_b": team_profile_b,
			"current_pred": _predict_current(pairwise_a, pairwise_b),
		})
	return result


func _extract_pairwise_team_features(team: Array, opponents: Array, combat_stats: Dictionary, synergy_stats: Dictionary, counter_stats: Dictionary) -> PackedFloat32Array:
	var base_sum: float = 0.0
	var synergy_sum: float = 0.0
	var counter_sum: float = 0.0
	var count: int = 0
	for champion in team:
		var base_data: Dictionary = combat_stats.get(champion, {"winrate": 0.5, "samples": 0})
		base_sum += _apply_bayesian_smoothing(base_data["winrate"], base_data["samples"])
		var synergy_values: Array = []
		var synergy_data: Dictionary = synergy_stats.get(champion, {})
		for ally in team:
			if ally != champion:
				synergy_values.append(synergy_data.get(ally, {"winrate": 0.5})["winrate"])
		synergy_sum += _apply_bayesian_smoothing(_compute_mean(synergy_values), synergy_values.size())
		var counter_values: Array = []
		var counter_data: Dictionary = counter_stats.get(champion, {})
		for enemy in opponents:
			counter_values.append(counter_data.get(enemy, {"winrate": 0.5})["winrate"])
		counter_sum += _apply_bayesian_smoothing(_compute_mean(counter_values), counter_values.size())
		count += 1
	var result := PackedFloat32Array()
	result.append(base_sum / float(count) if count > 0 else 0.5)
	result.append(synergy_sum / float(count) if count > 0 else 0.5)
	result.append(counter_sum / float(count) if count > 0 else 0.5)
	return result


func _extract_mechanical_team_features(team: Array, mechanical_signals: Dictionary) -> PackedFloat32Array:
	var sums: Dictionary = {}
	var maxes: Dictionary = {}
	for col in MECHANICAL_COLUMNS:
		sums[col] = 0.0
		maxes[col] = 0.0
	for champion in team:
		var row: Dictionary = mechanical_signals.get(champion, {})
		for col in MECHANICAL_COLUMNS:
			var value: float = float(row.get(col, 0.0))
			sums[col] += value
			maxes[col] = maxf(maxes[col], value)
	var count: float = float(team.size()) if team.size() > 0 else 1.0
	var result := PackedFloat32Array()
	for col in MECHANICAL_COLUMNS:
		result.append(sums[col] / count)
	for col in ["estimated_dps", "effective_hp", "burst_estimate", "sustain_per_sec", "cc_per_sec", "attack_range", "max_aoe_radius"]:
		result.append(maxes[col])
	return result


func _extract_probe_team_features(team: Array, probe_signals: Dictionary) -> PackedFloat32Array:
	if probe_signals.is_empty():
		return PackedFloat32Array()
	var feature_count: int = probe_signals.values()[0].size()
	var sums := PackedFloat32Array()
	var maxes := PackedFloat32Array()
	sums.resize(feature_count)
	maxes.resize(feature_count)
	for i in range(feature_count):
		maxes[i] = 0.0
	for champion in team:
		var values: PackedFloat32Array = probe_signals.get(champion, PackedFloat32Array())
		for i in range(feature_count):
			var value: float = values[i] if i < values.size() else 0.0
			sums[i] += value
			maxes[i] = maxf(maxes[i], value)
	var result := PackedFloat32Array()
	var count: float = float(team.size()) if team.size() > 0 else 1.0
	for i in range(feature_count):
		result.append(sums[i] / count)
	for i in range(feature_count):
		result.append(maxes[i])
	return result


func _feature_set_available(feature_data: Array, feature_set: String) -> bool:
	return not feature_data.is_empty() and feature_data[0].has(feature_set) and feature_data[0][feature_set].size() > 0


func _add_archetype_features(feature_data: Array, split: Dictionary, requested_clusters: int) -> void:
	if feature_data.is_empty():
		return
	var train_vectors: Array[PackedFloat32Array] = []
	for idx in split["train"]:
		train_vectors.append(feature_data[idx]["team_profile_a"])
		train_vectors.append(feature_data[idx]["team_profile_b"])
	if train_vectors.is_empty() or train_vectors[0].is_empty():
		return
	var clusters: int = mini(requested_clusters, train_vectors.size())
	var centroids := _fit_kmeans(train_vectors, clusters)
	var matchup_stats := _build_archetype_matchup_stats(feature_data, split["train"], centroids, clusters)
	for item in feature_data:
		var a_cluster := _nearest_centroid(item["team_profile_a"], centroids)
		var b_cluster := _nearest_centroid(item["team_profile_b"], centroids)
		var matchup_p := _archetype_matchup_probability(matchup_stats, a_cluster, b_cluster)
		var archetype := PackedFloat32Array()
		_append_one_hot(archetype, a_cluster, clusters)
		_append_one_hot(archetype, b_cluster, clusters)
		archetype.append(matchup_p)
		archetype.append(1.0 - matchup_p)
		archetype.append(float(a_cluster) / float(maxi(1, clusters - 1)))
		archetype.append(float(b_cluster) / float(maxi(1, clusters - 1)))
		var pairwise_archetype := PackedFloat32Array()
		pairwise_archetype.append_array(item["pairwise"])
		pairwise_archetype.append_array(archetype)
		item["archetype"] = archetype
		item["pairwise_archetype"] = pairwise_archetype


func _fit_kmeans(vectors: Array[PackedFloat32Array], clusters: int) -> Array[PackedFloat32Array]:
	var centroids: Array[PackedFloat32Array] = []
	for i in range(clusters):
		centroids.append(_copy_vector(vectors[i * vectors.size() / clusters]))
	for _iteration in range(ARCHETYPE_ITERATIONS):
		var sums: Array[PackedFloat32Array] = []
		var counts: Array[int] = []
		for c in range(clusters):
			var zero := PackedFloat32Array()
			zero.resize(vectors[0].size())
			sums.append(zero)
			counts.append(0)
		for vector in vectors:
			var cluster := _nearest_centroid(vector, centroids)
			counts[cluster] += 1
			for i in range(vector.size()):
				sums[cluster][i] += vector[i]
		for c in range(clusters):
			if counts[c] <= 0:
				continue
			for i in range(sums[c].size()):
				sums[c][i] /= float(counts[c])
			centroids[c] = sums[c]
	return centroids


func _build_archetype_matchup_stats(feature_data: Array, train_indices: Array, centroids: Array[PackedFloat32Array], clusters: int) -> Dictionary:
	var stats: Dictionary = {}
	for idx in train_indices:
		var item: Dictionary = feature_data[idx]
		var a_cluster := _nearest_centroid(item["team_profile_a"], centroids)
		var b_cluster := _nearest_centroid(item["team_profile_b"], centroids)
		var key := "%d:%d" % [a_cluster, b_cluster]
		if not stats.has(key):
			stats[key] = {"sum": 0.0, "count": 0}
		stats[key]["sum"] += float(item["true_p"])
		stats[key]["count"] += 1
	return stats


func _archetype_matchup_probability(stats: Dictionary, a_cluster: int, b_cluster: int) -> float:
	var key := "%d:%d" % [a_cluster, b_cluster]
	var reverse_key := "%d:%d" % [b_cluster, a_cluster]
	var sum: float = 0.0
	var count: int = 0
	if stats.has(key):
		sum += float(stats[key]["sum"])
		count += int(stats[key]["count"])
	if stats.has(reverse_key):
		sum += float(stats[reverse_key]["count"]) - float(stats[reverse_key]["sum"])
		count += int(stats[reverse_key]["count"])
	return (sum + ARCHETYPE_SMOOTHING_K * 0.5) / (float(count) + ARCHETYPE_SMOOTHING_K)


func _nearest_centroid(vector: PackedFloat32Array, centroids: Array[PackedFloat32Array]) -> int:
	var best_index: int = 0
	var best_distance: float = INF
	for c in range(centroids.size()):
		var distance := _squared_distance(vector, centroids[c])
		if distance < best_distance:
			best_distance = distance
			best_index = c
	return best_index


func _squared_distance(a: PackedFloat32Array, b: PackedFloat32Array) -> float:
	var distance: float = 0.0
	var n := mini(a.size(), b.size())
	for i in range(n):
		var delta := a[i] - b[i]
		distance += delta * delta
	return distance


func _copy_vector(vector: PackedFloat32Array) -> PackedFloat32Array:
	var result := PackedFloat32Array()
	result.append_array(vector)
	return result


func _append_one_hot(out: PackedFloat32Array, index: int, size: int) -> void:
	for i in range(size):
		out.append(1.0 if i == index else 0.0)


func _predict_current(features_a: PackedFloat32Array, features_b: PackedFloat32Array) -> float:
	var score_a: float = features_a[0] * 0.4 + features_a[1] * 0.3 + features_a[2] * 0.3
	var score_b: float = features_b[0] * 0.4 + features_b[1] * 0.3 + features_b[2] * 0.3
	return 1.0 / (1.0 + exp(-5.0 * (score_a - score_b)))


func _split_indices(size: int, seed: int) -> Dictionary:
	var indices: Array[int] = []
	for i in range(size):
		indices.append(i)
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	for i in range(indices.size() - 1, 0, -1):
		var j := rng.randi_range(0, i)
		var tmp: int = indices[i]
		indices[i] = indices[j]
		indices[j] = tmp
	var train_size: int = int(size * TRAIN_FRACTION)
	return {"train": indices.slice(0, train_size), "test": indices.slice(train_size), "all": indices}


func _train_and_evaluate(feature_data: Array, split: Dictionary, feature_set: String, target_mode: String, overfit: bool) -> Dictionary:
	var train_indices: Array = split["all"] if overfit else split["train"]
	var test_indices: Array = split["all"] if overfit else split["test"]
	var scaler := _fit_scaler(feature_data, train_indices, feature_set)
	var model := _train_logistic_regression(feature_data, train_indices, feature_set, target_mode, scaler)
	var train_eval := _evaluate_model(model, feature_data, train_indices, feature_set, scaler)
	var test_eval := _evaluate_model(model, feature_data, test_indices, feature_set, scaler)
	var all_eval := _evaluate_model(model, feature_data, split["all"], feature_set, scaler)
	return {
		"feature_set": feature_set,
		"target_mode": target_mode,
		"overfit": overfit,
		"scaler": scaler,
		"weights": model["weights"],
		"bias": model["bias"],
		"train": train_eval,
		"test": test_eval,
		"all": all_eval,
	}


func _run_repeated_split_evaluation(feature_data: Array, feature_sets: Array[String], archetype_clusters: int, split_repeats: int) -> Array[Dictionary]:
	var reports: Array[Dictionary] = []
	var selected_sets: Array[String] = []
	for feature_set in REPEATED_SPLIT_FEATURE_SETS:
		if feature_set in feature_sets:
			selected_sets.append(feature_set)
	for repeat_index in range(split_repeats):
		var split := _split_indices(feature_data.size(), SPLIT_SEED + repeat_index * 7919)
		var repeat_data := _duplicate_feature_data(feature_data)
		_add_archetype_features(repeat_data, split, archetype_clusters)
		for feature_set in selected_sets:
			if not _feature_set_available(repeat_data, feature_set):
				continue
			var label_report := _train_and_evaluate(repeat_data, split, feature_set, MODEL_LABEL, false)
			var probability_report := _train_and_evaluate(repeat_data, split, feature_set, MODEL_PROBABILITY, false)
			reports.append({
				"repeat": repeat_index,
				"feature_set": feature_set,
				"label_accuracy": label_report["test"]["accuracy"],
				"label_mse": label_report["test"]["mse"],
				"probability_accuracy": probability_report["test"]["accuracy"],
				"probability_mse": probability_report["test"]["mse"],
			})
	return _summarize_repeat_reports(reports)


func _duplicate_feature_data(feature_data: Array) -> Array:
	var result: Array = []
	for item in feature_data:
		var copy: Dictionary = {}
		for key in item:
			var value = item[key]
			if value is PackedFloat32Array:
				var vector := PackedFloat32Array()
				vector.append_array(value)
				copy[key] = vector
			elif value is Array:
				copy[key] = value.duplicate()
			else:
				copy[key] = value
		result.append(copy)
	return result


func _summarize_repeat_reports(reports: Array[Dictionary]) -> Array[Dictionary]:
	var grouped: Dictionary = {}
	for report in reports:
		var feature_set: String = report["feature_set"]
		if not grouped.has(feature_set):
			grouped[feature_set] = {
				"feature_set": feature_set,
				"label_accuracy": [],
				"label_mse": [],
				"probability_accuracy": [],
				"probability_mse": [],
			}
		grouped[feature_set]["label_accuracy"].append(report["label_accuracy"])
		grouped[feature_set]["label_mse"].append(report["label_mse"])
		grouped[feature_set]["probability_accuracy"].append(report["probability_accuracy"])
		grouped[feature_set]["probability_mse"].append(report["probability_mse"])
	var summaries: Array[Dictionary] = []
	for feature_set in grouped:
		var group: Dictionary = grouped[feature_set]
		summaries.append({
			"feature_set": feature_set,
			"n": group["label_accuracy"].size(),
			"label_accuracy": _summary_stats(group["label_accuracy"]),
			"label_mse": _summary_stats(group["label_mse"]),
			"probability_accuracy": _summary_stats(group["probability_accuracy"]),
			"probability_mse": _summary_stats(group["probability_mse"]),
		})
	summaries.sort_custom(func(a, b): return String(a["feature_set"]) < String(b["feature_set"]))
	return summaries


func _summary_stats(values: Array) -> Dictionary:
	if values.is_empty():
		return {"mean": 0.0, "min": 0.0, "max": 0.0}
	var min_value: float = values[0]
	var max_value: float = values[0]
	var sum: float = 0.0
	for value in values:
		var v: float = float(value)
		min_value = minf(min_value, v)
		max_value = maxf(max_value, v)
		sum += v
	return {"mean": sum / float(values.size()), "min": min_value, "max": max_value}


func _fit_scaler(feature_data: Array, indices: Array, feature_set: String) -> Dictionary:
	var feature_count: int = feature_data[indices[0]][feature_set].size()
	var means := PackedFloat32Array()
	var stddevs := PackedFloat32Array()
	means.resize(feature_count)
	stddevs.resize(feature_count)
	for idx in indices:
		var features: PackedFloat32Array = feature_data[idx][feature_set]
		for i in range(feature_count):
			means[i] += features[i]
	for i in range(feature_count):
		means[i] /= float(indices.size())
	for idx in indices:
		var features: PackedFloat32Array = feature_data[idx][feature_set]
		for i in range(feature_count):
			var d: float = features[i] - means[i]
			stddevs[i] += d * d
	for i in range(feature_count):
		stddevs[i] = sqrt(stddevs[i] / float(indices.size()))
		if stddevs[i] <= 0.000001:
			stddevs[i] = 1.0
	return {"means": means, "stddevs": stddevs}


func _standardized_features(raw: PackedFloat32Array, scaler: Dictionary) -> PackedFloat32Array:
	var means: PackedFloat32Array = scaler["means"]
	var stddevs: PackedFloat32Array = scaler["stddevs"]
	var result := PackedFloat32Array()
	result.resize(raw.size())
	for i in range(raw.size()):
		result[i] = (raw[i] - means[i]) / stddevs[i]
	return result


func _train_logistic_regression(feature_data: Array, train_indices: Array, feature_set: String, target_mode: String, scaler: Dictionary) -> Dictionary:
	var num_features: int = feature_data[train_indices[0]][feature_set].size()
	var weights := PackedFloat32Array()
	weights.resize(num_features)
	var bias: float = 0.0
	for iteration in range(MAX_ITERATIONS):
		var grad_w := PackedFloat32Array()
		grad_w.resize(num_features)
		var grad_b: float = 0.0
		for idx in train_indices:
			var item: Dictionary = feature_data[idx]
			var features := _standardized_features(item[feature_set], scaler)
			var target: float = 1.0 if item["true_p"] > 0.5 else 0.0
			if target_mode == MODEL_PROBABILITY:
				target = item["true_p"]
			var pred := _predict_logistic(weights, bias, features)
			var error: float = pred - target
			for i in range(num_features):
				grad_w[i] += error * features[i]
			grad_b += error
		var max_delta: float = 0.0
		for i in range(num_features):
			grad_w[i] = grad_w[i] / float(train_indices.size()) + L2_REGULARIZATION * weights[i]
			var delta: float = LEARNING_RATE * grad_w[i]
			weights[i] -= delta
			max_delta = maxf(max_delta, absf(delta))
		bias -= LEARNING_RATE * grad_b / float(train_indices.size())
		if max_delta < CONVERGENCE_TOLERANCE:
			break
	return {"weights": weights, "bias": bias}


func _evaluate_model(model: Dictionary, feature_data: Array, indices: Array, feature_set: String, scaler: Dictionary) -> Dictionary:
	var weights: PackedFloat32Array = model["weights"]
	var bias: float = model["bias"]
	var correct: int = 0
	var mse: float = 0.0
	var preds: Array = []
	for idx in indices:
		var item: Dictionary = feature_data[idx]
		var pred := _predict_logistic(weights, bias, _standardized_features(item[feature_set], scaler))
		preds.append(pred)
		if (pred > 0.5) == (item["true_p"] > 0.5):
			correct += 1
		mse += (pred - item["true_p"]) * (pred - item["true_p"])
	return {
		"n": indices.size(),
		"accuracy": float(correct) / float(indices.size()) if indices.size() > 0 else 0.0,
		"mse": mse / float(indices.size()) if indices.size() > 0 else 0.0,
		"pred_min": preds.min() if not preds.is_empty() else 0.0,
		"pred_max": preds.max() if not preds.is_empty() else 0.0,
		"pred_mean": _compute_mean(preds),
	}


func _evaluate_baselines(feature_data: Array, split: Dictionary) -> Dictionary:
	return {
		"always_team_a": {
			"train": _evaluate_prediction_column(feature_data, split["train"], "", true),
			"test": _evaluate_prediction_column(feature_data, split["test"], "", true),
			"all": _evaluate_prediction_column(feature_data, split["all"], "", true),
		},
		"current": {
			"train": _evaluate_prediction_column(feature_data, split["train"], "current_pred", false),
			"test": _evaluate_prediction_column(feature_data, split["test"], "current_pred", false),
			"all": _evaluate_prediction_column(feature_data, split["all"], "current_pred", false),
		},
	}


func _evaluate_prediction_column(feature_data: Array, indices: Array, pred_key: String, always_team_a: bool) -> Dictionary:
	var correct: int = 0
	var mse: float = 0.0
	var preds: Array = []
	for idx in indices:
		var item: Dictionary = feature_data[idx]
		var pred: float = 1.0 if always_team_a else item[pred_key]
		preds.append(pred)
		if (pred > 0.5) == (item["true_p"] > 0.5):
			correct += 1
		mse += (pred - item["true_p"]) * (pred - item["true_p"])
	return {
		"n": indices.size(),
		"accuracy": float(correct) / float(indices.size()) if indices.size() > 0 else 0.0,
		"mse": mse / float(indices.size()) if indices.size() > 0 else 0.0,
		"pred_min": preds.min() if not preds.is_empty() else 0.0,
		"pred_max": preds.max() if not preds.is_empty() else 0.0,
		"pred_mean": _compute_mean(preds),
	}


func _write_results(output_path: String, feature_data: Array, model_reports: Array[Dictionary]) -> void:
	var main_model: Dictionary = {}
	for report in model_reports:
		if report["feature_set"] == "combined" and report["target_mode"] == MODEL_LABEL and not report["overfit"]:
			main_model = report
			break
	var lines: Array[String] = ["comp_index,team_a,team_b,true_p,current_pred,combined_label_pred,error_current,error_combined_label"]
	for item in feature_data:
		var pred: float = 0.5
		if not main_model.is_empty():
			pred = _predict_logistic(main_model["weights"], main_model["bias"], _standardized_features(item["combined"], main_model["scaler"]))
		lines.append("%d,%s,%s,%.4f,%.4f,%.4f,%.4f,%.4f" % [
			item["comp_index"],
			"|".join(item["team_a"]),
			"|".join(item["team_b"]),
			item["true_p"],
			item["current_pred"],
			pred,
			item["current_pred"] - item["true_p"],
			pred - item["true_p"],
		])
	var f := FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f == null:
		push_error("Failed to open %s for writing" % output_path)
		return
	f.store_string("\n".join(lines) + "\n")
	f.close()


func _print_report(baseline: Dictionary, model_reports: Array[Dictionary], repeat_reports: Array[Dictionary], current_corr: float, output_path: String) -> void:
	var lines: Array[String] = []
	lines.append("")
	lines.append("================ DRAFT FEATURE SIGNAL VERIFICATION ================")
	lines.append("Current heuristic correlation vs true p: %.4f" % current_corr)
	lines.append("")
	lines.append("Baselines:")
	lines.append(_format_eval_line("  always_team_a", baseline["always_team_a"]))
	lines.append(_format_eval_line("  current_pairwise", baseline["current"]))
	lines.append("")
	lines.append("Logistic models (standardized, shuffled split seed=%d):" % SPLIT_SEED)
	for report in model_reports:
		lines.append(_format_model_line(report))
	if not repeat_reports.is_empty():
		lines.append("")
		lines.append("Repeated split summary:")
		for report in repeat_reports:
			lines.append(_format_repeat_line(report))
	lines.append("")
	lines.append("Decision gate:")
	var pairwise := _find_report(model_reports, "pairwise", MODEL_LABEL, false)
	var combined := _find_report(model_reports, "combined", MODEL_LABEL, false)
	var pairwise_archetype := _find_report(model_reports, "pairwise_archetype", MODEL_LABEL, false)
	var pairwise_probe := _find_report(model_reports, "pairwise_probe", MODEL_LABEL, false)
	if not pairwise.is_empty() and not combined.is_empty():
		var delta_pp: float = (combined["test"]["accuracy"] - pairwise["test"]["accuracy"]) * 100.0
		lines.append("  combined_label vs pairwise_label test delta: %+.1f pp" % delta_pp)
		if delta_pp >= 2.0 or (combined["test"]["accuracy"] >= pairwise["test"]["accuracy"] and combined["test"]["mse"] < pairwise["test"]["mse"]):
			lines.append("  recommendation: keep expanded mechanical feature path for next native wiring experiment")
		else:
			lines.append("  recommendation: do not wire mechanical features into native recommender yet; move to archetype/probe signals")
	if not pairwise.is_empty() and not pairwise_archetype.is_empty():
		var archetype_delta_pp: float = (pairwise_archetype["test"]["accuracy"] - pairwise["test"]["accuracy"]) * 100.0
		lines.append("  pairwise_archetype_label vs pairwise_label test delta: %+.1f pp" % archetype_delta_pp)
		if archetype_delta_pp >= 2.0 or (pairwise_archetype["test"]["accuracy"] >= pairwise["test"]["accuracy"] and pairwise_archetype["test"]["mse"] < pairwise["test"]["mse"]):
			lines.append("  recommendation: keep archetype feature path for next experiment")
		else:
			lines.append("  recommendation: do not wire archetype features into native recommender yet")
	if not pairwise.is_empty() and not pairwise_probe.is_empty():
		var probe_delta_pp: float = (pairwise_probe["test"]["accuracy"] - pairwise["test"]["accuracy"]) * 100.0
		lines.append("  pairwise_probe_label vs pairwise_label test delta: %+.1f pp" % probe_delta_pp)
		if probe_delta_pp >= 2.0 or (pairwise_probe["test"]["accuracy"] >= pairwise["test"]["accuracy"] and pairwise_probe["test"]["mse"] < pairwise["test"]["mse"]):
			lines.append("  recommendation: keep simulation probe feature path for next experiment")
		else:
			lines.append("  recommendation: do not wire simulation probes into native recommender yet")
	lines.append("")
	lines.append("CSV written to: %s" % output_path)
	lines.append("=========================================================")
	print("\n".join(lines))


func _format_eval_line(label: String, evals: Dictionary) -> String:
	return "%s train=%.1f%% test=%.1f%% all=%.1f%% mse(test)=%.4f pred(test)=%.4f..%.4f mean=%.4f" % [
		label,
		evals["train"]["accuracy"] * 100.0,
		evals["test"]["accuracy"] * 100.0,
		evals["all"]["accuracy"] * 100.0,
		evals["test"]["mse"],
		evals["test"]["pred_min"],
		evals["test"]["pred_max"],
		evals["test"]["pred_mean"],
	]


func _format_model_line(report: Dictionary) -> String:
	var mode: String = "overfit" if report["overfit"] else report["target_mode"]
	return "  %s/%s train=%.1f%% test=%.1f%% all=%.1f%% mse(test)=%.4f pred(test)=%.4f..%.4f mean=%.4f weights=%s bias=%.6f means=%s stddevs=%s" % [
		report["feature_set"],
		mode,
		report["train"]["accuracy"] * 100.0,
		report["test"]["accuracy"] * 100.0,
		report["all"]["accuracy"] * 100.0,
		report["test"]["mse"],
		report["test"]["pred_min"],
		report["test"]["pred_max"],
		report["test"]["pred_mean"],
		_format_weights(report["weights"]),
		report["bias"],
		_format_weights(report["scaler"]["means"]),
		_format_weights(report["scaler"]["stddevs"]),
	]


func _format_repeat_line(report: Dictionary) -> String:
	var la: Dictionary = report["label_accuracy"]
	var lm: Dictionary = report["label_mse"]
	var pa: Dictionary = report["probability_accuracy"]
	var pm: Dictionary = report["probability_mse"]
	return "  %s n=%d label_acc=%.1f%% [%.1f, %.1f] label_mse=%.4f prob_acc=%.1f%% [%.1f, %.1f] prob_mse=%.4f" % [
		report["feature_set"],
		report["n"],
		la["mean"] * 100.0,
		la["min"] * 100.0,
		la["max"] * 100.0,
		lm["mean"],
		pa["mean"] * 100.0,
		pa["min"] * 100.0,
		pa["max"] * 100.0,
		pm["mean"],
	]


func _find_report(reports: Array[Dictionary], feature_set: String, target_mode: String, overfit: bool) -> Dictionary:
	for report in reports:
		if report["feature_set"] == feature_set and report["target_mode"] == target_mode and report["overfit"] == overfit:
			return report
	return {}


func _format_weights(weights: PackedFloat32Array) -> String:
	var parts: Array[String] = []
	for i in range(mini(weights.size(), 8)):
		parts.append("%.6f" % weights[i])
	if weights.size() > 8:
		parts.append("...")
	return "[" + ",".join(parts) + "]"


func _predict_logistic(weights: PackedFloat32Array, bias: float, features: PackedFloat32Array) -> float:
	var z: float = bias
	for i in range(weights.size()):
		z += weights[i] * features[i]
	return 1.0 / (1.0 + exp(-z))


func _apply_bayesian_smoothing(raw_winrate: float, samples: int) -> float:
	if samples <= 0:
		return PRIOR_WINRATE
	var weight: float = float(samples) / (float(samples) + SMOOTHING_K)
	return weight * raw_winrate + (1.0 - weight) * PRIOR_WINRATE


func _compute_mean(values: Array) -> float:
	if values.is_empty():
		return 0.0
	var sum: float = 0.0
	for v in values:
		sum += float(v)
	return sum / float(values.size())


func _compute_correlation(feature_data: Array, x_key: String, y_key: String) -> float:
	var x: Array = []
	var y: Array = []
	for item in feature_data:
		x.append(item[x_key])
		y.append(item[y_key])
	if x.size() < 2:
		return 0.0
	var mean_x := _compute_mean(x)
	var mean_y := _compute_mean(y)
	var sum_xy: float = 0.0
	var sum_x2: float = 0.0
	var sum_y2: float = 0.0
	for i in range(x.size()):
		var dx: float = x[i] - mean_x
		var dy: float = y[i] - mean_y
		sum_xy += dx * dy
		sum_x2 += dx * dx
		sum_y2 += dy * dy
	var denominator := sqrt(sum_x2 * sum_y2)
	return sum_xy / denominator if denominator > 0.0 else 0.0
