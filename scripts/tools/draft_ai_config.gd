extends RefCounted
## Centralized GDScript-side draft AI tunables (Workstream 0.2). Mirrors the native
## sim::draft_ai::Config softmax defaults so gameplay (simulation_viewer_base.gd) and the
## validation harness (draft_strategy_native_softmax.gd) stay in sync. Reads the same optional
## JSON override file as the native side; falls back to hardcoded defaults when absent.

## Optional JSON override file shared with the native sim::draft_ai::Config loader
## (sim_draft_ai_config.cpp). Absent by default; when present, both native scoring
## weights and this GDScript softmax config read from the same file.
const DEFAULT_CONFIG_PATH: String = "res://model_stats/draft_ai_config.json"

const DEFAULT_TEMPERATURE: float = 0.5
const DEFAULT_SCALE: float = 100.0
const DEFAULT_TOP_K: int = 5

static func _load_softmax_dict(config_path: String) -> Dictionary:
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
	var root: Dictionary = parsed
	if not root.has("softmax") or typeof(root["softmax"]) != TYPE_DICTIONARY:
		return {}
	return root["softmax"]

static func _is_valid_number(v: Variant) -> bool:
	var t: int = typeof(v)
	return t == TYPE_FLOAT or t == TYPE_INT

static func _get_float(softmax: Dictionary, key: String, default_value: float, config_path: String) -> float:
	if not softmax.has(key):
		return default_value
	var v: Variant = softmax[key]
	if not _is_valid_number(v):
		push_warning("DraftAiConfig: '%s' is not numeric in %s, using default %s" % [key, config_path, default_value])
		return default_value
	var result: float = float(v)
	if not is_finite(result):
		push_warning("DraftAiConfig: '%s' is not finite in %s, using default %s" % [key, config_path, default_value])
		return default_value
	return result

static func _get_int(softmax: Dictionary, key: String, default_value: int, config_path: String) -> int:
	if not softmax.has(key):
		return default_value
	var v: Variant = softmax[key]
	if not _is_valid_number(v):
		push_warning("DraftAiConfig: '%s' is not numeric in %s, using default %s" % [key, config_path, default_value])
		return default_value
	if typeof(v) == TYPE_FLOAT:
		var result: float = float(v)
		if not is_finite(result):
			push_warning("DraftAiConfig: '%s' is not finite in %s, using default %s" % [key, config_path, default_value])
			return default_value
	return int(v)

static func get_softmax_temperature(config_path: String = DEFAULT_CONFIG_PATH) -> float:
	var softmax: Dictionary = _load_softmax_dict(config_path)
	return _get_float(softmax, "temperature", DEFAULT_TEMPERATURE, config_path)

static func get_softmax_scale(config_path: String = DEFAULT_CONFIG_PATH) -> float:
	var softmax: Dictionary = _load_softmax_dict(config_path)
	return _get_float(softmax, "scale", DEFAULT_SCALE, config_path)

static func get_softmax_top_k(config_path: String = DEFAULT_CONFIG_PATH) -> int:
	var softmax: Dictionary = _load_softmax_dict(config_path)
	return _get_int(softmax, "top_k", DEFAULT_TOP_K, config_path)
