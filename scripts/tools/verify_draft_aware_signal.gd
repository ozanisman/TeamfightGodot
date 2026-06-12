extends SceneTree

const LEARNING_RATE: float = 0.05
const MAX_ITERATIONS: int = 5000
const CONVERGENCE_TOLERANCE: float = 1e-7
const L2_REGULARIZATION: float = 0.001
const ROLES: Array[String] = ["tank", "fighter", "mage", "assassin", "marksman", "support"]
const FEATURE_DIM: int = 30

const DEFAULT_OUTPUT := "res://stats_output/draft_aware_model.csv"


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	var training_input := _extract_argument("--training-input=", "res://stats_output_training/draft_aware_training.csv")
	var testing_input := _extract_argument("--testing-input=", "res://stats_output_testing/draft_aware_training.csv")
	var stats_dir := _extract_argument("--stats-dir=", "res://stats_output_training")
	var output_path := _extract_argument("--output=", DEFAULT_OUTPUT)

	print("verify_draft_aware_signal: loading training data from %s" % training_input)
	var training_data := _load_training_csv(training_input)
	if training_data.is_empty():
		push_error("verify_draft_aware_signal: failed to load training data")
		quit(1)
		return

	print("verify_draft_aware_signal: loading testing data from %s" % testing_input)
	var testing_data := _load_training_csv(testing_input)
	if testing_data.is_empty():
		push_error("verify_draft_aware_signal: failed to load testing data")
		quit(1)
		return

	print("verify_draft_aware_signal: loading stats from %s" % stats_dir)
	var combat_stats := _load_combat_stats(stats_dir + "/combat_stats.csv")
	var synergy_stats := _load_matchup_stats(stats_dir + "/matchup_with.csv")
	var counter_stats := _load_matchup_stats(stats_dir + "/matchup_vs.csv")

	if combat_stats.is_empty() or synergy_stats.is_empty() or counter_stats.is_empty():
		push_error("verify_draft_aware_signal: failed to load stats CSVs")
		quit(1)
		return

	print("verify_draft_aware_signal: training on full training data, testing on full testing data")

	# Group training and testing data by depth
	var training_by_depth: Dictionary = {}
	var testing_by_depth: Dictionary = {}
	for depth in range(1, 5):
		training_by_depth[depth] = []
		testing_by_depth[depth] = []
	for row: Dictionary in training_data:
		var depth := int(row["depth"])
		if depth in training_by_depth:
			training_by_depth[depth].append(row)
	for row: Dictionary in testing_data:
		var depth := int(row["depth"])
		if depth in testing_by_depth:
			testing_by_depth[depth].append(row)

	# Train and test per depth
	var csv_lines: Array[String] = [
		"depth,train_size,test_size,train_accuracy,test_accuracy,train_mse,test_mse"
	]
	var trained_weights: Dictionary = {}
	for depth in range(1, 5):
		var depth_training: Array = training_by_depth.get(depth, [])
		var depth_testing: Array = testing_by_depth.get(depth, [])
		if depth_training.is_empty() or depth_testing.is_empty():
			print("verify_draft_aware_signal: no data for depth %d, skipping" % depth)
			continue

		print("verify_draft_aware_signal: processing depth %d (train: %d samples, test: %d samples)" % [depth, depth_training.size(), depth_testing.size()])

		# Extract training features and labels
		var train_features: Array = []
		var train_labels: Array[float] = []
		for row: Dictionary in depth_training:
			var allies: Array = (row["allies"] as String).split("|")
			var enemies: Array = (row["enemies"] as String).split("|")
			# Extract role counts if available
			var ally_roles: Dictionary = {}
			var enemy_roles: Dictionary = {}
			for role in ROLES:
				ally_roles[role] = int(row.get("ally_%s" % role, 0)) if row.has("ally_%s" % role) else 0
				enemy_roles[role] = int(row.get("enemy_%s" % role, 0)) if row.has("enemy_%s" % role) else 0
			
			# Extract synergy/counter features if available
			var syn_cnt: Dictionary = {}
			syn_cnt["avg_synergy"] = float(row.get("avg_synergy", 0.5)) if row.has("avg_synergy") else 0.5
			syn_cnt["avg_counter"] = float(row.get("avg_counter", 0.5)) if row.has("avg_counter") else 0.5
			syn_cnt["synergy_variance"] = float(row.get("synergy_variance", 0.0)) if row.has("synergy_variance") else 0.0
			syn_cnt["counter_variance"] = float(row.get("counter_variance", 0.0)) if row.has("counter_variance") else 0.0
			syn_cnt["net_matchup"] = float(row.get("net_matchup", 0.5)) if row.has("net_matchup") else 0.5
			
			# Extract composition balance features if available
			var comp_balance: Dictionary = {}
			comp_balance["ally_role_diversity"] = float(row.get("ally_role_diversity", 0.0)) if row.has("ally_role_diversity") else 0.0
			comp_balance["enemy_role_diversity"] = float(row.get("enemy_role_diversity", 0.0)) if row.has("enemy_role_diversity") else 0.0
			comp_balance["ally_tank_ratio"] = float(row.get("ally_tank_ratio", 0.0)) if row.has("ally_tank_ratio") else 0.0
			comp_balance["enemy_tank_ratio"] = float(row.get("enemy_tank_ratio", 0.0)) if row.has("enemy_tank_ratio") else 0.0
			comp_balance["ally_support_ratio"] = float(row.get("ally_support_ratio", 0.0)) if row.has("ally_support_ratio") else 0.0
			comp_balance["enemy_support_ratio"] = float(row.get("enemy_support_ratio", 0.0)) if row.has("enemy_support_ratio") else 0.0
			comp_balance["ally_damage_ratio"] = float(row.get("ally_damage_ratio", 0.0)) if row.has("ally_damage_ratio") else 0.0
			comp_balance["enemy_damage_ratio"] = float(row.get("enemy_damage_ratio", 0.0)) if row.has("enemy_damage_ratio") else 0.0
			
			var feature_vec := _extract_pairwise_features(allies, enemies, combat_stats, synergy_stats, counter_stats, ally_roles, enemy_roles, syn_cnt, comp_balance)
			train_features.append(feature_vec)
			train_labels.append(float(row["expected_win"]))

		# Extract testing features and labels
		var test_features: Array = []
		var test_labels: Array[float] = []
		for row: Dictionary in depth_testing:
			var allies: Array = (row["allies"] as String).split("|")
			var enemies: Array = (row["enemies"] as String).split("|")
			# Extract role counts if available
			var ally_roles: Dictionary = {}
			var enemy_roles: Dictionary = {}
			for role in ROLES:
				ally_roles[role] = int(row.get("ally_%s" % role, 0)) if row.has("ally_%s" % role) else 0
				enemy_roles[role] = int(row.get("enemy_%s" % role, 0)) if row.has("enemy_%s" % role) else 0
			
			# Extract synergy/counter features if available
			var syn_cnt: Dictionary = {}
			syn_cnt["avg_synergy"] = float(row.get("avg_synergy", 0.5)) if row.has("avg_synergy") else 0.5
			syn_cnt["avg_counter"] = float(row.get("avg_counter", 0.5)) if row.has("avg_counter") else 0.5
			syn_cnt["synergy_variance"] = float(row.get("synergy_variance", 0.0)) if row.has("synergy_variance") else 0.0
			syn_cnt["counter_variance"] = float(row.get("counter_variance", 0.0)) if row.has("counter_variance") else 0.0
			syn_cnt["net_matchup"] = float(row.get("net_matchup", 0.5)) if row.has("net_matchup") else 0.5
			
			# Extract composition balance features if available
			var comp_balance: Dictionary = {}
			comp_balance["ally_role_diversity"] = float(row.get("ally_role_diversity", 0.0)) if row.has("ally_role_diversity") else 0.0
			comp_balance["enemy_role_diversity"] = float(row.get("enemy_role_diversity", 0.0)) if row.has("enemy_role_diversity") else 0.0
			comp_balance["ally_tank_ratio"] = float(row.get("ally_tank_ratio", 0.0)) if row.has("ally_tank_ratio") else 0.0
			comp_balance["enemy_tank_ratio"] = float(row.get("enemy_tank_ratio", 0.0)) if row.has("enemy_tank_ratio") else 0.0
			comp_balance["ally_support_ratio"] = float(row.get("ally_support_ratio", 0.0)) if row.has("ally_support_ratio") else 0.0
			comp_balance["enemy_support_ratio"] = float(row.get("enemy_support_ratio", 0.0)) if row.has("enemy_support_ratio") else 0.0
			comp_balance["ally_damage_ratio"] = float(row.get("ally_damage_ratio", 0.0)) if row.has("ally_damage_ratio") else 0.0
			comp_balance["enemy_damage_ratio"] = float(row.get("enemy_damage_ratio", 0.0)) if row.has("enemy_damage_ratio") else 0.0
			
			var feature_vec := _extract_pairwise_features(allies, enemies, combat_stats, synergy_stats, counter_stats, ally_roles, enemy_roles, syn_cnt, comp_balance)
			test_features.append(feature_vec)
			test_labels.append(float(row["expected_win"]))

		# Standardize features using training statistics
		var train_mean := _compute_mean(train_features)
		var train_std := _compute_std(train_features, train_mean)
		var train_stdized := _standardize(train_features, train_mean, train_std)
		var test_stdized := _standardize(test_features, train_mean, train_std)

		# Train logistic regression on full training data
		var weights := _train_logistic(train_stdized, train_labels)
		trained_weights[depth] = weights
		
		var train_preds := _predict_logistic(train_stdized, weights)
		var test_preds := _predict_logistic(test_stdized, weights)

		# Compute metrics
		var train_acc := _compute_accuracy(train_preds, train_labels)
		var test_acc := _compute_accuracy(test_preds, test_labels)
		var train_mse := _compute_mse(train_preds, train_labels)
		var test_mse := _compute_mse(test_preds, test_labels)

		# Output weights for native baking
		var weights_str := ",".join(weights.map(func(x): return str(x)))
		var means_str := ",".join(train_mean.map(func(x): return str(x)))
		var stddevs_str := ",".join(train_std.map(func(x): return str(x)))
		print("Depth %d weights: %s" % [depth, weights_str])
		print("Depth %d means: %s" % [depth, means_str])
		print("Depth %d stddevs: %s" % [depth, stddevs_str])

		csv_lines.append("%d,%d,%d,%.4f,%.4f,%.6f,%.6f" % [
			depth, train_features.size(), test_features.size(), train_acc, test_acc, train_mse, test_mse
		])

	var mk := _ensure_parent_dir(ProjectSettings.globalize_path(output_path))
	if mk != OK:
		push_error("verify_draft_aware_signal: could not create output directory for %s: %s" % [output_path, error_string(mk)])
		quit(1)
		return

	var f := FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f == null:
		push_error("verify_draft_aware_signal: could not open %s" % output_path)
		quit(1)
		return
	f.store_string("\n".join(csv_lines) + "\n")
	f.close()
	
	print("")
	print("================ DRAFT-AWARE VERIFICATION COMPLETE ================")
	print("CSV written to: %s" % output_path)
	print("====================================================================")
	quit(0)


func _ensure_parent_dir(path: String) -> Error:
	var slash := path.rfind("/")
	var backslash := path.rfind("\\")
	var sep := maxi(slash, backslash)
	if sep < 0:
		return OK
	var dir_path := path.substr(0, sep)
	if dir_path.is_empty() or DirAccess.dir_exists_absolute(dir_path):
		return OK
	return DirAccess.make_dir_recursive_absolute(dir_path)


func _load_training_csv(path: String) -> Array:
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		push_error("verify_draft_aware_signal: could not open %s" % path)
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
		push_error("verify_draft_aware_signal: could not open %s" % path)
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
		push_error("verify_draft_aware_signal: could not open %s" % path)
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


func _extract_pairwise_features(allies: Array, enemies: Array, combat_stats: Dictionary, synergy_stats: Dictionary, counter_stats: Dictionary, ally_roles: Dictionary = {}, enemy_roles: Dictionary = {}, syn_cnt: Dictionary = {}, comp_balance: Dictionary = {}) -> Array:
	# Pick-focused features (30 total): [base_winrate, avg_synergy, avg_counter, synergy_variance, counter_variance, net_matchup,
	#            ally_tank, ally_fighter, ally_mage, ally_assassin, ally_marksman, ally_support,
	#            enemy_tank, enemy_fighter, enemy_mage, enemy_assassin, enemy_marksman, enemy_support,
	#            avg_synergy_precomputed, avg_counter_precomputed, synergy_variance_precomputed, counter_variance_precomputed,
	#            ally_role_diversity, enemy_role_diversity, ally_tank_ratio, enemy_tank_ratio,
	#            ally_support_ratio, enemy_support_ratio, ally_damage_ratio, enemy_damage_ratio]
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

	# Build result with pick-focused features only (no ban features)
	var result := [base_winrate, avg_synergy, avg_counter, synergy_variance, counter_variance, net_matchup]
	
	# Add ally role counts
	for role in ROLES:
		result.append(float(ally_roles.get(role, 0)))
	# Add enemy role counts
	for role in ROLES:
		result.append(float(enemy_roles.get(role, 0)))
	
	# Add precomputed synergy/counter features if available
	if not syn_cnt.is_empty():
		result.append(syn_cnt.get("avg_synergy", 0.5))
		result.append(syn_cnt.get("avg_counter", 0.5))
		result.append(syn_cnt.get("synergy_variance", 0.0))
		result.append(syn_cnt.get("counter_variance", 0.0))
	else:
		# Use computed values
		result.append(avg_synergy)
		result.append(avg_counter)
		result.append(synergy_variance)
		result.append(counter_variance)
	
	# Add composition balance features if available
	if not comp_balance.is_empty():
		result.append(comp_balance.get("ally_role_diversity", 0.0))
		result.append(comp_balance.get("enemy_role_diversity", 0.0))
		result.append(comp_balance.get("ally_tank_ratio", 0.0))
		result.append(comp_balance.get("enemy_tank_ratio", 0.0))
		result.append(comp_balance.get("ally_support_ratio", 0.0))
		result.append(comp_balance.get("enemy_support_ratio", 0.0))
		result.append(comp_balance.get("ally_damage_ratio", 0.0))
		result.append(comp_balance.get("enemy_damage_ratio", 0.0))
	else:
		# Add zeros for compatibility
		for i in range(8):
			result.append(0.0)
	
	return result


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
		var result: Array[float] = []
		for i in range(FEATURE_DIM):
			result.append(0.0)
		return result
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
		var result: Array[float] = []
		for i in range(FEATURE_DIM):
			result.append(1.0)
		return result
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
	if features.is_empty():
		push_error("verify_draft_aware_signal: cannot train on empty features")
		return []
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


func recommend_ban(allies: Array, enemies: Array, banned_allies: Array, banned_enemies: Array, available: Array, combat_stats: Dictionary, synergy_stats: Dictionary, counter_stats: Dictionary, weights: Array[float], team_banning: String) -> Dictionary:
	# TODO: Replace with ban-specific model training and recommendation
	# Current simplified model does not support ban-related features
	# Returns: {champion: StringName, predicted_win_rate: float, all_predictions: Dictionary}
	
	if available.is_empty():
		return {
			"champion": "",
			"predicted_win_rate": 0.5,
			"all_predictions": {},
			"team_banning": team_banning
		}
	
	# Random fallback until ban model is implemented
	var random_index := randi() % available.size()
	var random_champion: StringName = available[random_index]
	
	return {
		"champion": random_champion,
		"predicted_win_rate": 0.5,
		"all_predictions": {},
		"team_banning": team_banning
	}
