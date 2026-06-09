extends SceneTree

## Verify partial draft prediction signal by training depth-specific logistic models.
##
## Trains 4 separate logistic regression models (one per draft depth 1-4)
## on partial draft states labeled with rollout oracle predictions.

const LEARNING_RATE: float = 0.05
const MAX_ITERATIONS: int = 5000
const CONVERGENCE_TOLERANCE: float = 1e-7
const L2_REGULARIZATION: float = 0.001
const TRAIN_FRACTION: float = 0.8
const SPLIT_SEED: int = 1337

const DEFAULT_OUTPUT := "res://stats_output_partial/partial_draft_verification_smoke.csv"


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	var training_input := _extract_argument("--training-input=", "res://stats_output_partial/partial_draft_training_smoke.csv")
	var stats_dir := _extract_argument("--stats-dir=", "res://stats_output")
	var output_path := _extract_argument("--output=", DEFAULT_OUTPUT)

	print("verify_partial_draft_signal: loading training data from %s" % training_input)
	var training_data := _load_training_csv(training_input)
	if training_data.is_empty():
		push_error("verify_partial_draft_signal: failed to load training data")
		quit(1)
		return

	print("verify_partial_draft_signal: loading stats from %s" % stats_dir)
	var combat_stats := _load_combat_stats(stats_dir + "/combat_stats.csv")
	var synergy_stats := _load_matchup_stats(stats_dir + "/matchup_with.csv")
	var counter_stats := _load_matchup_stats(stats_dir + "/matchup_vs.csv")

	if combat_stats.is_empty() or synergy_stats.is_empty() or counter_stats.is_empty():
		push_error("verify_partial_draft_signal: failed to load stats CSVs")
		quit(1)
		return

	# Group data by depth
	var data_by_depth: Dictionary = {}
	for depth in range(1, 5):
		data_by_depth[depth] = []
	for row: Dictionary in training_data:
		var depth := int(row["depth"])
		if depth in data_by_depth:
			data_by_depth[depth].append(row)

	# Train and validate per depth
	var csv_lines: Array[String] = [
		"depth,train_size,test_size,train_accuracy,test_accuracy,train_mse,test_mse"
	]
	for depth in range(1, 5):
		var depth_data: Array = data_by_depth.get(depth, [])
		if depth_data.is_empty():
			print("verify_partial_draft_signal: no data for depth %d, skipping" % depth)
			continue

		print("verify_partial_draft_signal: processing depth %d (%d samples)" % [depth, depth_data.size()])

		# Extract features and labels
		var features: Array = []
		var labels: Array[float] = []
		for row: Dictionary in depth_data:
			var allies := (row["allies"] as String).split("|")
			var enemies := (row["enemies"] as String).split("|")
			var feature_vec := _extract_pairwise_features(allies, enemies, combat_stats, synergy_stats, counter_stats)
			features.append(feature_vec)
			labels.append(float(row["expected_win"]))

		# Train/test split
		var split_index := int(depth_data.size() * TRAIN_FRACTION)
		var train_features := features.slice(0, split_index)
		var train_labels := labels.slice(0, split_index)
		var test_features := features.slice(split_index)
		var test_labels := labels.slice(split_index)

		# Standardize features
		var train_mean := _compute_mean(train_features)
		var train_std := _compute_std(train_features, train_mean)
		var train_stdized := _standardize(train_features, train_mean, train_std)
		var test_stdized := _standardize(test_features, train_mean, train_std)

		# Train logistic regression
		var weights := _train_logistic(train_stdized, train_labels)
		var train_preds := _predict_logistic(train_stdized, weights)
		var test_preds := _predict_logistic(test_stdized, weights)

		# Compute metrics
		var train_acc := _compute_accuracy(train_preds, train_labels)
		var test_acc := _compute_accuracy(test_preds, test_labels)
		var train_mse := _compute_mse(train_preds, train_labels)
		var test_mse := _compute_mse(test_preds, test_labels)

		csv_lines.append("%d,%d,%d,%.4f,%.4f,%.6f,%.6f" % [
			depth, train_features.size(), test_features.size(), train_acc, test_acc, train_mse, test_mse
		])

	# Write verification CSV
	var f := FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f == null:
		push_error("verify_partial_draft_signal: could not open %s" % output_path)
		quit(1)
		return
	f.store_string("\n".join(csv_lines) + "\n")
	f.close()

	print("")
	print("================ PARTIAL DRAFT VERIFICATION COMPLETE ================")
	print("CSV written to: %s" % output_path)
	print("=======================================================================")
	quit(0)


func _load_training_csv(path: String) -> Array:
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		push_error("verify_partial_draft_signal: could not open %s" % path)
		return []
	var lines := f.get_as_text().split("\n")
	f.close()

	var result := []
	var headers := []
	for i in range(lines.size()):
		var line := lines[i].strip_edges()
		if line.is_empty():
			continue
		var parts := line.split(",")
		if i == 0:
			headers = parts
		else:
			var row := {}
			for j in range(mini(parts.size(), headers.size())):
				row[headers[j]] = parts[j]
			result.append(row)
	return result


func _load_combat_stats(path: String) -> Dictionary:
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		push_error("verify_partial_draft_signal: could not open %s" % path)
		return {}
	var lines := f.get_as_text().split("\n")
	f.close()

	var result := {}
	for i in range(1, lines.size()):
		var line := lines[i].strip_edges()
		if line.is_empty():
			continue
		var parts := line.split(",")
		if parts.size() >= 2:
			result[parts[0]] = float(parts[1])
	return result


func _load_matchup_stats(path: String) -> Dictionary:
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		push_error("verify_partial_draft_signal: could not open %s" % path)
		return {}
	var lines := f.get_as_text().split("\n")
	f.close()

	var result := {}
	for i in range(1, lines.size()):
		var line := lines[i].strip_edges()
		if line.is_empty():
			continue
		var parts := line.split(",")
		if parts.size() >= 5:
			var key := parts[0] + "|" + parts[1]
			result[key] = float(parts[4])
	return result


func _extract_pairwise_features(allies: Array, enemies: Array, combat_stats: Dictionary, synergy_stats: Dictionary, counter_stats: Dictionary) -> Array[float]:
	# Features: [base_winrate, avg_synergy, avg_counter, synergy_variance, counter_variance, net_matchup]
	var base_sum := 0.0
	var synergy_sum := 0.0
	var counter_sum := 0.0
	var synergy_values: Array[float] = []
	var counter_values: Array[float] = []
	var matchup_sum := 0.0

	for ally in allies:
		base_sum += combat_stats.get(String(ally), 0.5)
		for other_ally in allies:
			if ally != other_ally:
				var key := String(ally) + "|" + String(other_ally)
				var syn: float = synergy_stats.get(key, 0.5)
				synergy_sum += syn
				synergy_values.append(syn)
		for enemy in enemies:
			var key := String(ally) + "|" + String(enemy)
			var cnt: float = counter_stats.get(key, 0.5)
			counter_sum += cnt
			counter_values.append(cnt)
			matchup_sum += cnt

	var n_allies := float(allies.size())
	var n_enemies := float(enemies.size())
	var base_winrate := base_sum / n_allies if n_allies > 0 else 0.5
	var avg_synergy := synergy_sum / (n_allies * (n_allies - 1)) if n_allies > 1 else 0.5
	var avg_counter := counter_sum / (n_allies * n_enemies) if n_allies > 0 and n_enemies > 0 else 0.5
	var synergy_variance := _compute_variance(synergy_values) if synergy_values.size() > 0 else 0.0
	var counter_variance := _compute_variance(counter_values) if counter_values.size() > 0 else 0.0
	var net_matchup := matchup_sum / (n_allies * n_enemies) if n_allies > 0 and n_enemies > 0 else 0.5

	return [base_winrate, avg_synergy, avg_counter, synergy_variance, counter_variance, net_matchup]


func _compute_variance(values: Array[float]) -> float:
	if values.is_empty():
		return 0.0
	var mean := 0.0
	for v in values:
		mean += v
	mean /= float(values.size())
	var variance := 0.0
	for v in values:
		variance += (v - mean) * (v - mean)
	return variance / float(values.size())


func _compute_mean(features: Array) -> Array[float]:
	if features.is_empty():
		return [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
	var n := float(features.size())
	var dim: int = features[0].size()
	var result: Array[float] = []
	for i in range(dim):
		var sum := 0.0
		for vec in features:
			sum += vec[i]
		result.append(sum / n)
	return result


func _compute_std(features: Array, mean: Array[float]) -> Array[float]:
	if features.is_empty():
		return [1.0, 1.0, 1.0, 1.0, 1.0, 1.0]
	var n := float(features.size())
	var dim: int = features[0].size()
	var result: Array[float] = []
	for i in range(dim):
		var sum_sq := 0.0
		for vec in features:
			var diff: float = vec[i] - mean[i]
			sum_sq += diff * diff
		var variance := sum_sq / n
		result.append(sqrt(variance) if variance > 0 else 1.0)
	return result


func _standardize(features: Array, mean: Array[float], std: Array[float]) -> Array:
	var result: Array = []
	for vec in features:
		var stdized: Array[float] = []
		for i in range(vec.size()):
			stdized.append((vec[i] - mean[i]) / std[i])
		result.append(stdized)
	return result


func _train_logistic(features: Array, labels: Array[float]) -> Array[float]:
	var dim: int = features[0].size()
	var weights: Array[float] = []
	for i in range(dim + 1):
		weights.append(0.0)

	for iteration in range(MAX_ITERATIONS):
		var grad: Array[float] = []
		for i in range(dim + 1):
			grad.append(0.0)

		for i in range(features.size()):
			var x = features[i]
			var y: float = labels[i]
			var z: float = weights[0]
			for j in range(dim):
				z += weights[j + 1] * x[j]
			var p: float = 1.0 / (1.0 + exp(-z))
			var error: float = p - y
			grad[0] += error
			for j in range(dim):
				grad[j + 1] += error * x[j] + L2_REGULARIZATION * weights[j + 1]

		var max_grad: float = 0.0
		for g in grad:
			max_grad = max(max_grad, abs(g))

		if max_grad < CONVERGENCE_TOLERANCE:
			break

		for i in range(dim + 1):
			weights[i] -= LEARNING_RATE * grad[i] / float(features.size())

	return weights


func _predict_logistic(features: Array, weights: Array[float]) -> Array[float]:
	var result: Array[float] = []
	var dim: int = weights.size() - 1
	for x in features:
		var z: float = weights[0]
		for j in range(mini(dim, x.size())):
			z += weights[j + 1] * x[j]
		var p: float = 1.0 / (1.0 + exp(-z))
		result.append(p)
	return result


func _compute_accuracy(preds: Array[float], labels: Array[float]) -> float:
	var correct := 0
	for i in range(preds.size()):
		var pred_label := 1 if preds[i] >= 0.5 else 0
		var true_label := 1 if labels[i] >= 0.5 else 0
		if pred_label == true_label:
			correct += 1
	return float(correct) / float(preds.size())


func _compute_mse(preds: Array[float], labels: Array[float]) -> float:
	var sum_sq := 0.0
	for i in range(preds.size()):
		var diff := preds[i] - labels[i]
		sum_sq += diff * diff
	return sum_sq / float(preds.size())
