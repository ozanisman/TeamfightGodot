extends SceneTree

## Train draft-aware logistic regression models with draft position features.
##
## This script trains depth-specific models (1-5) that include draft_position
## as a feature. Features: base, synergy, counter, base*synergy, base*counter,
## synergy*counter, draft_position (per team).

const LEARNING_RATE: float = 0.05
const MAX_ITERATIONS: int = 5000
const CONVERGENCE_TOLERANCE: float = 1e-6
const L2_REGULARIZATION: float = 0.001
const SMOOTHING_K: float = 10.0
const PRIOR_WINRATE: float = 0.5


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	var training_input := _extract_argument("--training-input=", "res://stats_output/draft_training.csv")
	var stats_dir := _extract_argument("--stats-dir=", "res://stats_output")
	var output_path := _extract_argument("--output=", "res://stats_output/draft_aware_model.json")
	
	print("verify_draft_aware_signal: loading training data from %s" % training_input)
	var training_data := _load_training_csv(training_input)
	if training_data.is_empty():
		push_error("verify_draft_aware_signal: failed to load training data")
		quit(1)
		return
	
	print("verify_draft_aware_signal: loading stats from %s" % stats_dir)
	var combat_stats := _load_combat_stats(stats_dir + "/combat_stats.csv")
	var synergy_stats := _load_matchup_stats(stats_dir + "/matchup_with.csv")
	var counter_stats := _load_matchup_stats(stats_dir + "/matchup_vs.csv")
	print("verify_draft_aware_signal: loaded %d training rows, %d champions, %d synergy maps, %d counter maps" % [
		training_data.size(), combat_stats.size(), synergy_stats.size(), counter_stats.size()
	])
	
	# Group training data by depth
	var depth_groups: Dictionary = {}
	for row in training_data:
		var depth := row["depth"]
		if not depth in depth_groups:
			depth_groups[depth] = []
		depth_groups[depth].append(row)
	
	# Train model per depth
	var models: Dictionary = {}
	for depth in depth_groups.keys():
		print("verify_draft_aware_signal: training depth %d model (%d samples)" % [depth, depth_groups[depth].size()])
		var model := _train_depth_model(depth_groups[depth], combat_stats, synergy_stats, counter_stats, depth)
		models[depth] = model
	
	# Output model weights
	_write_model_json(output_path, models)
	print("verify_draft_aware_signal: model weights written to %s" % output_path)
	quit(0)


func _load_training_csv(path: String) -> Array:
	var result: Array = []
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		push_error("Failed to open %s" % path)
		return result
	var header: PackedStringArray = f.get_csv_line()
	var depth_col: int = header.find("depth")
	var draft_position_col: int = header.find("draft_position")
	var allies_col: int = header.find("allies")
	var enemies_col: int = header.find("enemies")
	var winrate_col: int = header.find("winrate")
	if depth_col < 0 or allies_col < 0 or enemies_col < 0 or winrate_col < 0:
		push_error("Missing required columns in training CSV")
		f.close()
		return result
	while not f.eof_reached():
		var line: PackedStringArray = f.get_csv_line()
		if line.size() <= 1:
			continue
		result.append({
			"depth": int(line[depth_col]),
			"draft_position": int(line[draft_position_col]) if draft_position_col >= 0 else 0,
			"allies": Array(line[allies_col].split("|")),
			"enemies": Array(line[enemies_col].split("|")),
			"winrate": float(line[winrate_col]),
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
		result[line[hero_col]] = {
			"winrate": float(line[winrate_col]),
			"samples": int(line[total_games_col]),
		}
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
		if not champion in result:
			result[champion] = {}
		result[champion][other] = {
			"winrate": float(line[winrate_col]),
			"samples": int(line[wins_col]) + int(line[losses_col]) if wins_col >= 0 and losses_col >= 0 else 0,
		}
	f.close()
	return result


func _apply_bayesian_smoothing(raw_winrate: float, samples: int) -> float:
	if samples <= 0:
		return PRIOR_WINRATE
	var prior := PRIOR_WINRATE
	var N := 10  # confidence_prior_samples
	return (raw_winrate * samples + prior * N) / (samples + N)


func _extract_team_features(team: Array, opponents: Array, combat_stats: Dictionary, synergy_stats: Dictionary, counter_stats: Dictionary) -> Dictionary:
	var base_sum := 0.0
	var synergy_sum := 0.0
	var counter_sum := 0.0
	var n := float(team.size())
	
	for champion in team:
		# Base winrate
		if champion in combat_stats:
			var stat = combat_stats[champion]
			base_sum += _apply_bayesian_smoothing(stat["winrate"], stat["samples"])
		else:
			base_sum += PRIOR_WINRATE
		
		# Synergy (vs allies)
		for ally in team:
			if ally == champion:
				continue
			if champion in synergy_stats and ally in synergy_stats[champion]:
				var stat = synergy_stats[champion][ally]
				synergy_sum += _apply_bayesian_smoothing(stat["winrate"], stat["samples"])
		
		# Counter (vs opponents)
		for enemy in opponents:
			if champion in counter_stats and enemy in counter_stats[champion]:
				var stat = counter_stats[champion][enemy]
				counter_sum += _apply_bayesian_smoothing(stat["winrate"], stat["samples"])
	
	var base := base_sum / n if n > 0 else PRIOR_WINRATE
	var synergy := synergy_sum / (n * (n - 1)) if n > 1 else PRIOR_WINRATE
	var counter := counter_sum / (n * float(opponents.size())) if n > 0 and opponents.size() > 0 else PRIOR_WINRATE
	
	return {
		"base": base,
		"synergy": synergy,
		"counter": counter,
		"base_synergy": base * synergy,
		"base_counter": base * counter,
		"synergy_counter": synergy * counter,
	}


func _train_depth_model(training_data: Array, combat_stats: Dictionary, synergy_stats: Dictionary, counter_stats: Dictionary, depth: int) -> Dictionary:
	# Extract features for each training sample
	var features: Array = []
	var labels: Array = []
	var draft_positions: Array = []
	
	for row in training_data:
		var allies: Array = row["allies"]
		var enemies: Array = row["enemies"]
		var draft_position: int = row["draft_position"]
		var winrate: float = row["winrate"]
		
		var team_a_features := _extract_team_features(allies, enemies, combat_stats, synergy_stats, counter_stats)
		var team_b_features := _extract_team_features(enemies, allies, combat_stats, synergy_stats, counter_stats)
		
		# Features: [base_a, synergy_a, counter_a, base_synergy_a, base_counter_a, synergy_counter_a,
		#            base_b, synergy_b, counter_b, base_synergy_b, base_counter_b, synergy_counter_b,
		#            draft_position]
		var feature_vector: Array = [
			team_a_features["base"], team_a_features["synergy"], team_a_features["counter"],
			team_a_features["base_synergy"], team_a_features["base_counter"], team_a_features["synergy_counter"],
			team_b_features["base"], team_b_features["synergy"], team_b_features["counter"],
			team_b_features["base_synergy"], team_b_features["base_counter"], team_b_features["synergy_counter"],
			float(draft_position) / 10.0,  # Normalize to [0, 1]
		]
		
		features.append(feature_vector)
		labels.append(winrate)
		draft_positions.append(draft_position)
	
	# Calculate feature means and stddevs
	var n_features := features[0].size()
	var means: Array = []
	var stddevs: Array = []
	
	for i in range(n_features):
		var values: Array = []
		for f in features:
			values.append(f[i])
		var mean: float = 0.0
		for v in values:
			mean += v
		mean /= float(values.size())
		
		var variance: float = 0.0
		for v in values:
			variance += (v - mean) * (v - mean)
		variance /= float(values.size())
		var stddev: float = sqrt(variance) if variance > 0 else 1.0
		
		means.append(mean)
		stddevs.append(stddev)
	
	# Normalize features
	var normalized_features: Array = []
	for f in features:
		var normalized: Array = []
		for i in range(n_features):
			normalized.append((f[i] - means[i]) / stddevs[i])
		normalized_features.append(normalized)
	
	# Train logistic regression using gradient descent
	var weights: Array = []
	for i in range(n_features + 1):  # +1 for bias
		weights.append(0.0)
	
	for iteration in range(MAX_ITERATIONS):
		var gradient: Array = []
		for i in range(n_features + 1):
			gradient.append(0.0)
		
		for i in range(normalized_features.size()):
			var x: Array = normalized_features[i]
			var y: float = labels[i]
			
			# Compute prediction
			var z: float = weights[0]  # bias
			for j in range(n_features):
				z += weights[j + 1] * x[j]
			var p: float = 1.0 / (1.0 + exp(-z))
			
			# Compute gradient
			gradient[0] += (p - y)
			for j in range(n_features):
				gradient[j + 1] += (p - y) * x[j]
		
		# Update weights with L2 regularization
		var max_update: float = 0.0
		for i in range(n_features + 1):
			var reg_term: float = L2_REGULARIZATION * weights[i] if i > 0 else 0.0
			var update: float = (gradient[i] / float(normalized_features.size()) + reg_term) * LEARNING_RATE
			weights[i] -= update
			max_update = max(max_update, abs(update))
		
		if max_update < CONVERGENCE_TOLERANCE:
			print("verify_draft_aware_signal: converged at iteration %d" % iteration)
			break
	
	return {
		"depth": depth,
		"weights": weights,
		"means": means,
		"stddevs": stddevs,
		"n_samples": training_data.size(),
	}


func _write_model_json(path: String, models: Dictionary) -> void:
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.WRITE)
	if f == null:
		push_error("Failed to open %s for writing" % path)
		return
	
	f.store_string(JSON.stringify(models, "\t"))
	f.close()
