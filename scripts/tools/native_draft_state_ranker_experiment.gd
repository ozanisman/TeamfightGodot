extends SceneTree

## Validation-only candidate-wide draft-state ranker experiment.

const DraftValidationCsvScript := preload("res://scripts/tools/draft_validation_csv.gd")
const DraftStateScorerModelScript := preload("res://scripts/tools/draft_state_scorer_model.gd")

const DEFAULT_INPUT_PATH: String = "res://model_stats/draft_state_candidate_rows.csv"
const DEFAULT_OUTPUT_DIR: String = "res://model_stats/draft_state_ranker_experiments/smoke"
const DEFAULT_TRAIN_FRACTION: float = 0.8
const DEFAULT_SPLIT_SEED: int = 1337
const DEFAULT_MIN_GROUPS: int = 20
const DEFAULT_CALIBRATION_BINS: int = 10
const MIN_SEGMENT_GROUPS: int = 20
const MIN_SEGMENT_TEST_GROUPS: int = 4
const LEARNING_RATE: float = 0.05
const MAX_ITERATIONS: int = 300
const CONVERGENCE_TOLERANCE: float = 0.000001
const L2_REGULARIZATION: float = 0.001

const REQUIRED_COLUMNS: Array[String] = [
	"draft_seed",
	"pairing_index",
	"step_index",
	"step_action",
	"acting_side",
	"blue_picks_before",
	"red_picks_before",
	"blue_bans_before",
	"red_bans_before",
	"legal_pool",
	"legal_pool_count",
	"selected",
	"candidate",
	"is_selected",
	"candidate_rank_native",
	"blue_winrate",
	"red_winrate",
	"base_power",
	"ally_synergy",
	"enemy_counter_value",
	"counter_risk",
	"role_fit",
	"comp_fit",
	"total_score",
	"phase_label",
	"candidate_role",
]


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value


func _has_flag(flag: String) -> bool:
	for a in OS.get_cmdline_user_args():
		if str(a) == flag:
			return true
	return false


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	await process_frame

	if _has_flag("--self-test"):
		var self_test: Dictionary = _run_self_test()
		if not bool(self_test.get("ok", false)):
			push_error("native_draft_state_ranker_experiment self-test failed: %s" % String(self_test.get("error", "")))
			quit(1)
			return
		print("native_draft_state_ranker_experiment: self-test PASS")
		quit(0)
		return

	var input_path: String = _extract_argument("--input=", DEFAULT_INPUT_PATH)
	var output_dir: String = _extract_argument("--output-dir=", DEFAULT_OUTPUT_DIR)
	var train_fraction: float = clampf(float(_extract_argument("--train-fraction=", str(DEFAULT_TRAIN_FRACTION))), 0.1, 0.95)
	var split_seed: int = int(_extract_argument("--seed=", str(DEFAULT_SPLIT_SEED)))
	var min_groups: int = maxi(1, int(_extract_argument("--min-groups=", str(DEFAULT_MIN_GROUPS))))
	var calibration_bins: int = maxi(1, int(_extract_argument("--calibration-bins=", str(DEFAULT_CALIBRATION_BINS))))
	var segment_probes_enabled: bool = _extract_argument("--segment-probes=", "false") == "true"

	var rows: Array[Dictionary] = _load_rows(input_path)
	var grouped: Dictionary = _group_rows(rows)
	if grouped.size() < min_groups:
		push_error("native_draft_state_ranker_experiment: input has %d groups, min-groups=%d" % [grouped.size(), min_groups])
		quit(1)
		return

	var result: Dictionary = _run_experiment(rows, grouped, input_path, output_dir, train_fraction, split_seed, calibration_bins, segment_probes_enabled)
	if not bool(result.get("ok", false)):
		push_error("native_draft_state_ranker_experiment: %s" % String(result.get("error", "")))
		quit(1)
		return
	var status: String = String(result.get("status", "FAIL"))
	print("native_draft_state_ranker_experiment: %s" % status)
	print("native_draft_state_ranker_experiment: wrote %s" % output_dir)
	quit(0)


func _load_rows(path: String) -> Array[Dictionary]:
	var rows: Array[Dictionary] = []
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		push_error("native_draft_state_ranker_experiment: could not open %s" % path)
		return rows

	var header: Array = DraftValidationCsvScript.split_csv_line(f.get_line())
	var idx: Dictionary = DraftValidationCsvScript.header_index(header)
	for column in REQUIRED_COLUMNS:
		if not idx.has(column):
			push_error("native_draft_state_ranker_experiment: missing required column %s" % column)
			f.close()
			return []

	while not f.eof_reached():
		var line: String = f.get_line()
		if line.strip_edges().is_empty():
			continue
		var fields: Array = DraftValidationCsvScript.split_csv_line(line)
		var row: Dictionary = {}
		for key in idx.keys():
			var col: int = int(idx[key])
			row[key] = String(fields[col]) if col < fields.size() else ""
		if _row_is_valid(row):
			rows.append(row)
	f.close()
	return rows


func _row_is_valid(row: Dictionary) -> bool:
	for column in ["step_index", "legal_pool_count", "is_selected", "candidate_rank_native"]:
		if not String(row.get(column, "")).strip_edges().is_valid_int():
			return false
	for column in ["blue_winrate", "red_winrate"]:
		if not String(row.get(column, "")).strip_edges().is_valid_float():
			return false
	for column in DraftStateScorerModelScript.NUMERIC_FEATURE_COLUMNS:
		if not String(row.get(column, "")).strip_edges().is_valid_float():
			return false
	var acting_side: String = String(row.get("acting_side", "")).strip_edges()
	return acting_side == "blue" or acting_side == "red"


func _run_experiment(
	rows: Array[Dictionary],
	grouped: Dictionary,
	input_path: String,
	output_dir: String,
	train_fraction: float,
	split_seed: int,
	calibration_bins: int,
	segment_probes_enabled: bool
) -> Dictionary:
	var categories: Dictionary = _collect_categories(rows)
	var feature_names: Array[String] = DraftStateScorerModelScript.feature_names_for_categories(categories)
	var feature_data: Array[Dictionary] = _extract_feature_data(rows, categories)
	var split: Dictionary = _split_group_keys(grouped.keys(), train_fraction, split_seed)
	var row_split: Dictionary = _row_indices_for_group_split(feature_data, split)
	if Array(row_split["train"]).is_empty() or Array(row_split["test"]).is_empty():
		return {"ok": false, "error": "split produced empty train or test rows"}

	var choice_scaler: Dictionary = _fit_scaler(feature_data, row_split["train"], "features")
	var choice_model: Dictionary = _train_logistic(feature_data, row_split["train"], "features", choice_scaler, "choice_target")

	var selected_train: Array[int] = _selected_indices(feature_data, row_split["train"])
	var selected_test: Array[int] = _selected_indices(feature_data, row_split["test"])
	var selected_all: Array[int] = _selected_indices(feature_data, row_split["all"])
	if selected_train.is_empty() or selected_test.is_empty():
		return {"ok": false, "error": "split produced empty selected train or test rows"}

	var outcome_scaler: Dictionary = _fit_scaler(feature_data, selected_train, "features")
	var outcome_model: Dictionary = _train_logistic(feature_data, selected_train, "features", outcome_scaler, "outcome_target")
	var native_outcome_scaler: Dictionary = _fit_scaler(feature_data, selected_train, "native_features")
	var native_outcome_model: Dictionary = _train_logistic(feature_data, selected_train, "native_features", native_outcome_scaler, "outcome_target")

	var ranking_metrics: Dictionary = {
		"train": _ranking_metrics_for_groups(feature_data, split["train"], choice_model, choice_scaler),
		"test": _ranking_metrics_for_groups(feature_data, split["test"], choice_model, choice_scaler),
		"all": _ranking_metrics_for_groups(feature_data, split["all"], choice_model, choice_scaler),
	}
	var grouped_ranking_diagnostics: Dictionary = _grouped_ranking_diagnostics(feature_data, split["test"], choice_model, choice_scaler)
	var selected_outcome_metrics: Dictionary = {
		"train": _selected_outcome_metrics(feature_data, selected_train, outcome_model, outcome_scaler, native_outcome_model, native_outcome_scaler),
		"test": _selected_outcome_metrics(feature_data, selected_test, outcome_model, outcome_scaler, native_outcome_model, native_outcome_scaler),
		"all": _selected_outcome_metrics(feature_data, selected_all, outcome_model, outcome_scaler, native_outcome_model, native_outcome_scaler),
	}
	var selected_calibration: Dictionary = _selected_outcome_calibration(
		feature_data, selected_test, outcome_model, outcome_scaler, native_outcome_model, native_outcome_scaler, calibration_bins
	)

	var test_metrics: Dictionary = ranking_metrics["test"]
	var learned_metrics: Dictionary = test_metrics["learned_model"]
	var native_metrics: Dictionary = test_metrics["native_total_score_baseline"]
	var top1_delta: float = float(learned_metrics["top1_selected_agreement"]) - float(native_metrics["top1_selected_agreement"])
	var mrr_delta: float = float(learned_metrics["mrr"]) - float(native_metrics["mrr"])
	var mean_rank_delta: float = float(learned_metrics["mean_selected_rank"]) - float(native_metrics["mean_selected_rank"])
	var status: String = _ranker_status(top1_delta, mrr_delta, mean_rank_delta)
	var segment_probes: Array[Dictionary] = []
	if segment_probes_enabled:
		segment_probes = _run_segment_probes(feature_data, train_fraction, split_seed)

	var output_ok: bool = _write_outputs(
		output_dir, input_path, rows.size(), grouped.size(), train_fraction, split_seed, calibration_bins,
		status, top1_delta, mrr_delta, mean_rank_delta, feature_names, categories, choice_model, choice_scaler,
		outcome_model, outcome_scaler, native_outcome_model, native_outcome_scaler,
		ranking_metrics, grouped_ranking_diagnostics, selected_outcome_metrics, selected_calibration,
		segment_probes, segment_probes_enabled, feature_data, split
	)
	if not output_ok:
		return {"ok": false, "error": "failed to write outputs"}
	return {"ok": true, "status": status}


func _collect_categories(rows: Array[Dictionary]) -> Dictionary:
	var raw: Dictionary = {
		"candidate_role": {},
		"phase_label": {},
		"step_action": {},
		"acting_side": {},
	}
	for row in rows:
		for key in raw.keys():
			var value: String = String(row.get(key, "")).strip_edges()
			if value.is_empty():
				value = "unknown"
			raw[key][value] = true
	var categories: Dictionary = {}
	for key in raw.keys():
		var values: Array[String] = []
		for value in Dictionary(raw[key]).keys():
			values.append(String(value))
		values.sort()
		categories[key] = values
	return categories


func _extract_feature_data(rows: Array[Dictionary], categories: Dictionary) -> Array[Dictionary]:
	var data: Array[Dictionary] = []
	for i in range(rows.size()):
		var row: Dictionary = rows[i]
		var native_features := PackedFloat32Array()
		native_features.append(float(row.get("total_score", "0.0")))
		data.append({
			"row_index": i,
			"group_key": _group_key(row),
			"step_action": String(row.get("step_action", "")),
			"acting_side": String(row.get("acting_side", "")),
			"phase_label": String(row.get("phase_label", "")),
			"step_band": _step_band(int(row.get("step_index", "0"))),
			"candidate": String(row.get("candidate", "")),
			"selected": String(row.get("selected", "")),
			"is_selected": int(row.get("is_selected", "0")) == 1,
			"choice_target": float(int(row.get("is_selected", "0"))),
			"outcome_target": float(row["blue_winrate"]) if String(row["acting_side"]) == "blue" else float(row["red_winrate"]),
			"native_rank": int(row.get("candidate_rank_native", "0")),
			"native_score": float(row.get("total_score", "0.0")),
			"features": DraftStateScorerModelScript.features_for_row(row, categories),
			"native_features": native_features,
		})
	return data


func _group_rows(rows: Array[Dictionary]) -> Dictionary:
	var grouped: Dictionary = {}
	for row in rows:
		var key: String = _group_key(row)
		if not grouped.has(key):
			grouped[key] = []
		grouped[key].append(row)
	return grouped


func _group_key(row: Dictionary) -> String:
	return "%s|%s|%s" % [
		String(row.get("draft_seed", "")),
		String(row.get("pairing_index", "")),
		String(row.get("step_index", "")),
	]


func _step_band(step_index: int) -> String:
	var start: int = floori(float(clampi(step_index, 0, 19)) / 5.0) * 5
	return "%d-%d" % [start, start + 4]


func _split_group_keys(raw_keys: Array, train_fraction: float, split_seed: int) -> Dictionary:
	var keys: Array[String] = []
	for key in raw_keys:
		keys.append(String(key))
	keys.sort()
	var rng := RandomNumberGenerator.new()
	rng.seed = split_seed
	for i in range(keys.size() - 1, 0, -1):
		var j: int = rng.randi_range(0, i)
		var tmp: String = keys[i]
		keys[i] = keys[j]
		keys[j] = tmp
	var train_size: int = clampi(int(round(float(keys.size()) * train_fraction)), 1, keys.size() - 1)
	return {"train": keys.slice(0, train_size), "test": keys.slice(train_size), "all": keys}


func _row_indices_for_group_split(feature_data: Array[Dictionary], split: Dictionary) -> Dictionary:
	var train_keys: Dictionary = _set_from_array(split["train"])
	var test_keys: Dictionary = _set_from_array(split["test"])
	var train_indices: Array[int] = []
	var test_indices: Array[int] = []
	var all_indices: Array[int] = []
	for i in range(feature_data.size()):
		var key: String = String(feature_data[i]["group_key"])
		if train_keys.has(key):
			train_indices.append(i)
		elif test_keys.has(key):
			test_indices.append(i)
		all_indices.append(i)
	return {"train": train_indices, "test": test_indices, "all": all_indices}


func _set_from_array(values: Array) -> Dictionary:
	var out: Dictionary = {}
	for value in values:
		out[String(value)] = true
	return out


func _selected_indices(feature_data: Array[Dictionary], indices: Array) -> Array[int]:
	var out: Array[int] = []
	for idx_var in indices:
		var idx: int = int(idx_var)
		if bool(feature_data[idx]["is_selected"]):
			out.append(idx)
	return out


func _fit_scaler(feature_data: Array[Dictionary], indices: Array, feature_key: String) -> Dictionary:
	var feature_count: int = PackedFloat32Array(feature_data[int(indices[0])][feature_key]).size()
	var means := PackedFloat32Array()
	var stddevs := PackedFloat32Array()
	means.resize(feature_count)
	stddevs.resize(feature_count)
	for idx_var in indices:
		var features: PackedFloat32Array = feature_data[int(idx_var)][feature_key]
		for i in range(feature_count):
			means[i] += features[i]
	for i in range(feature_count):
		means[i] /= float(indices.size())
	for idx_var in indices:
		var features: PackedFloat32Array = feature_data[int(idx_var)][feature_key]
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
	var out := PackedFloat32Array()
	out.resize(raw.size())
	for i in range(raw.size()):
		out[i] = (raw[i] - means[i]) / stddevs[i]
	return out


func _train_logistic(feature_data: Array[Dictionary], train_indices: Array, feature_key: String, scaler: Dictionary, target_key: String, max_iterations: int = MAX_ITERATIONS) -> Dictionary:
	var num_features: int = PackedFloat32Array(feature_data[int(train_indices[0])][feature_key]).size()
	var train_features: Array[PackedFloat32Array] = []
	var train_targets: Array[float] = []
	for idx_var in train_indices:
		var item: Dictionary = feature_data[int(idx_var)]
		train_features.append(_standardized_features(item[feature_key], scaler))
		train_targets.append(float(item[target_key]))
	var weights := PackedFloat32Array()
	weights.resize(num_features)
	var bias: float = 0.0
	for _iteration in range(max_iterations):
		var grad_w := PackedFloat32Array()
		grad_w.resize(num_features)
		var grad_b: float = 0.0
		for train_index in range(train_features.size()):
			var features: PackedFloat32Array = train_features[train_index]
			var pred: float = _predict_logistic(weights, bias, features)
			var error: float = pred - train_targets[train_index]
			for i in range(num_features):
				grad_w[i] += error * features[i]
			grad_b += error
		var max_delta: float = 0.0
		for i in range(num_features):
			grad_w[i] = grad_w[i] / float(train_features.size()) + L2_REGULARIZATION * weights[i]
			var delta: float = LEARNING_RATE * grad_w[i]
			weights[i] -= delta
			max_delta = maxf(max_delta, absf(delta))
		bias -= LEARNING_RATE * grad_b / float(train_features.size())
		if max_delta < CONVERGENCE_TOLERANCE:
			break
	return {"weights": weights, "bias": bias}


func _ranking_metrics_for_groups(feature_data: Array[Dictionary], group_keys: Array, choice_model: Dictionary, choice_scaler: Dictionary) -> Dictionary:
	var native_ranks: Array[float] = []
	var learned_ranks: Array[float] = []
	var random_top1_values: Array[float] = []
	var random_ranks: Array[float] = []
	var random_mrr_values: Array[float] = []
	var rows_by_group: Dictionary = _feature_rows_by_group(feature_data, group_keys)
	for key in rows_by_group.keys():
		var rows: Array = rows_by_group[key]
		var native_rank: int = _selected_native_rank(rows)
		var learned_rank: int = _selected_learned_rank(rows, choice_model, choice_scaler)
		native_ranks.append(float(native_rank))
		learned_ranks.append(float(learned_rank))
		random_top1_values.append(1.0 / float(rows.size()))
		random_ranks.append((float(rows.size()) + 1.0) / 2.0)
		random_mrr_values.append(_harmonic(float(rows.size())) / float(rows.size()))
	return {
		"native_total_score_baseline": _rank_metric_summary(native_ranks),
		"learned_model": _rank_metric_summary(learned_ranks),
		"random_baseline": {
			"groups": random_top1_values.size(),
			"top1_selected_agreement": _mean_float(random_top1_values),
			"mean_selected_rank": _mean_float(random_ranks),
			"mrr": _mean_float(random_mrr_values),
		},
	}


func _grouped_ranking_diagnostics(feature_data: Array[Dictionary], group_keys: Array, choice_model: Dictionary, choice_scaler: Dictionary) -> Dictionary:
	return {
		"step_action": _grouped_ranking_diagnostics_for_field(feature_data, group_keys, choice_model, choice_scaler, "step_action"),
		"acting_side": _grouped_ranking_diagnostics_for_field(feature_data, group_keys, choice_model, choice_scaler, "acting_side"),
		"phase_label": _grouped_ranking_diagnostics_for_field(feature_data, group_keys, choice_model, choice_scaler, "phase_label"),
		"step_band": _grouped_ranking_diagnostics_for_field(feature_data, group_keys, choice_model, choice_scaler, "step_band"),
	}


func _grouped_ranking_diagnostics_for_field(
	feature_data: Array[Dictionary],
	group_keys: Array,
	choice_model: Dictionary,
	choice_scaler: Dictionary,
	field_name: String
) -> Dictionary:
	var keys_by_value: Dictionary = {}
	var seen_groups: Dictionary = {}
	var allowed: Dictionary = _set_from_array(group_keys)
	for item in feature_data:
		var group_key: String = String(item["group_key"])
		if not allowed.has(group_key) or seen_groups.has(group_key):
			continue
		seen_groups[group_key] = true
		var value: String = String(item.get(field_name, "unknown"))
		if value.is_empty():
			value = "unknown"
		if not keys_by_value.has(value):
			keys_by_value[value] = []
		keys_by_value[value].append(group_key)
	var out: Dictionary = {}
	for value in _sorted_keys(keys_by_value):
		var metrics: Dictionary = _ranking_metrics_for_groups(feature_data, keys_by_value[value], choice_model, choice_scaler)
		out[value] = _metrics_with_deltas(metrics)
	return out


func _metrics_with_deltas(metrics: Dictionary) -> Dictionary:
	var native_metrics: Dictionary = metrics["native_total_score_baseline"]
	var learned_metrics: Dictionary = metrics["learned_model"]
	return {
		"random_baseline": metrics["random_baseline"],
		"native_total_score_baseline": native_metrics,
		"learned_model": learned_metrics,
		"top1_delta_vs_native": float(learned_metrics["top1_selected_agreement"]) - float(native_metrics["top1_selected_agreement"]),
		"mrr_delta_vs_native": float(learned_metrics["mrr"]) - float(native_metrics["mrr"]),
		"mean_selected_rank_delta_vs_native": float(learned_metrics["mean_selected_rank"]) - float(native_metrics["mean_selected_rank"]),
	}


func _run_segment_probes(feature_data: Array[Dictionary], train_fraction: float, split_seed: int) -> Array[Dictionary]:
	var probes: Array[Dictionary] = []
	var segments: Array[Dictionary] = []
	segments.append({"field": "step_action", "value": "PICK"})
	segments.append({"field": "step_action", "value": "BAN"})
	for phase_label in _unique_group_values(feature_data, "phase_label"):
		segments.append({"field": "phase_label", "value": phase_label})
	for segment in segments:
		var field_name: String = String(segment["field"])
		var field_value: String = String(segment["value"])
		var group_keys: Array[String] = _group_keys_for_value(feature_data, field_name, field_value)
		if group_keys.size() < MIN_SEGMENT_GROUPS:
			probes.append(_skipped_segment_probe(field_name, field_value, group_keys.size(), "group_count<%d" % MIN_SEGMENT_GROUPS))
			continue
		var split: Dictionary = _split_group_keys(group_keys, train_fraction, split_seed + absi(field_value.hash()) % 100000)
		if Array(split["test"]).size() < MIN_SEGMENT_TEST_GROUPS:
			probes.append(_skipped_segment_probe(field_name, field_value, group_keys.size(), "test_group_count<%d" % MIN_SEGMENT_TEST_GROUPS))
			continue
		var row_split: Dictionary = _row_indices_for_group_split(feature_data, split)
		if Array(row_split["train"]).is_empty() or Array(row_split["test"]).is_empty():
			probes.append(_skipped_segment_probe(field_name, field_value, group_keys.size(), "empty_train_or_test_rows"))
			continue
		var choice_scaler: Dictionary = _fit_scaler(feature_data, row_split["train"], "features")
		var choice_model: Dictionary = _train_logistic(feature_data, row_split["train"], "features", choice_scaler, "choice_target")
		var metrics: Dictionary = _ranking_metrics_for_groups(feature_data, split["test"], choice_model, choice_scaler)
		var with_deltas: Dictionary = _metrics_with_deltas(metrics)
		var top1_delta: float = float(with_deltas["top1_delta_vs_native"])
		var mrr_delta: float = float(with_deltas["mrr_delta_vs_native"])
		var mean_rank_delta: float = float(with_deltas["mean_selected_rank_delta_vs_native"])
		probes.append({
			"segment_field": field_name,
			"segment_value": field_value,
			"group_count": group_keys.size(),
			"train_groups": Array(split["train"]).size(),
			"test_groups": Array(split["test"]).size(),
			"status": _ranker_status(top1_delta, mrr_delta, mean_rank_delta),
			"metrics": with_deltas,
		})
	return probes


func _skipped_segment_probe(field_name: String, field_value: String, group_count: int, reason: String) -> Dictionary:
	return {
		"segment_field": field_name,
		"segment_value": field_value,
		"group_count": group_count,
		"train_groups": 0,
		"test_groups": 0,
		"status": "SKIPPED_INSUFFICIENT_GROUPS",
		"skip_reason": reason,
		"metrics": {},
	}


func _ranker_status(top1_delta: float, mrr_delta: float, mean_rank_delta: float) -> String:
	return "PASS" if top1_delta > 0.0 and mrr_delta >= 0.0 and mean_rank_delta <= 0.0 else "VALIDATION_ONLY"


func _unique_group_values(feature_data: Array[Dictionary], field_name: String) -> Array[String]:
	var seen: Dictionary = {}
	for item in feature_data:
		var value: String = String(item.get(field_name, "unknown"))
		if value.is_empty():
			value = "unknown"
		seen[value] = true
	return _sorted_keys(seen)


func _group_keys_for_value(feature_data: Array[Dictionary], field_name: String, field_value: String) -> Array[String]:
	var seen: Dictionary = {}
	for item in feature_data:
		if String(item.get(field_name, "unknown")) == field_value:
			seen[String(item["group_key"])] = true
	return _sorted_keys(seen)


func _sorted_keys(dict: Dictionary) -> Array[String]:
	var out: Array[String] = []
	for key in dict.keys():
		out.append(String(key))
	out.sort()
	return out


func _feature_rows_by_group(feature_data: Array[Dictionary], group_keys: Array) -> Dictionary:
	var allowed: Dictionary = _set_from_array(group_keys)
	var out: Dictionary = {}
	for item in feature_data:
		var key: String = String(item["group_key"])
		if not allowed.has(key):
			continue
		if not out.has(key):
			out[key] = []
		out[key].append(item)
	return out


func _selected_native_rank(rows: Array) -> int:
	for row_var in rows:
		var row: Dictionary = row_var
		if bool(row["is_selected"]):
			return int(row["native_rank"])
	return rows.size()


func _selected_learned_rank(rows: Array, choice_model: Dictionary, choice_scaler: Dictionary) -> int:
	var scored: Array[Dictionary] = []
	for row_var in rows:
		var row: Dictionary = row_var
		var pred: float = _predict_logistic(choice_model["weights"], float(choice_model["bias"]), _standardized_features(row["features"], choice_scaler))
		scored.append({"row": row, "pred": pred})
	scored.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		if float(a["pred"]) != float(b["pred"]):
			return float(a["pred"]) > float(b["pred"])
		var ar: Dictionary = a["row"]
		var br: Dictionary = b["row"]
		if float(ar["native_score"]) != float(br["native_score"]):
			return float(ar["native_score"]) > float(br["native_score"])
		return String(ar["candidate"]) < String(br["candidate"])
	)
	for i in range(scored.size()):
		var row: Dictionary = scored[i]["row"]
		if bool(row["is_selected"]):
			return i + 1
	return scored.size()


func _rank_metric_summary(ranks: Array[float]) -> Dictionary:
	var top1: int = 0
	var reciprocal: Array[float] = []
	for rank in ranks:
		if int(rank) == 1:
			top1 += 1
		reciprocal.append(1.0 / maxf(rank, 1.0))
	return {
		"groups": ranks.size(),
		"top1_selected_agreement": float(top1) / float(ranks.size()) if not ranks.is_empty() else 0.0,
		"mean_selected_rank": _mean_float(ranks),
		"mrr": _mean_float(reciprocal),
	}


func _selected_outcome_metrics(
	feature_data: Array[Dictionary],
	indices: Array[int],
	outcome_model: Dictionary,
	outcome_scaler: Dictionary,
	native_outcome_model: Dictionary,
	native_outcome_scaler: Dictionary
) -> Dictionary:
	return {
		"native_total_score_baseline": _outcome_metric_for_model(feature_data, indices, native_outcome_model, native_outcome_scaler, "native_features"),
		"learned_model": _outcome_metric_for_model(feature_data, indices, outcome_model, outcome_scaler, "features"),
	}


func _outcome_metric_for_model(feature_data: Array[Dictionary], indices: Array[int], model: Dictionary, scaler: Dictionary, feature_key: String) -> Dictionary:
	var mse: float = 0.0
	for idx in indices:
		var item: Dictionary = feature_data[idx]
		var pred: float = _predict_logistic(model["weights"], float(model["bias"]), _standardized_features(item[feature_key], scaler))
		var target: float = float(item["outcome_target"])
		mse += (pred - target) * (pred - target)
	return {"rows": indices.size(), "mse": mse / float(indices.size()) if not indices.is_empty() else 0.0}


func _selected_outcome_calibration(
	feature_data: Array[Dictionary],
	indices: Array[int],
	outcome_model: Dictionary,
	outcome_scaler: Dictionary,
	native_outcome_model: Dictionary,
	native_outcome_scaler: Dictionary,
	bins: int
) -> Dictionary:
	var prediction_rows: Array[Dictionary] = []
	for idx in indices:
		var item: Dictionary = feature_data[idx]
		prediction_rows.append({
			"target": float(item["outcome_target"]),
			"native_pred": _predict_logistic(native_outcome_model["weights"], float(native_outcome_model["bias"]), _standardized_features(item["native_features"], native_outcome_scaler)),
			"learned_pred": _predict_logistic(outcome_model["weights"], float(outcome_model["bias"]), _standardized_features(item["features"], outcome_scaler)),
		})
	return {
		"native_total_score_baseline": _calibration_for_key(prediction_rows, "native_pred", bins),
		"learned_model": _calibration_for_key(prediction_rows, "learned_pred", bins),
	}


func _calibration_for_key(rows: Array[Dictionary], pred_key: String, bins: int) -> Dictionary:
	var bucket_rows: Array[Array] = []
	for _i in range(bins):
		bucket_rows.append([])
	for row in rows:
		var pred: float = clampf(float(row[pred_key]), 0.0, 1.0)
		var bin_index: int = clampi(int(floor(pred * float(bins))), 0, bins - 1)
		bucket_rows[bin_index].append(row)
	var out_bins: Array[Dictionary] = []
	var weighted_abs_error: float = 0.0
	var total_rows: int = 0
	var mse: float = 0.0
	for bin_index in range(bins):
		var bucket: Array = bucket_rows[bin_index]
		var mean_pred: float = _mean_field(bucket, pred_key)
		var mean_target: float = _mean_field(bucket, "target")
		var abs_error: float = absf(mean_pred - mean_target) if not bucket.is_empty() else 0.0
		weighted_abs_error += abs_error * float(bucket.size())
		total_rows += bucket.size()
		for row_var in bucket:
			var row: Dictionary = row_var
			var diff: float = float(row[pred_key]) - float(row["target"])
			mse += diff * diff
		out_bins.append({
			"bin": bin_index,
			"n": bucket.size(),
			"mean_prediction": mean_pred,
			"mean_target": mean_target,
			"mean_abs_calibration_error": abs_error,
		})
	return {
		"summary": {
			"n": total_rows,
			"mse": mse / float(total_rows) if total_rows > 0 else 0.0,
			"mean_abs_calibration_error": weighted_abs_error / float(total_rows) if total_rows > 0 else 0.0,
		},
		"bins": out_bins,
	}


func _write_outputs(
	output_dir: String,
	input_path: String,
	row_count: int,
	group_count: int,
	train_fraction: float,
	split_seed: int,
	calibration_bins: int,
	status: String,
	top1_delta: float,
	mrr_delta: float,
	mean_rank_delta: float,
	feature_names: Array[String],
	categories: Dictionary,
	choice_model: Dictionary,
	choice_scaler: Dictionary,
	outcome_model: Dictionary,
	outcome_scaler: Dictionary,
	native_outcome_model: Dictionary,
	native_outcome_scaler: Dictionary,
	ranking_metrics: Dictionary,
	grouped_ranking_diagnostics: Dictionary,
	selected_outcome_metrics: Dictionary,
	selected_calibration: Dictionary,
	segment_probes: Array[Dictionary],
	segment_probes_enabled: bool,
	feature_data: Array[Dictionary],
	split: Dictionary
) -> bool:
	var global_output_dir: String = ProjectSettings.globalize_path(output_dir)
	if not DirAccess.dir_exists_absolute(global_output_dir):
		var err: Error = DirAccess.make_dir_recursive_absolute(global_output_dir)
		if err != OK:
			return false
	var metrics: Dictionary = {
		"status": status,
		"input_path": input_path,
		"row_count": row_count,
		"group_count": group_count,
		"train_fraction": train_fraction,
		"split_seed": split_seed,
		"calibration_bins": calibration_bins,
		"top1_delta_vs_native": top1_delta,
		"mrr_delta_vs_native": mrr_delta,
		"mean_selected_rank_delta_vs_native": mean_rank_delta,
		"ranking": ranking_metrics,
		"grouped_ranking_diagnostics": grouped_ranking_diagnostics,
		"selected_outcome": selected_outcome_metrics,
		"selected_outcome_calibration": selected_calibration,
		"segment_probes": segment_probes,
	}
	var model_json: Dictionary = {
		"model_type": "standardized_logistic_regression",
		"validation_only": true,
		"target": "is_selected",
		"feature_names": feature_names,
		"categories": categories,
		"learned_model": _model_to_json(choice_model, choice_scaler, feature_names),
		"selected_outcome_model": _model_to_json(outcome_model, outcome_scaler, feature_names),
		"selected_native_total_score_baseline": _model_to_json(native_outcome_model, native_outcome_scaler, ["total_score"]),
	}
	if not _write_text(output_dir.path_join("draft_state_ranker_metrics.json"), JSON.stringify(metrics, "\t") + "\n"):
		return false
	if not _write_text(output_dir.path_join("draft_state_ranker_model.json"), JSON.stringify(model_json, "\t") + "\n"):
		return false
	if not _write_text(output_dir.path_join("draft_state_ranker_predictions.csv"), _rankings_csv(feature_data, split, choice_model, choice_scaler)):
		return false
	if segment_probes_enabled and not _write_text(output_dir.path_join("draft_state_ranker_segment_probes.csv"), _segment_probes_csv(segment_probes)):
		return false
	if not _write_text(output_dir.path_join("draft_state_ranker_report.md"), _report_markdown(input_path, row_count, group_count, status, ranking_metrics, grouped_ranking_diagnostics, selected_outcome_metrics, segment_probes, segment_probes_enabled, top1_delta, mrr_delta, mean_rank_delta)):
		return false
	return true


func _model_to_json(model: Dictionary, scaler: Dictionary, feature_names: Array[String]) -> Dictionary:
	return {
		"feature_names": feature_names,
		"bias": float(model["bias"]),
		"weights": _packed_to_array(model["weights"]),
		"means": _packed_to_array(scaler["means"]),
		"stddevs": _packed_to_array(scaler["stddevs"]),
	}


func _rankings_csv(feature_data: Array[Dictionary], split: Dictionary, choice_model: Dictionary, choice_scaler: Dictionary) -> String:
	var train_keys: Dictionary = _set_from_array(split["train"])
	var lines: Array[String] = ["row_index,split,group_key,candidate,selected,is_selected,candidate_rank_native,total_score,learned_choice_pred"]
	for item in feature_data:
		var key: String = String(item["group_key"])
		var pred: float = _predict_logistic(choice_model["weights"], float(choice_model["bias"]), _standardized_features(item["features"], choice_scaler))
		lines.append("%d,%s,%s,%s,%s,%d,%d,%.6f,%.6f" % [
			int(item["row_index"]),
			"train" if train_keys.has(key) else "test",
			_csv_field(key),
			_csv_field(String(item["candidate"])),
			_csv_field(String(item["selected"])),
			1 if bool(item["is_selected"]) else 0,
			int(item["native_rank"]),
			float(item["native_score"]),
			pred,
		])
	return "\n".join(lines) + "\n"


func _report_markdown(
	input_path: String,
	row_count: int,
	group_count: int,
	status: String,
	ranking_metrics: Dictionary,
	grouped_ranking_diagnostics: Dictionary,
	selected_outcome_metrics: Dictionary,
	segment_probes: Array[Dictionary],
	segment_probes_enabled: bool,
	top1_delta: float,
	mrr_delta: float,
	mean_rank_delta: float
) -> String:
	var native_test: Dictionary = ranking_metrics["test"]["native_total_score_baseline"]
	var learned_test: Dictionary = ranking_metrics["test"]["learned_model"]
	var random_test: Dictionary = ranking_metrics["test"]["random_baseline"]
	var outcome_native: Dictionary = selected_outcome_metrics["test"]["native_total_score_baseline"]
	var outcome_learned: Dictionary = selected_outcome_metrics["test"]["learned_model"]
	var lines: Array[String] = []
	lines.append("# Draft-State Ranker Experiment")
	lines.append("")
	lines.append("Input: `%s`" % input_path)
	lines.append("Rows: %d" % row_count)
	lines.append("Decision groups: %d" % group_count)
	lines.append("")
	lines.append("## Held-out Ranking")
	lines.append("| Model | Top-1 selected agreement | Mean selected rank | MRR |")
	lines.append("| --- | ---: | ---: | ---: |")
	lines.append("| random expected | %.4f | %.3f | %.4f |" % [float(random_test["top1_selected_agreement"]), float(random_test["mean_selected_rank"]), float(random_test["mrr"])])
	lines.append("| native total_score | %.4f | %.3f | %.4f |" % [float(native_test["top1_selected_agreement"]), float(native_test["mean_selected_rank"]), float(native_test["mrr"])])
	lines.append("| learned ranker | %.4f | %.3f | %.4f |" % [float(learned_test["top1_selected_agreement"]), float(learned_test["mean_selected_rank"]), float(learned_test["mrr"])])
	lines.append("")
	lines.append("Top-1 delta vs native: %.4f" % top1_delta)
	lines.append("MRR delta vs native: %.4f" % mrr_delta)
	lines.append("Mean-rank delta vs native: %.4f" % mean_rank_delta)
	lines.append("")
	lines.append("## Held-out Group Diagnostics")
	for field_name in ["step_action", "acting_side", "phase_label", "step_band"]:
		lines.append("")
		lines.append("### %s" % field_name)
		lines.append("| Value | Groups | Random top-1 | Native top-1 | Learned top-1 | Top-1 delta | Random mean rank | Native mean rank | Learned mean rank | Mean-rank delta | Random MRR | Native MRR | Learned MRR | MRR delta |")
		lines.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
		var field_metrics: Dictionary = grouped_ranking_diagnostics.get(field_name, {})
		for value in _sorted_keys(field_metrics):
			var item: Dictionary = field_metrics[value]
			var random_row: Dictionary = item["random_baseline"]
			var native_row: Dictionary = item["native_total_score_baseline"]
			var learned_row: Dictionary = item["learned_model"]
			lines.append("| %s | %d | %.4f | %.4f | %.4f | %.4f | %.3f | %.3f | %.3f | %.3f | %.4f | %.4f | %.4f | %.4f |" % [
				value,
				int(native_row["groups"]),
				float(random_row["top1_selected_agreement"]),
				float(native_row["top1_selected_agreement"]),
				float(learned_row["top1_selected_agreement"]),
				float(item["top1_delta_vs_native"]),
				float(random_row["mean_selected_rank"]),
				float(native_row["mean_selected_rank"]),
				float(learned_row["mean_selected_rank"]),
				float(item["mean_selected_rank_delta_vs_native"]),
				float(random_row["mrr"]),
				float(native_row["mrr"]),
				float(learned_row["mrr"]),
				float(item["mrr_delta_vs_native"]),
			])
	lines.append("")
	if segment_probes_enabled:
		lines.append("## Segment Probes")
		lines.append("| Segment | Groups | Test groups | Status | Top-1 delta | MRR delta | Mean-rank delta |")
		lines.append("| --- | ---: | ---: | --- | ---: | ---: | ---: |")
		for probe in segment_probes:
			var metrics: Dictionary = probe.get("metrics", {})
			lines.append("| %s=%s | %d | %d | %s | %.4f | %.4f | %.4f |" % [
				String(probe.get("segment_field", "")),
				String(probe.get("segment_value", "")),
				int(probe.get("group_count", 0)),
				int(probe.get("test_groups", 0)),
				String(probe.get("status", "")),
				float(metrics.get("top1_delta_vs_native", 0.0)),
				float(metrics.get("mrr_delta_vs_native", 0.0)),
				float(metrics.get("mean_selected_rank_delta_vs_native", 0.0)),
			])
		lines.append("")
	lines.append("## Selected-Row Outcome Diagnostic")
	lines.append("- native total_score MSE: %.6f" % float(outcome_native["mse"]))
	lines.append("- learned selected-outcome MSE: %.6f" % float(outcome_learned["mse"]))
	lines.append("")
	lines.append("STATUS: %s" % status)
	return "\n".join(lines) + "\n"


func _segment_probes_csv(segment_probes: Array[Dictionary]) -> String:
	var lines: Array[String] = [
		"segment_field,segment_value,status,group_count,train_groups,test_groups,native_top1,learned_top1,top1_delta,native_mean_selected_rank,learned_mean_selected_rank,mean_selected_rank_delta,native_mrr,learned_mrr,mrr_delta,skip_reason"
	]
	for probe in segment_probes:
		var metrics: Dictionary = probe.get("metrics", {})
		var native_row: Dictionary = metrics.get("native_total_score_baseline", {})
		var learned_row: Dictionary = metrics.get("learned_model", {})
		lines.append("%s,%s,%s,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%s" % [
			_csv_field(String(probe.get("segment_field", ""))),
			_csv_field(String(probe.get("segment_value", ""))),
			_csv_field(String(probe.get("status", ""))),
			int(probe.get("group_count", 0)),
			int(probe.get("train_groups", 0)),
			int(probe.get("test_groups", 0)),
			float(native_row.get("top1_selected_agreement", 0.0)),
			float(learned_row.get("top1_selected_agreement", 0.0)),
			float(metrics.get("top1_delta_vs_native", 0.0)),
			float(native_row.get("mean_selected_rank", 0.0)),
			float(learned_row.get("mean_selected_rank", 0.0)),
			float(metrics.get("mean_selected_rank_delta_vs_native", 0.0)),
			float(native_row.get("mrr", 0.0)),
			float(learned_row.get("mrr", 0.0)),
			float(metrics.get("mrr_delta_vs_native", 0.0)),
			_csv_field(String(probe.get("skip_reason", ""))),
		])
	return "\n".join(lines) + "\n"


func _run_self_test() -> Dictionary:
	var rows: Array[Dictionary] = []
	for group_index in range(40):
		var selected_index: int = group_index % 4
		for candidate_index in range(4):
			var score: float = 1.0 - absf(float(candidate_index - selected_index))
			var selected: bool = candidate_index == selected_index
			var blue_turn: bool = group_index % 2 == 0
			var winrate: float = 0.7 if selected else 0.45
			rows.append({
				"draft_seed": str(1000 + group_index / 20),
				"pairing_index": str(group_index / 20),
				"step_index": str(group_index % 20),
				"step_action": "PICK" if group_index % 3 != 0 else "BAN",
				"acting_side": "blue" if blue_turn else "red",
				"blue_picks_before": "a|b",
				"red_picks_before": "c|d",
				"blue_bans_before": "e",
				"red_bans_before": "f",
				"legal_pool": "aa|bb|cc|dd",
				"legal_pool_count": "4",
				"selected": "c%d" % selected_index,
				"candidate": "c%d" % candidate_index,
				"is_selected": "1" if selected else "0",
				"candidate_rank_native": str(candidate_index + 1),
				"blue_winrate": "%.6f" % (winrate if blue_turn else 1.0 - winrate),
				"red_winrate": "%.6f" % (1.0 - winrate if blue_turn else winrate),
				"base_power": "%.6f" % score,
				"ally_synergy": "%.6f" % (score * 0.5),
				"enemy_counter_value": "%.6f" % (score * 0.25),
				"counter_risk": "%.6f" % (-score * 0.1),
				"role_fit": "%.6f" % score,
				"comp_fit": "%.6f" % (score * 0.1),
				"total_score": "%.6f" % score,
				"phase_label": "early_pick",
				"candidate_role": "tank" if candidate_index % 2 == 0 else "carry",
			})
	var grouped: Dictionary = _group_rows(rows)
	var result: Dictionary = _run_experiment(rows, grouped, "self-test", "res://logs/native_draft_state_ranker_self_test", 0.8, 1337, 5, true)
	if not bool(result.get("ok", false)):
		return result
	var report_path: String = ProjectSettings.globalize_path("res://logs/native_draft_state_ranker_self_test/draft_state_ranker_report.md")
	if not FileAccess.file_exists(report_path):
		return {"ok": false, "error": "self-test report was not written"}
	return {"ok": true}


func _predict_logistic(weights: PackedFloat32Array, bias: float, features: PackedFloat32Array) -> float:
	var z: float = bias
	for i in range(weights.size()):
		z += weights[i] * features[i]
	if z >= 0.0:
		var e_neg: float = exp(-z)
		return 1.0 / (1.0 + e_neg)
	var e_pos: float = exp(z)
	return e_pos / (1.0 + e_pos)


func _harmonic(n: float) -> float:
	var total: float = 0.0
	for i in range(1, int(n) + 1):
		total += 1.0 / float(i)
	return total


func _mean_float(values: Array[float]) -> float:
	if values.is_empty():
		return 0.0
	var sum: float = 0.0
	for value in values:
		sum += value
	return sum / float(values.size())


func _mean_field(rows: Array, key: String) -> float:
	if rows.is_empty():
		return 0.0
	var sum: float = 0.0
	for row_var in rows:
		var row: Dictionary = row_var
		sum += float(row.get(key, 0.0))
	return sum / float(rows.size())


func _packed_to_array(values: PackedFloat32Array) -> Array[float]:
	var out: Array[float] = []
	for value in values:
		out.append(float(value))
	return out


func _csv_field(value: String) -> String:
	if value.contains("\"") or value.contains(",") or value.contains("\n") or value.contains("\r"):
		return "\"" + value.replace("\"", "\"\"") + "\""
	return value


func _write_text(path: String, text: String) -> bool:
	var global_path: String = ProjectSettings.globalize_path(path)
	var dir_path: String = global_path.get_base_dir()
	if not DirAccess.dir_exists_absolute(dir_path):
		var err: Error = DirAccess.make_dir_recursive_absolute(dir_path)
		if err != OK:
			return false
	var f := FileAccess.open(global_path, FileAccess.WRITE)
	if f == null:
		return false
	f.store_string(text)
	f.close()
	return true
