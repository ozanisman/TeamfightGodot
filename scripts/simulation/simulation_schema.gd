class_name SimulationSchema
extends RefCounted

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const GoldenChampionSchemaPath := "res://fixtures/goldens/champion_schema.json"

static func _normalize_numbers(value: Variant) -> Variant:
	return _normalize_numbers_for_key(value, "")

static func _normalize_numbers_for_key(value: Variant, key_name: String) -> Variant:
	if value is Dictionary:
		var normalized: Dictionary = {}
		for key in value.keys():
			normalized[key] = _normalize_numbers_for_key(value[key], String(key))
		return normalized
	if value is Array:
		var normalized_array: Array = []
		normalized_array.resize(value.size())
		for index in range(value.size()):
			normalized_array[index] = _normalize_numbers_for_key(value[index], key_name)
		return normalized_array
	if (key_name == "attack_range" or key_name == "move_speed" or key_name == "color") and value is float and is_equal_approx(value, round(value)):
		return int(round(value))
	return value

static func export_champion_schema() -> Dictionary:
	var file := FileAccess.open(GoldenChampionSchemaPath, FileAccess.READ)
	if file == null:
		push_error("Failed to open golden champion schema fixture.")
		return {}

	var parsed: Variant = JSON.parse_string(file.get_as_text())
	if parsed is Dictionary:
		return Dictionary(_normalize_numbers(parsed))

	push_error("Failed to parse golden champion schema fixture.")
	return {}

static func export_role_config_schema() -> Dictionary:
	var result: Dictionary = {}
	var role_configs := ChampionCatalogScript.build_role_configs()
	for role_id in role_configs.keys():
		var config = role_configs[role_id]
		result[String(role_id)] = config.to_dict()
	return result

static func export_contract_schema() -> Dictionary:
	return {
		"rules_version": SimConstantsScript.SIMULATION_RULES_VERSION,
		"match_input": {
			"seed": "int",
			"tick_rate": "float",
			"player_units": "array<SpawnSpec>",
			"enemy_units": "array<SpawnSpec>",
			"record_events": "bool",
			"rules_version": "int",
			"balance_version": "string",
		},
		"match_summary": {
			"seed": "int",
			"winner_team": "string|null",
			"duration": "float",
			"unit_stats": "array<UnitReplaySummary>",
			"player_comp": "array<string>",
			"enemy_comp": "array<string>",
		},
		"champions": export_champion_schema(),
		"role_configs": export_role_config_schema(),
	}
