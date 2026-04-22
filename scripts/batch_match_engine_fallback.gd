extends RefCounted
class_name BatchMatchEngineFallback

const BattleMetricsScript = preload("res://scripts/battle_metrics.gd")
const CombatData = preload("res://scripts/combat_data.gd")
const CombatSimulationCoreScript = preload("res://scripts/combat_simulation_core.gd")
const BatchUnitFactoryScript = preload("res://scripts/batch_unit_factory.gd")
const BatchUnitSpecScript = preload("res://scripts/batch_unit_spec.gd")
const ChampionCatalog = preload("res://scripts/champion_catalog.gd")
const SimulationContractScript = preload("res://scripts/simulation_contract.gd")

signal combat_event_recorded(event: Dictionary)

var _world: CombatSimulationCore = null
var _units: Array[CombatUnitState] = []


func _init() -> void:
	_world = CombatSimulationCoreScript.new()
	_world.combat_event_recorded.connect(_on_world_combat_event_recorded)


func set_seed(match_seed: int) -> void:
	_world.set_seed(match_seed)


func set_units(units: Array) -> void:
	_units = _materialize_units(units)
	ChampionCatalog.bootstrap_combat_registry(_world.get_combat_registry())
	_world.set_units(_units)
	for unit in _units:
		unit.set_combat_world(_world)
		unit.set_combat_registry(_world.combat_registry)
	_world.capture_spawn_positions()


func step(dt: float) -> void:
	_world.step(dt)


func get_match_result() -> Dictionary:
	return _world.get_match_result()


func snapshot_units() -> Array[Dictionary]:
	return _world.snapshot_units()


func clear() -> void:
	_units.clear()
	_world.clear()


func build_units(player_roster: Array[String], enemy_roster: Array[String]) -> Array:
	return BatchUnitFactoryScript.build_match_units(player_roster, enemy_roster)


func run_match(
		match_seed: int,
		tick_rate: float,
		max_ticks: int,
		player_roster: Array[String],
		enemy_roster: Array[String],
		record_events: bool
	) -> Dictionary:
	_world.tick_rate = tick_rate
	_world.record_events = record_events
	_world.simulate_motion = true
	set_seed(match_seed)

	var units: Array = build_units(player_roster, enemy_roster)
	set_units(units)

	var result: Dictionary = {}
	var termination := "timeout"
	while _world.tick < max_ticks:
		step(tick_rate)
		result = get_match_result()
		if not result.is_empty():
			termination = "winner"
			break

	if result.is_empty():
		result = {
			"winner": "timeout",
			"player_kills": _world.player_kills,
			"enemy_kills": _world.enemy_kills,
			"time": _world.time,
		}

	var unit_snapshots: Array[Dictionary] = _world.snapshot_units()
	var match_record := SimulationContractScript.build_match_summary(
		termination,
		String(result.get("winner", "timeout")),
		int(result.get("player_kills", 0)),
		int(result.get("enemy_kills", 0)),
		float(result.get("time", 0.0)),
		int(_world.tick)
	)
	_world.clear()
	return {
		"match_record": match_record,
		"unit_snapshots": unit_snapshots,
	}


func get_combat_registry() -> Object:
	return _world.get_combat_registry()


func _on_world_combat_event_recorded(event: Dictionary) -> void:
	combat_event_recorded.emit(event)


func _materialize_units(units: Array) -> Array[CombatUnitState]:
	var metrics := _build_metrics()
	var result: Array[CombatUnitState] = []
	for unit in units:
		if unit is BatchUnitSpecScript:
			result.append((unit as BatchUnitSpecScript).to_combat_unit_state(metrics))
		elif unit is CombatUnitState:
			result.append(unit)
		elif unit is Dictionary:
			result.append(BatchUnitSpecScript.from_dictionary(unit).to_combat_unit_state(metrics))
	return result


func _build_metrics() -> Object:
	var metrics := BattleMetricsScript.new()
	metrics.setup_for_world(CombatData.WORLD_SIZE_VECTOR, 1.0)
	return metrics
