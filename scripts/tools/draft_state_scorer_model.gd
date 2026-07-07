class_name DraftStateScorerModel
extends RefCounted

## Loader and feature builder for validation-only draft-state scorer artifacts.

const DEFAULT_MODEL_PATH: String = "res://model_stats/draft_state_scorer_experiments/validation_native_softmax_200x3/draft_state_scorer_model.json"
const MAX_DRAFT_STEP_INDEX: float = 19.0
const DEFAULT_CATEGORY_VALUE: String = "unknown"

const NUMERIC_FEATURE_COLUMNS: Array[String] = [
	"base_power",
	"ally_synergy",
	"enemy_counter_value",
	"counter_risk",
	"role_fit",
	"comp_fit",
	"total_score",
]

const CATEGORICAL_FEATURE_KEYS: Array[String] = [
	"candidate_role",
	"phase_label",
	"step_action",
	"acting_side",
]

var _loaded: bool = false
var _error: String = ""
var _categories: Dictionary = {}
var _feature_names: Array[String] = []
var _weights := PackedFloat32Array()
var _means := PackedFloat32Array()
var _stddevs := PackedFloat32Array()
var _bias: float = 0.0


func load_from_path(model_path: String = DEFAULT_MODEL_PATH) -> bool:
	_loaded = false
	_error = ""
	_categories = {}
	_feature_names = []
	_weights = PackedFloat32Array()
	_means = PackedFloat32Array()
	_stddevs = PackedFloat32Array()
	_bias = 0.0

	var global_path: String = ProjectSettings.globalize_path(model_path)
	if not FileAccess.file_exists(global_path):
		_error = "model file does not exist: %s" % model_path
		return false

	var f := FileAccess.open(global_path, FileAccess.READ)
	if f == null:
		_error = "could not open model file: %s" % model_path
		return false
	var parsed: Variant = JSON.parse_string(f.get_as_text())
	f.close()
	if typeof(parsed) != TYPE_DICTIONARY:
		_error = "model file is not a JSON object: %s" % model_path
		return false

	var model_json: Dictionary = parsed
	if not model_json.has("learned_model"):
		_error = "model file missing learned_model"
		return false
	if not model_json.has("categories"):
		_error = "model file missing categories"
		return false

	var learned_model: Dictionary = model_json["learned_model"]
	_categories = _normalize_categories(model_json["categories"])
	_feature_names = _string_array(learned_model.get("feature_names", model_json.get("feature_names", [])))
	_weights = _float_array(learned_model.get("weights", []))
	_means = _float_array(learned_model.get("means", []))
	_stddevs = _float_array(learned_model.get("stddevs", []))
	_bias = float(learned_model.get("bias", 0.0))

	var expected_feature_names: Array[String] = feature_names_for_categories(_categories)
	if _feature_names != expected_feature_names:
		_error = "model feature_names do not match scorer feature builder"
		return false
	if _weights.size() != _feature_names.size() or _means.size() != _feature_names.size() or _stddevs.size() != _feature_names.size():
		_error = "model vector sizes do not match feature_names"
		return false

	_loaded = true
	return true


func is_loaded() -> bool:
	return _loaded


func last_error() -> String:
	return _error


func score_row(row: Dictionary) -> float:
	if not _loaded:
		return 0.5
	var features: PackedFloat32Array = features_for_row(row, _categories)
	var z: float = _bias
	for i in range(features.size()):
		var stddev: float = float(_stddevs[i])
		var standardized: float = 0.0
		if absf(stddev) > 0.000001:
			standardized = (float(features[i]) - float(_means[i])) / stddev
		z += standardized * float(_weights[i])
	return _sigmoid(z)


func row_from_recommendation(
	recommendation: Dictionary,
	action: String,
	acting_side: String,
	step_index: int,
	blue_picks_before: Array,
	red_picks_before: Array,
	blue_bans_before: Array,
	red_bans_before: Array,
	legal_pool: Array
) -> Dictionary:
	var row: Dictionary = {
		"step_index": str(step_index),
		"step_action": action,
		"acting_side": acting_side,
		"blue_picks_before": _join_values(blue_picks_before),
		"red_picks_before": _join_values(red_picks_before),
		"blue_bans_before": _join_values(blue_bans_before),
		"red_bans_before": _join_values(red_bans_before),
		"legal_pool": _join_values(legal_pool),
		"selected": String(recommendation.get("candidate", "")),
		"total_score": str(float(recommendation.get("total_score", 0.0))),
		"phase_label": String(recommendation.get("phase_label", "")),
		"candidate_role": String(recommendation.get("candidate_role", "")),
	}
	if action == "PICK":
		row["base_power"] = str(float(recommendation.get("base_power", 0.0)))
		row["ally_synergy"] = str(float(recommendation.get("ally_synergy", 0.0)))
		row["enemy_counter_value"] = str(float(recommendation.get("enemy_counter_value", 0.0)))
		row["counter_risk"] = str(float(recommendation.get("counter_risk", 0.0)))
		row["role_fit"] = str(float(recommendation.get("role_fit", 0.0)))
		row["comp_fit"] = str(float(recommendation.get("comp_fit", 0.0)))
	else:
		row["base_power"] = str(float(recommendation.get("enemy_pick_value", 0.0)))
		row["ally_synergy"] = str(float(recommendation.get("enemy_synergy", 0.0)))
		row["enemy_counter_value"] = str(float(recommendation.get("counters_my_team", 0.0)))
		row["counter_risk"] = "0.0"
		row["role_fit"] = str(float(recommendation.get("fills_enemy_role_need", 0.0)))
		row["comp_fit"] = str(float(recommendation.get("enemy_comp_fit", 0.0)))
	return row


static func feature_names_for_categories(categories: Dictionary) -> Array[String]:
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
	for key in CATEGORICAL_FEATURE_KEYS:
		for value in Array(categories.get(key, [])):
			names.append("%s=%s" % [key, String(value)])
	return names


static func features_for_row(row: Dictionary, categories: Dictionary) -> PackedFloat32Array:
	var features := PackedFloat32Array()
	for column in NUMERIC_FEATURE_COLUMNS:
		features.append(float(row.get(column, 0.0)))
	var step_index: float = float(row.get("step_index", 0.0))
	features.append(step_index / MAX_DRAFT_STEP_INDEX)
	features.append(1.0 if String(row.get("step_action", "")) == "PICK" else 0.0)
	features.append(1.0 if String(row.get("acting_side", "")) == "blue" else 0.0)
	features.append(float(_list_count(row.get("blue_picks_before", ""))))
	features.append(float(_list_count(row.get("red_picks_before", ""))))
	features.append(float(_list_count(row.get("blue_bans_before", ""))))
	features.append(float(_list_count(row.get("red_bans_before", ""))))
	features.append(float(_list_count(row.get("legal_pool", ""))))
	for key in CATEGORICAL_FEATURE_KEYS:
		var value: String = String(row.get(key, "")).strip_edges()
		if value.is_empty():
			value = DEFAULT_CATEGORY_VALUE
		for category in Array(categories.get(key, [])):
			features.append(1.0 if value == String(category) else 0.0)
	return features


static func _list_count(raw: Variant) -> int:
	if typeof(raw) == TYPE_ARRAY:
		return Array(raw).size()
	var count: int = 0
	for part in String(raw).split("|", false):
		if not part.strip_edges().is_empty():
			count += 1
	return count


static func _join_values(values: Array) -> String:
	var out: Array[String] = []
	for value in values:
		out.append(String(value))
	return "|".join(out)


func _normalize_categories(raw: Variant) -> Dictionary:
	var out: Dictionary = {}
	var raw_dict: Dictionary = raw if typeof(raw) == TYPE_DICTIONARY else {}
	for key in CATEGORICAL_FEATURE_KEYS:
		var values: Array[String] = _string_array(raw_dict.get(key, []))
		values.sort()
		out[key] = values
	return out


func _string_array(raw: Variant) -> Array[String]:
	var out: Array[String] = []
	if typeof(raw) != TYPE_ARRAY:
		return out
	for value in Array(raw):
		out.append(String(value))
	return out


func _float_array(raw: Variant) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	if typeof(raw) != TYPE_ARRAY:
		return out
	for value in Array(raw):
		out.append(float(value))
	return out


func _sigmoid(z: float) -> float:
	if z >= 0.0:
		var e_neg: float = exp(-z)
		return 1.0 / (1.0 + e_neg)
	var e_pos: float = exp(z)
	return e_pos / (1.0 + e_pos)
