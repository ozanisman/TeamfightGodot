class_name SimulationSchema
extends RefCounted

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const EffectSpecScript := preload("res://scripts/simulation/effect_spec.gd")
const GoldenChampionSchemaPath := "res://fixtures/goldens/champion_schema.json"
const GoldenMinionSchemaPath := "res://fixtures/goldens/minion_schema.json"

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

static func _normalize_passives(passives: Dictionary) -> Dictionary:
	var normalized: Dictionary = {}
	for passive_id in passives:
		var passive_data: Dictionary = passives[passive_id]
		var normalized_passive: Dictionary = {}
		for hook in passive_data:
			var hook_data: Variant = passive_data[hook]
			if hook_data is Array:
				var normalized_effects: Array = []
				for effect in hook_data:
					if effect is EffectSpecScript:
						normalized_effects.append(effect.to_dict())
					else:
						normalized_effects.append(effect)
				normalized_passive[hook] = normalized_effects
			else:
				normalized_passive[hook] = _normalize_numbers(hook_data)
		normalized[String(passive_id)] = normalized_passive
	return normalized

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

static func generate_champion_schema_from_gdscript() -> Dictionary:
	var script_path = "res://scripts/simulation/champion_catalog.gd"

	# Force reload from disk, bypassing cache to get fresh data
	var script = ResourceLoader.load(script_path, "", ResourceLoader.CACHE_MODE_IGNORE)
	if script == null:
		push_error("Failed to load fresh champion catalog script")
		return {}

	# Call static methods on the fresh script with explicit types
	var catalog: Dictionary = script.build_catalog()
	var passives: Dictionary = script.build_passive_registry()
	var role_configs: Dictionary = script.build_role_configs()
	var minions: Dictionary = script.build_minion_catalog()

	# Start with champions at top level (for C++ compatibility)
	var result: Dictionary = {}
	for champion_id in catalog.keys():
		var champ = catalog[champion_id]
		var champ_dict: Dictionary = champ.to_dict()
		result[String(champion_id)] = champ_dict

	# Add passives and role_configs as additional top-level keys
	result["passives"] = _normalize_passives(passives)

	# Convert role_configs to dictionaries
	var role_configs_dict: Dictionary = {}
	for role_id in role_configs.keys():
		var config = role_configs[role_id]
		role_configs_dict[String(role_id)] = config.to_dict()
	result["role_configs"] = _normalize_numbers(role_configs_dict)

	return result

static func write_champion_schema_to_file(output_path: String = GoldenChampionSchemaPath) -> bool:
	var schema := generate_champion_schema_from_gdscript()
	var json_string := JSON.stringify(schema, "\t")
	if json_string.is_empty():
		push_error("Failed to stringify champion schema.")
		return false
	
	var file := FileAccess.open(output_path, FileAccess.WRITE)
	if file == null:
		push_error("Failed to open schema file for writing.")
		return false
	
	file.store_string(json_string)
	file.close()
	print("Champion schema exported successfully to: %s" % output_path)
	
	# Also export minion schema
	write_minion_schema_to_file()
	return true

static func generate_minion_schema_from_gdscript() -> Dictionary:
	var script_path = "res://scripts/simulation/champion_catalog.gd"

	# Force reload from disk, bypassing cache to get fresh data
	var script = ResourceLoader.load(script_path, "", ResourceLoader.CACHE_MODE_IGNORE)
	if script == null:
		push_error("Failed to load fresh champion catalog script for minion export")
		return {}

	var minions: Dictionary = script.build_minion_catalog()

	var result: Dictionary = {}
	for minion_id in minions.keys():
		var minion = minions[minion_id]
		var minion_dict: Dictionary = minion.to_dict()
		result[String(minion_id)] = minion_dict

	return _normalize_numbers(result)

static func write_minion_schema_to_file(output_path: String = GoldenMinionSchemaPath) -> bool:
	var schema := generate_minion_schema_from_gdscript()
	var json_string := JSON.stringify(schema, "\t")
	if json_string.is_empty():
		push_error("Failed to stringify minion schema.")
		return false
	
	var file := FileAccess.open(output_path, FileAccess.WRITE)
	if file == null:
		push_error("Failed to open minion schema file for writing.")
		return false
	
	file.store_string(json_string)
	file.close()
	print("Minion schema exported successfully to: %s" % output_path)
	return true

static func export_role_config_schema() -> Dictionary:
	var result: Dictionary = {}
	
	# Force reload from disk, bypassing cache to get fresh data
	var script_path = "res://scripts/simulation/champion_catalog.gd"
	var script = ResourceLoader.load(script_path, "", ResourceLoader.CACHE_MODE_IGNORE)
	if script == null:
		push_error("Failed to load fresh champion catalog script for role config export")
		return {}
	
	var role_configs: Dictionary = script.build_role_configs()
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
		## Native adds optional per-unit [code]telemetry[/code]; dashboards may consume later without parity coupling.
		"match_summary_unit_telemetry_v1": {
			"schema": SimConstantsScript.SIM_TELEMETRY_SCHEMA_V1,
			"hard_cc_seconds": "float>=0",
		},
		"champions": export_champion_schema(),
		"role_configs": export_role_config_schema(),
	}
