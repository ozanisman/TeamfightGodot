extends RefCounted
## Centralized GDScript-side draft AI tunables (Workstream 0.2). Mirrors the native
## sim::draft_ai::Config softmax defaults so gameplay (simulation_viewer_base.gd) and the
## validation harness (draft_strategy_native_softmax.gd) stay in sync. Reads the same optional
## JSON override file as the native side; falls back to hardcoded defaults when absent.

## Optional JSON override file shared with the native sim::draft_ai::Config loader
## (sim_draft_ai_config.cpp). Absent by default; when present, both native scoring
## weights and this GDScript softmax config read from the same file.
const DEFAULT_CONFIG_PATH: String = "res://model_stats/draft_ai_config.json"
const LEGACY_LOOKAHEAD_CONFIG_PATH: String = "res://fixtures/draft_ai/draft_ai_config_legacy_lookahead.json"
const LOOKAHEAD_SOFTMAX_CONFIG_PATH: String = "res://fixtures/draft_ai/draft_ai_config_lookahead_softmax.json"

const DEFAULT_TEMPERATURE: float = 0.5
const DEFAULT_SCALE: float = 100.0
const DEFAULT_TOP_K: int = 5

const TIER_EASY: String = "easy"
const TIER_NORMAL: String = "normal"
const TIER_HARD: String = "hard"

const TIER_IDS: Array[String] = [TIER_EASY, TIER_NORMAL, TIER_HARD]

const DEFAULT_TIER_PRESETS: Dictionary = {
	TIER_EASY: {"temperature": 2.0, "top_k": 8},
	TIER_NORMAL: {"temperature": DEFAULT_TEMPERATURE, "top_k": DEFAULT_TOP_K},
	TIER_HARD: {"temperature": 0.15, "top_k": 3},
}


static func _load_config_root(config_path: String) -> Dictionary:
	if config_path.is_empty():
		return {}
	if not FileAccess.file_exists(config_path):
		return {}
	var f := FileAccess.open(config_path, FileAccess.READ)
	if f == null:
		push_warning("DraftAiConfig: could not open override file %s" % config_path)
		return {}
	var parsed: Variant = JSON.parse_string(f.get_as_text())
	f.close()
	if typeof(parsed) != TYPE_DICTIONARY:
		push_warning("DraftAiConfig: failed to parse override JSON at %s, using defaults" % config_path)
		return {}
	return parsed


static func _load_softmax_dict(config_path: String) -> Dictionary:
	var root: Dictionary = _load_config_root(config_path)
	if not root.has("softmax") or typeof(root["softmax"]) != TYPE_DICTIONARY:
		return {}
	return root["softmax"]


static func _load_tier_dict(config_path: String) -> Dictionary:
	var root: Dictionary = _load_config_root(config_path)
	if not root.has("difficulty_tiers") or typeof(root["difficulty_tiers"]) != TYPE_DICTIONARY:
		return {}
	return root["difficulty_tiers"]


static func _is_valid_number(v: Variant) -> bool:
	var t: int = typeof(v)
	return t == TYPE_FLOAT or t == TYPE_INT


static func _get_float(dict: Dictionary, key: String, default_value: float, config_path: String) -> float:
	if not dict.has(key):
		return default_value
	var v: Variant = dict[key]
	if not _is_valid_number(v):
		push_warning("DraftAiConfig: '%s' is not numeric in %s, using default %s" % [key, config_path, default_value])
		return default_value
	var result: float = float(v)
	if not is_finite(result):
		push_warning("DraftAiConfig: '%s' is not finite in %s, using default %s" % [key, config_path, default_value])
		return default_value
	return result


static func _get_int(dict: Dictionary, key: String, default_value: int, config_path: String) -> int:
	if not dict.has(key):
		return default_value
	var v: Variant = dict[key]
	if not _is_valid_number(v):
		push_warning("DraftAiConfig: '%s' is not numeric in %s, using default %s" % [key, config_path, default_value])
		return default_value
	if typeof(v) == TYPE_FLOAT:
		var result: float = float(v)
		if not is_finite(result):
			push_warning("DraftAiConfig: '%s' is not finite in %s, using default %s" % [key, config_path, default_value])
			return default_value
	return int(v)


static func is_valid_tier_id(tier_id: String) -> bool:
	return TIER_IDS.has(tier_id)


static func get_softmax_temperature(config_path: String = DEFAULT_CONFIG_PATH) -> float:
	var softmax: Dictionary = _load_softmax_dict(config_path)
	return _get_float(softmax, "temperature", DEFAULT_TEMPERATURE, config_path)


static func get_softmax_scale(config_path: String = DEFAULT_CONFIG_PATH) -> float:
	var softmax: Dictionary = _load_softmax_dict(config_path)
	return _get_float(softmax, "scale", DEFAULT_SCALE, config_path)


static func get_softmax_top_k(config_path: String = DEFAULT_CONFIG_PATH) -> int:
	var softmax: Dictionary = _load_softmax_dict(config_path)
	return _get_int(softmax, "top_k", DEFAULT_TOP_K, config_path)


static func get_tier_preset(tier_id: String, config_path: String = DEFAULT_CONFIG_PATH) -> Dictionary:
	var resolved_tier: String = tier_id
	if not is_valid_tier_id(resolved_tier):
		push_warning("DraftAiConfig: invalid tier '%s', using normal" % tier_id)
		resolved_tier = TIER_NORMAL

	var defaults: Dictionary = DEFAULT_TIER_PRESETS[resolved_tier]
	var temperature: float = float(defaults["temperature"])
	var top_k: int = int(defaults["top_k"])

	if resolved_tier == TIER_NORMAL:
		var softmax: Dictionary = _load_softmax_dict(config_path)
		temperature = _get_float(softmax, "temperature", DEFAULT_TEMPERATURE, config_path)
		top_k = _get_int(softmax, "top_k", DEFAULT_TOP_K, config_path)

	var tiers: Dictionary = _load_tier_dict(config_path)
	if tiers.has(resolved_tier) and typeof(tiers[resolved_tier]) == TYPE_DICTIONARY:
		var tier_block: Dictionary = tiers[resolved_tier]
		temperature = _get_float(tier_block, "temperature", temperature, config_path)
		top_k = _get_int(tier_block, "top_k", top_k, config_path)

	if temperature <= 0.0:
		push_warning("DraftAiConfig: tier '%s' temperature must be positive, using default" % resolved_tier)
		temperature = float(defaults["temperature"])
	if top_k < 1:
		push_warning("DraftAiConfig: tier '%s' top_k must be >= 1, using default" % resolved_tier)
		top_k = int(defaults["top_k"])

	return {"temperature": temperature, "top_k": top_k}


static func get_tier_temperature(tier_id: String, config_path: String = DEFAULT_CONFIG_PATH) -> float:
	return float(get_tier_preset(tier_id, config_path)["temperature"])


static func get_tier_top_k(tier_id: String, config_path: String = DEFAULT_CONFIG_PATH) -> int:
	return int(get_tier_preset(tier_id, config_path)["top_k"])


static func write_certification_snapshot_id(config_path: String, snapshot_id: String) -> bool:
	if snapshot_id.is_empty():
		push_error("DraftAiConfig: snapshot_id required for certification write")
		return false

	var root: Dictionary = _load_config_root(config_path)
	root["certification"] = {"stats_snapshot_id": snapshot_id}

	var global_path: String = ProjectSettings.globalize_path(config_path)
	var parent_dir: String = global_path.get_base_dir()
	if not DirAccess.dir_exists_absolute(parent_dir):
		var dir_err: Error = DirAccess.make_dir_recursive_absolute(parent_dir)
		if dir_err != OK:
			push_error("DraftAiConfig: could not create config parent dir %s" % parent_dir)
			return false

	var f := FileAccess.open(global_path, FileAccess.WRITE)
	if f == null:
		push_error("DraftAiConfig: could not write certification config %s" % config_path)
		return false
	f.store_string(JSON.stringify(root, "\t") + "\n")
	f.close()
	return true


static func run_tier_self_check() -> bool:
	var easy: Dictionary = get_tier_preset(TIER_EASY)
	var normal: Dictionary = get_tier_preset(TIER_NORMAL)
	var hard: Dictionary = get_tier_preset(TIER_HARD)
	if float(easy["temperature"]) < float(normal["temperature"]):
		push_error("DraftAiConfig tier self-check: easy temperature must be >= normal (got easy=%.3f normal=%.3f)" % [
			float(easy["temperature"]), float(normal["temperature"])
		])
		return false
	if float(normal["temperature"]) < float(hard["temperature"]):
		push_error("DraftAiConfig tier self-check: normal temperature must be >= hard (got normal=%.3f hard=%.3f)" % [
			float(normal["temperature"]), float(hard["temperature"])
		])
		return false
	for tier_id in TIER_IDS:
		var preset: Dictionary = get_tier_preset(tier_id)
		if float(preset["temperature"]) <= 0.0 or int(preset["top_k"]) < 1:
			push_error("DraftAiConfig tier self-check: invalid preset for %s" % tier_id)
			return false
	return true
