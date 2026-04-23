class_name SimulationSchema
extends RefCounted

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const GoldenChampionSchemaPath := "res://fixtures/goldens/champion_schema.json"

static func export_champion_schema() -> Dictionary:
	var file := FileAccess.open(GoldenChampionSchemaPath, FileAccess.READ)
	if file != null:
		var parsed: Variant = JSON.parse_string(file.get_as_text())
		if parsed is Dictionary:
			return Dictionary(parsed)
		push_error("Failed to parse golden champion schema fixture.")
	return ChampionCatalogScript.export_schema_dict()

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
