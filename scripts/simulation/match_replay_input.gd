class_name MatchReplayInput
extends RefCounted

const SimConstantsScript := preload("res://scripts/simulation/sim_constants.gd")
const SpawnSpecScript := preload("res://scripts/simulation/spawn_spec.gd")

var seed: int = 0
var tick_rate: float = SimConstantsScript.DEFAULT_TICK_RATE
var player_units: Array = []
var enemy_units: Array = []
var record_events: bool = false
var rules_version: int = SimConstantsScript.SIMULATION_RULES_VERSION
var balance_version: StringName = &"default"

func _init(
	_seed: int = 0,
	_tick_rate: float = SimConstantsScript.DEFAULT_TICK_RATE,
	_player_units: Array = [],
	_enemy_units: Array = [],
	_record_events: bool = false,
	_rules_version: int = SimConstantsScript.SIMULATION_RULES_VERSION,
	_balance_version: StringName = &"default"
) -> void:
	seed = _seed
	tick_rate = _tick_rate
	player_units = _player_units.duplicate()
	enemy_units = _enemy_units.duplicate()
	record_events = _record_events
	rules_version = _rules_version
	balance_version = _balance_version

func all_units():
	var result: Array = []
	result.append_array(player_units)
	result.append_array(enemy_units)
	return result

func to_dict() -> Dictionary:
	return {
		"seed": seed,
		"tick_rate": tick_rate,
		"player_units": player_units.map(func(value): return value.to_dict()),
		"enemy_units": enemy_units.map(func(value): return value.to_dict()),
		"record_events": record_events,
		"rules_version": rules_version,
		"balance_version": String(balance_version),
	}

static func from_dict(data: Dictionary):
	var players: Array = []
	for item in Array(data.get("player_units", [])):
		players.append(SpawnSpecScript.from_dict(Dictionary(item)))
	var enemies: Array = []
	for item in Array(data.get("enemy_units", [])):
		enemies.append(SpawnSpecScript.from_dict(Dictionary(item)))
	var input := new()
	input.seed = int(data.get("seed", 0))
	input.tick_rate = float(data.get("tick_rate", SimConstantsScript.DEFAULT_TICK_RATE))
	input.player_units = players
	input.enemy_units = enemies
	input.record_events = bool(data.get("record_events", false))
	input.rules_version = int(data.get("rules_version", SimConstantsScript.SIMULATION_RULES_VERSION))
	input.balance_version = StringName(String(data.get("balance_version", "default")))
	return input

static func build_match_input(
	seed: int,
	player_types: Array[StringName],
	enemy_types: Array[StringName],
	_tick_rate: float = SimConstantsScript.SIMULATION_TICK_RATE
):
	var player_units: Array = []
	for index in range(player_types.size()):
		var position := SimConstantsScript.spawn_position(index, &"player")
		player_units.append(SpawnSpecScript.new(player_types[index], &"player", position.x, position.y))
	var enemy_units: Array = []
	for index in range(enemy_types.size()):
		var position := SimConstantsScript.spawn_position(index, &"enemy")
		enemy_units.append(SpawnSpecScript.new(enemy_types[index], &"enemy", position.x, position.y))
	var input := new()
	input.seed = seed
	input.tick_rate = _tick_rate
	input.player_units = player_units
	input.enemy_units = enemy_units
	return input
