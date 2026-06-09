extends SceneTree

## Evaluate partial draft model vs certified model extrapolation.
##
## Compares hybrid model (partial for depths 1-3, certified for depth 4)
## against certified model extrapolation on the same test set.

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

const DEFAULT_TRAINING_INPUT := "res://stats_output_partial/partial_draft_training_full_50.csv"
const DEFAULT_OUTPUT := "res://stats_output_partial/partial_draft_evaluation.csv"


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

	var training_input := _extract_argument("--training-input=", DEFAULT_TRAINING_INPUT)
	var stats_dir := _extract_argument("--stats-dir=", "res://stats_output")
	var output_path := _extract_argument("--output=", DEFAULT_OUTPUT)

	print("evaluate_partial_draft_model: loading training data from %s" % training_input)
	var training_data := _load_training_csv(training_input)
	if training_data.is_empty():
		push_error("evaluate_partial_draft_model: failed to load training data")
		quit(1)
		return

	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("evaluate_partial_draft_model: native simulation backend unavailable")
		quit(1)
		return

	# Group data by depth and split train/test
	var data_by_depth := {}
	for depth in range(1, 5):
		data_by_depth[depth] = []
	for row: Dictionary in training_data:
		var depth := int(row["depth"])
		if depth in data_by_depth:
			data_by_depth[depth].append(row)

	var csv_lines: Array[String] = [
		"depth,partial_accuracy,partial_mse,certified_accuracy,certified_mse,improvement_pp"
	]

	for depth in range(1, 5):
		var depth_data: Array = data_by_depth.get(depth, [])
		if depth_data.is_empty():
			continue

		print("evaluate_partial_draft_model: evaluating depth %d (%d samples)" % [depth, depth_data.size()])

		# Use test split (last 20%)
		var split_index := int(depth_data.size() * 0.8)
		var test_data := depth_data.slice(split_index)

		var partial_correct := 0
		var certified_correct := 0
		var hybrid_mse_sum := 0.0
		var certified_mse_sum := 0.0

		for row: Dictionary in test_data:
			var allies := (row["allies"] as String).split("|")
			var enemies := (row["enemies"] as String).split("|")
			var oracle_label := float(row["expected_win"])

			# Hybrid model prediction (partial for depths 1-3, certified for depth 4)
			var hybrid_pred_dict := backend.predict_draft_winner_hybrid(allies, enemies, stats_dir)
			var hybrid_pred := float(hybrid_pred_dict.get("team1_prob", 0.5))

			# Certified model prediction (extrapolation)
			var certified_pred_dict := backend.predict_draft_winner(allies, enemies, stats_dir)
			var certified_pred := float(certified_pred_dict.get("team1_prob", 0.5))

			# Compare
			var hybrid_label := 1 if hybrid_pred >= 0.5 else 0
			var certified_label := 1 if certified_pred >= 0.5 else 0
			var oracle_label_bin := 1 if oracle_label >= 0.5 else 0

			if hybrid_label == oracle_label_bin:
				partial_correct += 1
			if certified_label == oracle_label_bin:
				certified_correct += 1

			hybrid_mse_sum += (hybrid_pred - oracle_label) * (hybrid_pred - oracle_label)
			certified_mse_sum += (certified_pred - oracle_label) * (certified_pred - oracle_label)

		var n := float(test_data.size())
		var hybrid_acc := float(partial_correct) / n
		var certified_acc := float(certified_correct) / n
		var hybrid_mse := hybrid_mse_sum / n
		var certified_mse := certified_mse_sum / n
		var improvement_pp := (hybrid_acc - certified_acc) * 100.0

		csv_lines.append("%d,%.4f,%.6f,%.4f,%.6f,%.2f" % [
			depth, hybrid_acc, hybrid_mse, certified_acc, certified_mse, improvement_pp
		])

	# Write results
	var f := FileAccess.open(ProjectSettings.globalize_path(output_path), FileAccess.WRITE)
	if f == null:
		push_error("evaluate_partial_draft_model: could not open %s" % output_path)
		quit(1)
		return
	f.store_string("\n".join(csv_lines) + "\n")
	f.close()

	if backend.has_method("clear"):
		backend.call("clear")

	print("")
	print("================ HYBRID MODEL EVALUATION COMPLETE ================")
	print("CSV written to: %s" % output_path)
	print("====================================================================")
	quit(0)


func _load_training_csv(path: String) -> Array:
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		push_error("evaluate_partial_draft_model: could not open %s" % path)
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
