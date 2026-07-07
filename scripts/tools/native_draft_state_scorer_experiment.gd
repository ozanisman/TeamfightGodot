extends SceneTree

## Validation-only learned scorer experiment for draft-state decision rows.

const DraftValidationCsvScript := preload("res://scripts/tools/draft_validation_csv.gd")

const DEFAULT_INPUT_PATH: String = "res://model_stats/draft_state_training_rows.csv"
const DEFAULT_OUTPUT_DIR: String = "res://model_stats/draft_state_scorer_experiments/smoke"
const DEFAULT_TRAIN_FRACTION: float = 0.8
const DEFAULT_SPLIT_SEED: int = 1337
const DEFAULT_MIN_ROWS: int = 200
const DEFAULT_CALIBRATION_BINS: int = 10
const DEFAULT_SPLIT_REPEATS: int = 0
const REPEATED_SPLIT_SEED_STRIDE: int = 7919
const LEARNING_RATE: float = 0.05
const MAX_ITERATIONS: int = 5000
const CONVERGENCE_TOLERANCE: float = 0.000001
const L2_REGULARIZATION: float = 0.001
const GATE_ACCURACY_DELTA_PP: float = 2.0

const REQUIRED_COLUMNS: Array[String] = [
	"step_index",
	"step_action",
	"acting_side",
	"blue_picks_before",
	"red_picks_before",
	"blue_bans_before",
	"red_bans_before",
	"legal_pool",
	"selected",
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

const NUMERIC_FEATURE_COLUMNS: Array[String] = [
	"base_power",
	"ally_synergy",
	"enemy_counter_value",
	"counter_risk",
	"role_fit",
	"comp_fit",
	"total_score",
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
		var self_test_result: Dictionary = _run_self_test()
		if not bool(self_test_result.get("ok", false)):
			push_error("native_draft_state_scorer_experiment self-test failed: %s" % String(self_test_result.get("error", "")))
			quit(1)
			return
		print("native_draft_state_scorer_experiment: self-test PASS")
		quit(0)
		return

	var input_path: String = _extract_argument("--input=", DEFAULT_INPUT_PATH)
	var output_dir: String = _extract_argument("--output-dir=", DEFAULT_OUTPUT_DIR)
	var train_fraction: float = clampf(float(_extract_argument("--train-fraction=", str(DEFAULT_TRAIN_FRACTION))), 0.1, 0.95)
	var split_seed: int = int(_extract_argument("--seed=", str(DEFAULT_SPLIT_SEED)))
	var min_rows: int = maxi(1, int(_extract_argument("--min-rows=", str(DEFAULT_MIN_ROWS))))
	var calibration_bins: int = maxi(1, int(_extract_argument("--calibration-bins=", str(DEFAULT_CALIBRATION_BINS))))
	var split_repeats: int = maxi(0, int(_extract_argument("--split-repeats=", str(DEFAULT_SPLIT_REPEATS))))
	var require_pass: bool = _extract_argument("--require-pass=", "false") == "true"

	var rows: Array[Dictionary] = _load_rows(input_path)
	if rows.size() < min_rows:
		push_error("native_draft_state_scorer_experiment: input has %d rows, min-rows=%d" % [rows.size(), min_rows])
		quit(1)
		return

	var result: Dictionary = _run_experiment(rows, input_path, output_dir, train_fraction, split_seed, calibration_bins, split_repeats)
	if not bool(result.get("ok", false)):
		push_error("native_draft_state_scorer_experiment: %s" % String(result.get("error", "")))
		quit(1)
		return

	var status: String = String(result.get("status", "FAIL"))
	print("native_draft_state_scorer_experiment: %s" % status)
	print("native_draft_state_scorer_experiment: wrote %s" % output_dir)
	if require_pass and status != "PASS":
		push_error("SCRIPT ERROR: native_draft_state_scorer_experiment require-pass failed with status %s" % status)
	quit(1 if require_pass and status != "PASS" else 0)


func _load_rows(path: String) -> Array[Dictionary]:
	var rows: Array[Dictionary] = []
	var f := FileAccess.open(ProjectSettings.globalize_path(path), FileAccess.READ)
	if f == null:
		push_error("native_draft_state_scorer_experiment: could not open %s" % path)
		return rows

	var header: Array = DraftValidationCsvScript.split_csv_line(f.get_line())
	var idx: Dictionary = DraftValidationCsvScript.header_index(header)
	for column in REQUIRED_COLUMNS:
		if not idx.has(column):
			push_error("native_draft_state_scorer_experiment: missing required column %s" % column)
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
	for column in ["step_index", "blue_winrate", "red_winrate"]:
		var text: String = String(row.get(column, "")).strip_edges()
		if not text.is_valid_float():
			push_warning("native_draft_state_scorer_experiment: skipping row with invalid %s" % column)
			return false
	for column in NUMERIC_FEATURE_COLUMNS:
		var text: String = String(row.get(column, "")).strip_edges()
		if not text.is_valid_float():
			push_warning("native_draft_state_scorer_experiment: skipping row with invalid %s" % column)
			return false
	var acting_side: String = String(row.get("acting_side", "")).strip_edges()
	if acting_side != "blue" and acting_side != "red":
		push_warning("native_draft_state_scorer_experiment: skipping row with invalid acting_side")
		return false
	return true


func _run_experiment(
	rows: Array[Dictionary],
	input_path: String,
	output_dir: String,
	train_fraction: float,
	split_seed: int,
	calibration_bins: int,
	split_repeats: int
) -> Dictionary:
	var categories: Dictionary = _collect_categories(rows)
	var feature_names: Array[String] = _feature_names(categories)
	var feature_data: Array[Dictionary] = _extract_feature_data(rows, categories)
	if feature_data.size() < 2:
		return {"ok": false, "error": "not enough usable rows"}

	var split: Dictionary = _split_indices(feature_data.size(), train_fraction, split_seed)
	if Array(split["train"]).is_empty() or Array(split["test"]).is_empty():
		return {"ok": false, "error": "split produced empty train or test set"}

	var train_mean: float = _mean_target(feature_data, split["train"])
	var native_scaler: Dictionary = _fit_scaler(feature_data, split["train"], "native_features")
	var native_model: Dictionary = _train_logistic_regression(feature_data, split["train"], "native_features", native_scaler)
	var model_scaler: Dictionary = _fit_scaler(feature_data, split["train"], "features")
	var model: Dictionary = _train_logistic_regression(feature_data, split["train"], "features", model_scaler)

	var constant_eval: Dictionary = _evaluate_constant(feature_data, split, train_mean)
	var native_eval: Dictionary = _evaluate_model(native_model, feature_data, split, "native_features", native_scaler)
	var model_eval: Dictionary = _evaluate_model(model, feature_data, split, "features", model_scaler)
	var accuracy_delta_pp: float = (float(model_eval["test"]["accuracy"]) - float(native_eval["test"]["accuracy"])) * 100.0
	var mse_delta: float = float(model_eval["test"]["mse"]) - float(native_eval["test"]["mse"])
	var status: String = "PASS" if accuracy_delta_pp >= GATE_ACCURACY_DELTA_PP and mse_delta <= 0.0 else "VALIDATION_ONLY"
	var test_predictions: Array[Dictionary] = _prediction_rows(feature_data, split["test"], train_mean, native_model, native_scaler, model, model_scaler)
	var calibration: Dictionary = _calibration_report(test_predictions, calibration_bins)
	var grouped_metrics: Dictionary = _grouped_metrics(test_predictions)
	var repeated_splits: Array[Dictionary] = _run_repeated_splits(feature_data, train_fraction, split_seed, split_repeats)
	var repeated_summary: Dictionary = _summarize_repeated_splits(repeated_splits)

	var output_ok: bool = _write_outputs(
		output_dir, input_path, rows, feature_data, split, train_fraction, split_seed,
		feature_names, categories, constant_eval, native_eval, model_eval,
		native_model, native_scaler, model, model_scaler, train_mean,
		accuracy_delta_pp, mse_delta, status, calibration_bins,
		calibration, grouped_metrics, repeated_splits, repeated_summary
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


func _feature_names(categories: Dictionary) -> Array[String]:
	var names: Array[String] = []
	names.append_array(NUMERIC_FEATURE_COLUMNS)
	names.append_array([
		"step_index_norm",
		"is_pick",
		"is_blue",
		"blue_picks_before_count",
		"red_picks_before_count",
		"blue_bans_before_count",
		"red_bans_before_count",
		"legal_pool_count",
	])
	for key in ["candidate_role", "phase_label", "step_action", "acting_side"]:
		for value in Array(categories[key]):
			names.append("%s=%s" % [key, String(value)])
	return names


func _extract_feature_data(rows: Array[Dictionary], categories: Dictionary) -> Array[Dictionary]:
	var data: Array[Dictionary] = []
	for i in range(rows.size()):
		var row: Dictionary = rows[i]
		var step_index: int = int(row["step_index"])
		var target: float = float(row["blue_winrate"]) if String(row["acting_side"]) == "blue" else float(row["red_winrate"])
		var features: PackedFloat32Array = _features_for_row(row, categories)
		var native_features := PackedFloat32Array()
		native_features.append(float(row["total_score"]))
		data.append({
			"row_index": i,
			"step_index": step_index,
			"step_band": _step_band(step_index),
			"target": target,
			"features": features,
			"native_features": native_features,
			"selected": String(row.get("selected", "")),
			"acting_side": String(row.get("acting_side", "")),
			"step_action": String(row.get("step_action", "")),
			"total_score": float(row.get("total_score", "0.0")),
		})
	return data


func _step_band(step_index: int) -> String:
	var start: int = floori(float(clampi(step_index, 0, 19)) / 5.0) * 5
	return "%d-%d" % [start, start + 4]


func _features_for_row(row: Dictionary, categories: Dictionary) -> PackedFloat32Array:
	var features := PackedFloat32Array()
	for column in NUMERIC_FEATURE_COLUMNS:
		features.append(float(row[column]))
	var step_index: float = float(row["step_index"])
	features.append(step_index / 19.0)
	features.append(1.0 if String(row["step_action"]) == "PICK" else 0.0)
	features.append(1.0 if String(row["acting_side"]) == "blue" else 0.0)
	features.append(float(_split_list(String(row["blue_picks_before"])).size()))
	features.append(float(_split_list(String(row["red_picks_before"])).size()))
	features.append(float(_split_list(String(row["blue_bans_before"])).size()))
	features.append(float(_split_list(String(row["red_bans_before"])).size()))
	features.append(float(_split_list(String(row["legal_pool"])).size()))
	for key in ["candidate_role", "phase_label", "step_action", "acting_side"]:
		var value: String = String(row.get(key, "")).strip_edges()
		if value.is_empty():
			value = "unknown"
		for category in Array(categories[key]):
			features.append(1.0 if value == String(category) else 0.0)
	return features


func _split_list(raw: String) -> Array[String]:
	var out: Array[String] = []
	for part in raw.split("|", false):
		var trimmed: String = part.strip_edges()
		if not trimmed.is_empty():
			out.append(trimmed)
	return out


func _split_indices(size: int, train_fraction: float, split_seed: int) -> Dictionary:
	var indices: Array[int] = []
	for i in range(size):
		indices.append(i)
	var rng := RandomNumberGenerator.new()
	rng.seed = split_seed
	for i in range(indices.size() - 1, 0, -1):
		var j: int = rng.randi_range(0, i)
		var tmp: int = indices[i]
		indices[i] = indices[j]
		indices[j] = tmp
	var train_size: int = clampi(int(round(float(size) * train_fraction)), 1, size - 1)
	return {"train": indices.slice(0, train_size), "test": indices.slice(train_size), "all": indices}


func _fit_scaler(feature_data: Array[Dictionary], indices: Array, feature_key: String) -> Dictionary:
	var feature_count: int = PackedFloat32Array(feature_data[int(indices[0])][feature_key]).size()
	var means := PackedFloat32Array()
	var stddevs := PackedFloat32Array()
	means.resize(feature_count)
	stddevs.resize(feature_count)
	for idx_var in indices:
		var idx: int = int(idx_var)
		var features: PackedFloat32Array = feature_data[idx][feature_key]
		for i in range(feature_count):
			means[i] += features[i]
	for i in range(feature_count):
		means[i] /= float(indices.size())
	for idx_var in indices:
		var idx: int = int(idx_var)
		var features: PackedFloat32Array = feature_data[idx][feature_key]
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


func _train_logistic_regression(
	feature_data: Array[Dictionary],
	train_indices: Array,
	feature_key: String,
	scaler: Dictionary
) -> Dictionary:
	var num_features: int = PackedFloat32Array(feature_data[int(train_indices[0])][feature_key]).size()
	var weights := PackedFloat32Array()
	weights.resize(num_features)
	var bias: float = 0.0
	for _iteration in range(MAX_ITERATIONS):
		var grad_w := PackedFloat32Array()
		grad_w.resize(num_features)
		var grad_b: float = 0.0
		for idx_var in train_indices:
			var idx: int = int(idx_var)
			var item: Dictionary = feature_data[idx]
			var features: PackedFloat32Array = _standardized_features(item[feature_key], scaler)
			var target: float = float(item["target"])
			var pred: float = _predict_logistic(weights, bias, features)
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


func _evaluate_model(
	model: Dictionary,
	feature_data: Array[Dictionary],
	split: Dictionary,
	feature_key: String,
	scaler: Dictionary
) -> Dictionary:
	return {
		"train": _evaluate_model_indices(model, feature_data, split["train"], feature_key, scaler),
		"test": _evaluate_model_indices(model, feature_data, split["test"], feature_key, scaler),
		"all": _evaluate_model_indices(model, feature_data, split["all"], feature_key, scaler),
	}


func _evaluate_model_indices(
	model: Dictionary,
	feature_data: Array[Dictionary],
	indices: Array,
	feature_key: String,
	scaler: Dictionary
) -> Dictionary:
	var weights: PackedFloat32Array = model["weights"]
	var bias: float = float(model["bias"])
	var preds: Array[float] = []
	var correct: int = 0
	var mse: float = 0.0
	for idx_var in indices:
		var idx: int = int(idx_var)
		var item: Dictionary = feature_data[idx]
		var pred: float = _predict_logistic(weights, bias, _standardized_features(item[feature_key], scaler))
		var target: float = float(item["target"])
		preds.append(pred)
		if (pred > 0.5) == (target > 0.5):
			correct += 1
		mse += (pred - target) * (pred - target)
	return _metric_summary(indices.size(), correct, mse, preds)


func _evaluate_constant(feature_data: Array[Dictionary], split: Dictionary, train_mean: float) -> Dictionary:
	return {
		"train": _evaluate_constant_indices(feature_data, split["train"], train_mean),
		"test": _evaluate_constant_indices(feature_data, split["test"], train_mean),
		"all": _evaluate_constant_indices(feature_data, split["all"], train_mean),
	}


func _evaluate_constant_indices(feature_data: Array[Dictionary], indices: Array, pred: float) -> Dictionary:
	var preds: Array[float] = []
	var correct: int = 0
	var mse: float = 0.0
	for idx_var in indices:
		var idx: int = int(idx_var)
		var target: float = float(feature_data[idx]["target"])
		preds.append(pred)
		if (pred > 0.5) == (target > 0.5):
			correct += 1
		mse += (pred - target) * (pred - target)
	return _metric_summary(indices.size(), correct, mse, preds)


func _prediction_rows(
	feature_data: Array[Dictionary],
	indices: Array,
	constant_pred: float,
	native_model: Dictionary,
	native_scaler: Dictionary,
	model: Dictionary,
	model_scaler: Dictionary
) -> Array[Dictionary]:
	var rows: Array[Dictionary] = []
	for idx_var in indices:
		var idx: int = int(idx_var)
		var item: Dictionary = feature_data[idx]
		var native_pred: float = _predict_logistic(native_model["weights"], float(native_model["bias"]), _standardized_features(item["native_features"], native_scaler))
		var learned_pred: float = _predict_logistic(model["weights"], float(model["bias"]), _standardized_features(item["features"], model_scaler))
		rows.append({
			"row_index": int(item["row_index"]),
			"target": float(item["target"]),
			"constant_pred": constant_pred,
			"native_pred": native_pred,
			"learned_pred": learned_pred,
			"acting_side": String(item["acting_side"]),
			"step_action": String(item["step_action"]),
			"step_band": String(item["step_band"]),
		})
	return rows


func _calibration_report(prediction_rows: Array[Dictionary], bins: int) -> Dictionary:
	return {
		"constant_baseline": _calibration_for_model(prediction_rows, "constant_pred", bins),
		"native_total_score_baseline": _calibration_for_model(prediction_rows, "native_pred", bins),
		"learned_model": _calibration_for_model(prediction_rows, "learned_pred", bins),
	}


func _calibration_for_model(prediction_rows: Array[Dictionary], pred_key: String, bins: int) -> Dictionary:
	var bucket_rows: Array[Array] = []
	for _i in range(bins):
		bucket_rows.append([])
	for row in prediction_rows:
		var pred: float = clampf(float(row[pred_key]), 0.0, 1.0)
		var bin_index: int = clampi(int(floor(pred * float(bins))), 0, bins - 1)
		bucket_rows[bin_index].append(row)

	var out_bins: Array[Dictionary] = []
	var weighted_abs_error: float = 0.0
	var total_rows: int = 0
	for bin_index in range(bins):
		var rows: Array = bucket_rows[bin_index]
		var metrics: Dictionary = _metrics_for_prediction_key(rows, pred_key)
		var mean_pred: float = float(metrics["pred_mean"])
		var mean_target: float = _mean_target_for_rows(rows)
		var abs_calibration_error: float = absf(mean_pred - mean_target)
		var count: int = rows.size()
		weighted_abs_error += abs_calibration_error * float(count)
		total_rows += count
		out_bins.append({
			"bin": bin_index,
			"pred_min": float(bin_index) / float(bins),
			"pred_max": float(bin_index + 1) / float(bins),
			"n": count,
			"mean_prediction": mean_pred,
			"mean_target": mean_target,
			"accuracy": metrics["accuracy"],
			"mse": metrics["mse"],
			"mean_abs_calibration_error": abs_calibration_error,
		})
	return {
		"summary": {
			"n": total_rows,
			"mse": _metrics_for_prediction_key(prediction_rows, pred_key)["mse"],
			"mean_abs_calibration_error": weighted_abs_error / float(total_rows) if total_rows > 0 else 0.0,
		},
		"bins": out_bins,
	}


func _grouped_metrics(prediction_rows: Array[Dictionary]) -> Dictionary:
	return {
		"acting_side": _group_metrics_by_field(prediction_rows, "acting_side"),
		"step_action": _group_metrics_by_field(prediction_rows, "step_action"),
		"step_band": _group_metrics_by_field(prediction_rows, "step_band"),
	}


func _group_metrics_by_field(prediction_rows: Array[Dictionary], field_name: String) -> Dictionary:
	var grouped_rows: Dictionary = {}
	for row in prediction_rows:
		var key: String = String(row.get(field_name, "unknown"))
		if not grouped_rows.has(key):
			grouped_rows[key] = []
		grouped_rows[key].append(row)
	var out: Dictionary = {}
	for key in grouped_rows.keys():
		var rows: Array = grouped_rows[key]
		out[key] = {
			"constant_baseline": _metrics_for_prediction_key(rows, "constant_pred"),
			"native_total_score_baseline": _metrics_for_prediction_key(rows, "native_pred"),
			"learned_model": _metrics_for_prediction_key(rows, "learned_pred"),
		}
	return out


func _metrics_for_prediction_key(rows: Array, pred_key: String) -> Dictionary:
	var preds: Array[float] = []
	var correct: int = 0
	var mse: float = 0.0
	for row_var in rows:
		var row: Dictionary = row_var
		var pred: float = float(row.get(pred_key, 0.5))
		var target: float = float(row.get("target", 0.5))
		preds.append(pred)
		if (pred > 0.5) == (target > 0.5):
			correct += 1
		mse += (pred - target) * (pred - target)
	return _metric_summary(rows.size(), correct, mse, preds)


func _mean_target_for_rows(rows: Array) -> float:
	var values: Array[float] = []
	for row_var in rows:
		var row: Dictionary = row_var
		values.append(float(row.get("target", 0.5)))
	return _mean_float(values)


func _run_repeated_splits(
	feature_data: Array[Dictionary],
	train_fraction: float,
	split_seed: int,
	split_repeats: int
) -> Array[Dictionary]:
	var reports: Array[Dictionary] = []
	for repeat_index in range(1, split_repeats + 1):
		var repeat_seed: int = split_seed + repeat_index * REPEATED_SPLIT_SEED_STRIDE
		var split: Dictionary = _split_indices(feature_data.size(), train_fraction, repeat_seed)
		var train_mean: float = _mean_target(feature_data, split["train"])
		var native_scaler: Dictionary = _fit_scaler(feature_data, split["train"], "native_features")
		var native_model: Dictionary = _train_logistic_regression(feature_data, split["train"], "native_features", native_scaler)
		var model_scaler: Dictionary = _fit_scaler(feature_data, split["train"], "features")
		var model: Dictionary = _train_logistic_regression(feature_data, split["train"], "features", model_scaler)
		var constant_eval: Dictionary = _evaluate_constant(feature_data, split, train_mean)
		var native_eval: Dictionary = _evaluate_model(native_model, feature_data, split, "native_features", native_scaler)
		var model_eval: Dictionary = _evaluate_model(model, feature_data, split, "features", model_scaler)
		var accuracy_delta_pp: float = (float(model_eval["test"]["accuracy"]) - float(native_eval["test"]["accuracy"])) * 100.0
		var mse_delta: float = float(model_eval["test"]["mse"]) - float(native_eval["test"]["mse"])
		reports.append({
			"repeat": repeat_index,
			"seed": repeat_seed,
			"constant_test_accuracy": constant_eval["test"]["accuracy"],
			"constant_test_mse": constant_eval["test"]["mse"],
			"native_test_accuracy": native_eval["test"]["accuracy"],
			"native_test_mse": native_eval["test"]["mse"],
			"learned_test_accuracy": model_eval["test"]["accuracy"],
			"learned_test_mse": model_eval["test"]["mse"],
			"accuracy_delta_pp": accuracy_delta_pp,
			"mse_delta": mse_delta,
			"status": "PASS" if accuracy_delta_pp >= GATE_ACCURACY_DELTA_PP and mse_delta <= 0.0 else "VALIDATION_ONLY",
		})
	return reports


func _summarize_repeated_splits(reports: Array[Dictionary]) -> Dictionary:
	var accuracy_deltas: Array[float] = []
	var mse_deltas: Array[float] = []
	var pass_count: int = 0
	for report in reports:
		accuracy_deltas.append(float(report["accuracy_delta_pp"]))
		mse_deltas.append(float(report["mse_delta"]))
		if String(report["status"]) == "PASS":
			pass_count += 1
	return {
		"n": reports.size(),
		"pass_count": pass_count,
		"accuracy_delta_pp": _summary_stats(accuracy_deltas),
		"mse_delta": _summary_stats(mse_deltas),
	}


func _metric_summary(n: int, correct: int, mse_sum: float, preds: Array[float]) -> Dictionary:
	return {
		"n": n,
		"accuracy": float(correct) / float(n) if n > 0 else 0.0,
		"mse": mse_sum / float(n) if n > 0 else 0.0,
		"pred_min": _min_float(preds),
		"pred_max": _max_float(preds),
		"pred_mean": _mean_float(preds),
	}


func _mean_target(feature_data: Array[Dictionary], indices: Array) -> float:
	var values: Array[float] = []
	for idx_var in indices:
		values.append(float(feature_data[int(idx_var)]["target"]))
	return _mean_float(values)


func _predict_logistic(weights: PackedFloat32Array, bias: float, features: PackedFloat32Array) -> float:
	var z: float = bias
	for i in range(weights.size()):
		z += weights[i] * features[i]
	return 1.0 / (1.0 + exp(-z))


func _write_outputs(
	output_dir: String,
	input_path: String,
	rows: Array[Dictionary],
	feature_data: Array[Dictionary],
	split: Dictionary,
	train_fraction: float,
	split_seed: int,
	feature_names: Array[String],
	categories: Dictionary,
	constant_eval: Dictionary,
	native_eval: Dictionary,
	model_eval: Dictionary,
	native_model: Dictionary,
	native_scaler: Dictionary,
	model: Dictionary,
	model_scaler: Dictionary,
	train_mean: float,
	accuracy_delta_pp: float,
	mse_delta: float,
	status: String,
	calibration_bins: int,
	calibration: Dictionary,
	grouped_metrics: Dictionary,
	repeated_splits: Array[Dictionary],
	repeated_summary: Dictionary
) -> bool:
	var global_output_dir: String = ProjectSettings.globalize_path(output_dir)
	if not DirAccess.dir_exists_absolute(global_output_dir):
		var err: Error = DirAccess.make_dir_recursive_absolute(global_output_dir)
		if err != OK:
			push_error("native_draft_state_scorer_experiment: could not create %s" % output_dir)
			return false

	var metrics: Dictionary = {
		"status": status,
		"input_path": input_path,
		"row_count": rows.size(),
		"train_fraction": train_fraction,
		"split_seed": split_seed,
		"calibration_bins": calibration_bins,
		"gate_accuracy_delta_pp": GATE_ACCURACY_DELTA_PP,
		"accuracy_delta_pp": accuracy_delta_pp,
		"mse_delta": mse_delta,
		"constant_baseline": constant_eval,
		"native_total_score_baseline": native_eval,
		"learned_model": model_eval,
		"calibration": calibration,
		"grouped_holdout_metrics": grouped_metrics,
		"repeated_split_summary": repeated_summary,
	}
	var model_json: Dictionary = {
		"model_type": "standardized_logistic_regression",
		"validation_only": true,
		"target": "acting_side_winrate",
		"feature_names": feature_names,
		"categories": categories,
		"constant_train_mean": train_mean,
		"native_total_score_baseline": _model_to_json(native_model, native_scaler, ["total_score"]),
		"learned_model": _model_to_json(model, model_scaler, feature_names),
	}

	if not _write_text(output_dir.path_join("draft_state_scorer_metrics.json"), JSON.stringify(metrics, "\t") + "\n"):
		return false
	if not _write_text(output_dir.path_join("draft_state_scorer_model.json"), JSON.stringify(model_json, "\t") + "\n"):
		return false
	if not _write_text(output_dir.path_join("draft_state_scorer_predictions.csv"), _predictions_csv(feature_data, split, train_mean, native_model, native_scaler, model, model_scaler)):
		return false
	if not _write_text(output_dir.path_join("draft_state_scorer_calibration.csv"), _calibration_csv(calibration)):
		return false
	if not _write_text(output_dir.path_join("draft_state_scorer_repeated_splits.csv"), _repeated_splits_csv(repeated_splits)):
		return false
	if not _write_text(output_dir.path_join("draft_state_scorer_report.md"), _report_markdown(input_path, rows.size(), train_fraction, split_seed, calibration_bins, constant_eval, native_eval, model_eval, accuracy_delta_pp, mse_delta, status, calibration, grouped_metrics, repeated_summary)):
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


func _predictions_csv(
	feature_data: Array[Dictionary],
	split: Dictionary,
	constant_pred: float,
	native_model: Dictionary,
	native_scaler: Dictionary,
	model: Dictionary,
	model_scaler: Dictionary
) -> String:
	var split_by_index: Dictionary = {}
	for idx_var in Array(split["train"]):
		split_by_index[int(idx_var)] = "train"
	for idx_var in Array(split["test"]):
		split_by_index[int(idx_var)] = "test"
	var lines: Array[String] = [
		"row_index,split,target,constant_pred,native_pred,learned_pred,total_score,selected,acting_side,step_action"
	]
	for i in range(feature_data.size()):
		var item: Dictionary = feature_data[i]
		var native_pred: float = _predict_logistic(native_model["weights"], float(native_model["bias"]), _standardized_features(item["native_features"], native_scaler))
		var learned_pred: float = _predict_logistic(model["weights"], float(model["bias"]), _standardized_features(item["features"], model_scaler))
		lines.append("%d,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%s,%s,%s" % [
			int(item["row_index"]),
			String(split_by_index.get(i, "unknown")),
			float(item["target"]),
			constant_pred,
			native_pred,
			learned_pred,
			float(item["total_score"]),
			_csv_field(String(item["selected"])),
			_csv_field(String(item["acting_side"])),
			_csv_field(String(item["step_action"])),
		])
	return "\n".join(lines) + "\n"


func _calibration_csv(calibration: Dictionary) -> String:
	var lines: Array[String] = [
		"model,bin,pred_min,pred_max,n,mean_prediction,mean_target,accuracy,mse,mean_abs_calibration_error"
	]
	for model_key in ["constant_baseline", "native_total_score_baseline", "learned_model"]:
		var model_calibration: Dictionary = calibration.get(model_key, {})
		for bin_var in Array(model_calibration.get("bins", [])):
			var bin_row: Dictionary = bin_var
			lines.append("%s,%d,%.6f,%.6f,%d,%.6f,%.6f,%.6f,%.6f,%.6f" % [
				model_key,
				int(bin_row.get("bin", 0)),
				float(bin_row.get("pred_min", 0.0)),
				float(bin_row.get("pred_max", 0.0)),
				int(bin_row.get("n", 0)),
				float(bin_row.get("mean_prediction", 0.0)),
				float(bin_row.get("mean_target", 0.0)),
				float(bin_row.get("accuracy", 0.0)),
				float(bin_row.get("mse", 0.0)),
				float(bin_row.get("mean_abs_calibration_error", 0.0)),
			])
	return "\n".join(lines) + "\n"


func _repeated_splits_csv(reports: Array[Dictionary]) -> String:
	var lines: Array[String] = [
		"repeat,seed,constant_test_accuracy,constant_test_mse,native_test_accuracy,native_test_mse,learned_test_accuracy,learned_test_mse,accuracy_delta_pp,mse_delta,status"
	]
	for report in reports:
		lines.append("%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%+.6f,%+.6f,%s" % [
			int(report.get("repeat", 0)),
			int(report.get("seed", 0)),
			float(report.get("constant_test_accuracy", 0.0)),
			float(report.get("constant_test_mse", 0.0)),
			float(report.get("native_test_accuracy", 0.0)),
			float(report.get("native_test_mse", 0.0)),
			float(report.get("learned_test_accuracy", 0.0)),
			float(report.get("learned_test_mse", 0.0)),
			float(report.get("accuracy_delta_pp", 0.0)),
			float(report.get("mse_delta", 0.0)),
			String(report.get("status", "")),
		])
	return "\n".join(lines) + "\n"


func _report_markdown(
	input_path: String,
	row_count: int,
	train_fraction: float,
	split_seed: int,
	calibration_bins: int,
	constant_eval: Dictionary,
	native_eval: Dictionary,
	model_eval: Dictionary,
	accuracy_delta_pp: float,
	mse_delta: float,
	status: String,
	calibration: Dictionary,
	grouped_metrics: Dictionary,
	repeated_summary: Dictionary
) -> String:
	var lines: Array[String] = []
	lines.append("# Draft-State Scorer Experiment")
	lines.append("")
	lines.append("Input: `%s`" % input_path)
	lines.append("Rows: %d" % row_count)
	lines.append("Train fraction: %.3f" % train_fraction)
	lines.append("Split seed: %d" % split_seed)
	lines.append("Calibration bins: %d" % calibration_bins)
	lines.append("")
	lines.append("## Metrics")
	lines.append("- constant baseline: %s" % _format_eval(constant_eval))
	lines.append("- native total_score baseline: %s" % _format_eval(native_eval))
	lines.append("- learned scorer: %s" % _format_eval(model_eval))
	lines.append("")
	lines.append("## Calibration")
	lines.append("- constant baseline: %s" % _format_calibration(calibration.get("constant_baseline", {})))
	lines.append("- native total_score baseline: %s" % _format_calibration(calibration.get("native_total_score_baseline", {})))
	lines.append("- learned scorer: %s" % _format_calibration(calibration.get("learned_model", {})))
	lines.append("")
	lines.append("## Held-Out Groups")
	for group_name in ["acting_side", "step_action", "step_band"]:
		lines.append("- %s:" % group_name)
		var group: Dictionary = grouped_metrics.get(group_name, {})
		var keys: Array = _ordered_group_keys(group, group_name)
		for key in keys:
			var row: Dictionary = group[key]
			lines.append("  - %s: native %s; learned %s" % [
				String(key),
				_format_short_eval(row["native_total_score_baseline"]),
				_format_short_eval(row["learned_model"]),
			])
	if int(repeated_summary.get("n", 0)) > 0:
		lines.append("")
		lines.append("## Repeated Splits")
		lines.append("- repeats: %d, pass_count: %d" % [
			int(repeated_summary.get("n", 0)),
			int(repeated_summary.get("pass_count", 0)),
		])
		lines.append("- accuracy delta pp: %s" % _format_summary(repeated_summary["accuracy_delta_pp"]))
		lines.append("- MSE delta: %s" % _format_summary(repeated_summary["mse_delta"]))
	lines.append("")
	lines.append("## Gate")
	lines.append("- learned vs native accuracy delta: %+.2f pp" % accuracy_delta_pp)
	lines.append("- learned vs native MSE delta: %+.6f" % mse_delta)
	lines.append("- required: accuracy delta >= %.2f pp and MSE delta <= 0" % GATE_ACCURACY_DELTA_PP)
	lines.append("")
	lines.append("No runtime scorer wiring is performed by this validation-only experiment.")
	lines.append("")
	lines.append("STATUS: %s" % status)
	return "\n".join(lines) + "\n"


func _format_calibration(model_calibration: Dictionary) -> String:
	var summary: Dictionary = model_calibration.get("summary", {})
	return "n=%d mse=%.6f mean_abs_calibration_error=%.6f" % [
		int(summary.get("n", 0)),
		float(summary.get("mse", 0.0)),
		float(summary.get("mean_abs_calibration_error", 0.0)),
	]


func _ordered_group_keys(group: Dictionary, group_name: String) -> Array:
	if group_name == "step_band":
		var ordered: Array[String] = []
		for key in ["0-4", "5-9", "10-14", "15-19"]:
			if group.has(key):
				ordered.append(key)
		return ordered
	var keys: Array = group.keys()
	keys.sort()
	return keys


func _format_eval(evals: Dictionary) -> String:
	return "train=%.2f%% test=%.2f%% all=%.2f%% test_mse=%.6f pred_test=%.4f..%.4f mean=%.4f" % [
		float(evals["train"]["accuracy"]) * 100.0,
		float(evals["test"]["accuracy"]) * 100.0,
		float(evals["all"]["accuracy"]) * 100.0,
		float(evals["test"]["mse"]),
		float(evals["test"]["pred_min"]),
		float(evals["test"]["pred_max"]),
		float(evals["test"]["pred_mean"]),
	]


func _format_short_eval(evals: Dictionary) -> String:
	return "n=%d acc=%.2f%% mse=%.6f" % [
		int(evals.get("n", 0)),
		float(evals.get("accuracy", 0.0)) * 100.0,
		float(evals.get("mse", 0.0)),
	]


func _format_summary(summary: Dictionary) -> String:
	return "mean=%+.6f min=%+.6f max=%+.6f" % [
		float(summary.get("mean", 0.0)),
		float(summary.get("min", 0.0)),
		float(summary.get("max", 0.0)),
	]


func _write_text(path: String, text: String) -> bool:
	var global_path: String = ProjectSettings.globalize_path(path)
	var dir_path: String = global_path.get_base_dir()
	if not DirAccess.dir_exists_absolute(dir_path):
		var err: Error = DirAccess.make_dir_recursive_absolute(dir_path)
		if err != OK:
			push_error("native_draft_state_scorer_experiment: could not create %s" % dir_path)
			return false
	var f := FileAccess.open(global_path, FileAccess.WRITE)
	if f == null:
		push_error("native_draft_state_scorer_experiment: could not write %s" % path)
		return false
	f.store_string(text)
	f.close()
	return true


func _run_self_test() -> Dictionary:
	var rows: Array[Dictionary] = []
	for i in range(240):
		var blue_turn: bool = (i % 2) == 0
		var pick_turn: bool = (i % 3) != 0
		var score_signal: float = -1.0 + float(i % 40) / 20.0
		var blue_winrate: float = clampf(0.5 + score_signal * 0.18 + (0.08 if blue_turn else -0.08), 0.05, 0.95)
		var red_winrate: float = 1.0 - blue_winrate
		rows.append({
			"step_index": str(i % 20),
			"step_action": "PICK" if pick_turn else "BAN",
			"acting_side": "blue" if blue_turn else "red",
			"blue_picks_before": "a|b" if i % 5 > 1 else "a",
			"red_picks_before": "c|d" if i % 7 > 2 else "c",
			"blue_bans_before": "e|f" if i % 4 > 1 else "e",
			"red_bans_before": "g|h" if i % 6 > 2 else "g",
			"legal_pool": "aa|bb|cc|dd|ee",
			"selected": "aa",
			"blue_winrate": "%.6f" % blue_winrate,
			"red_winrate": "%.6f" % red_winrate,
			"base_power": "%.6f" % score_signal,
			"ally_synergy": "%.6f" % (score_signal * 0.5),
			"enemy_counter_value": "%.6f" % (score_signal * 0.25),
			"counter_risk": "%.6f" % (-score_signal * 0.2),
			"role_fit": "%.6f" % (0.1 if pick_turn else -0.1),
			"comp_fit": "%.6f" % (score_signal * 0.1),
			"total_score": "%.6f" % score_signal,
			"phase_label": "early" if i % 20 < 8 else "late",
			"candidate_role": "tank" if i % 2 == 0 else "carry",
		})
	var result: Dictionary = _run_experiment(
		rows,
		"self-test",
		"res://logs/native_draft_state_scorer_self_test",
		0.8,
		1337,
		10,
		2
	)
	if not bool(result.get("ok", false)):
		return result
	var report_path: String = ProjectSettings.globalize_path("res://logs/native_draft_state_scorer_self_test/draft_state_scorer_report.md")
	if not FileAccess.file_exists(report_path):
		return {"ok": false, "error": "self-test report was not written"}
	return {"ok": true}


func _packed_to_array(values: PackedFloat32Array) -> Array[float]:
	var out: Array[float] = []
	for value in values:
		out.append(float(value))
	return out


func _mean_float(values: Array[float]) -> float:
	if values.is_empty():
		return 0.0
	var sum: float = 0.0
	for value in values:
		sum += value
	return sum / float(values.size())


func _summary_stats(values: Array[float]) -> Dictionary:
	if values.is_empty():
		return {"mean": 0.0, "min": 0.0, "max": 0.0}
	return {
		"mean": _mean_float(values),
		"min": _min_float(values),
		"max": _max_float(values),
	}


func _min_float(values: Array[float]) -> float:
	if values.is_empty():
		return 0.0
	var out: float = values[0]
	for value in values:
		out = minf(out, value)
	return out


func _max_float(values: Array[float]) -> float:
	if values.is_empty():
		return 0.0
	var out: float = values[0]
	for value in values:
		out = maxf(out, value)
	return out


func _csv_field(value: String) -> String:
	if value.find(",") < 0 and value.find("\"") < 0 and value.find("\n") < 0:
		return value
	return "\"" + value.replace("\"", "\"\"") + "\""
