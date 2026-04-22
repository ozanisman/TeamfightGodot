extends SceneTree

const BatchMatchEngineFallbackScript = preload("res://scripts/batch_match_engine_fallback.gd")
const SimulationContractScript = preload("res://scripts/simulation_contract.gd")
const CombatData = preload("res://scripts/combat_data.gd")
const BatchCoreExtensionPath = "res://native/batch_core/batch_core.gdextension"

var failures: Array[String] = []


func _initialize() -> void:
	_ensure_native_extension_loaded()
	if not ClassDB.class_exists("BatchMatchEngineNative"):
		print("NATIVE PARITY SKIPPED: BatchMatchEngineNative is not registered")
		quit(0)
		return

	var code := _run_parity()
	for failure in failures:
		push_error(failure)
	if failures.is_empty():
		print("NATIVE PARITY OK")
	else:
		print("NATIVE PARITY FAIL: %d issue(s)" % failures.size())
	quit(code)


func _run_parity() -> int:
	var native_engine: Object = ClassDB.instantiate("BatchMatchEngineNative")
	var fallback_engine := BatchMatchEngineFallbackScript.new()
	var config := {
		"matches": 1,
		"seed": 4242,
		"tick_rate": CombatData.SIMULATION_TICK_RATE,
		"max_ticks": 32,
		"player_roster": ["swordsman", "archer", "oracle"],
		"enemy_roster": ["assassin", "sniper", "warlock"],
		"record_events": false,
	}

	var native_result := _run_single(native_engine, config)
	var fallback_result := _run_single(fallback_engine, config)
	var native_run_match_result := _run_match_api(native_engine, config)
	var fallback_run_match_result := _run_match_api(fallback_engine, config)

	var native_single_summary := _normalize_match_record(native_result["match_record"])
	var fallback_single_summary := _normalize_match_record(fallback_result["match_record"])
	_assert_true(native_single_summary == fallback_single_summary, "Match summary diverged: native=%s fallback=%s" % [JSON.stringify(native_single_summary), JSON.stringify(fallback_single_summary)])
	_assert_true(_normalize_snapshots(native_result["unit_snapshots"]) == _normalize_snapshots(fallback_result["unit_snapshots"]), _snapshot_diff_message(native_result["unit_snapshots"], fallback_result["unit_snapshots"]))
	var native_run_summary := _normalize_match_record(native_run_match_result["match_record"])
	var fallback_run_summary := _normalize_match_record(fallback_run_match_result["match_record"])
	_assert_true(native_run_summary == fallback_run_summary, "run_match summary diverged: native=%s fallback=%s" % [JSON.stringify(native_run_summary), JSON.stringify(fallback_run_summary)])
	_assert_true(_normalize_snapshots(native_run_match_result["unit_snapshots"]) == _normalize_snapshots(fallback_run_match_result["unit_snapshots"]), _snapshot_diff_message(native_run_match_result["unit_snapshots"], fallback_run_match_result["unit_snapshots"]))

	return 0 if failures.is_empty() else 1


func _run_single(engine: Object, config: Dictionary) -> Dictionary:
	var match_seed: int = int(config.get("seed", 0))
	var tick_rate: float = float(config.get("tick_rate", CombatData.SIMULATION_TICK_RATE))
	var max_ticks: int = int(config.get("max_ticks", 1))
	var player_roster: Array[String] = _as_string_array(config.get("player_roster", []))
	var enemy_roster: Array[String] = _as_string_array(config.get("enemy_roster", []))
	var record_events: bool = bool(config.get("record_events", false))

	if engine.has_method("set_seed"):
		engine.call("set_seed", match_seed)
	if engine.has_method("build_units"):
		var units: Variant = engine.call("build_units", player_roster, enemy_roster)
		_assert_true(units is Array, "build_units did not return an Array")
		if units is Array and engine.has_method("set_units"):
			engine.call("set_units", units)

	var result: Dictionary = {}
	for _tick in range(max_ticks):
		if engine.has_method("step"):
			engine.call("step", tick_rate)
		if engine.has_method("get_match_result"):
			result = engine.call("get_match_result")
		if not result.is_empty():
			break

	if result.is_empty():
		result = {
			"winner": "timeout",
			"player_kills": 0,
			"enemy_kills": 0,
			"time": float(max_ticks) * tick_rate,
			"ticks": max_ticks,
		}

	var snapshots: Array = []
	if engine.has_method("snapshot_units"):
		snapshots = engine.call("snapshot_units")

	if engine.has_method("clear"):
		engine.call("clear")

	return {
		"match_record": SimulationContractScript.build_match_summary(
			String(result.get("termination", "timeout")),
			String(result.get("winner", "timeout")),
			int(result.get("player_kills", 0)),
			int(result.get("enemy_kills", 0)),
			float(result.get("time", 0.0)),
			int(result.get("ticks", max_ticks))
		),
		"unit_snapshots": snapshots,
	}


func _run_match_api(engine: Object, config: Dictionary) -> Dictionary:
	if not engine.has_method("run_match"):
		return {"match_record": {}, "unit_snapshots": []}
	var result: Variant = engine.call(
		"run_match",
		int(config.get("seed", 0)),
		float(config.get("tick_rate", CombatData.SIMULATION_TICK_RATE)),
		int(config.get("max_ticks", 1)),
		_as_string_array(config.get("player_roster", [])),
		_as_string_array(config.get("enemy_roster", [])),
		bool(config.get("record_events", false))
	)
	if result is Dictionary:
		return result
	return {"match_record": {}, "unit_snapshots": []}


func _normalize_match_record(record: Dictionary) -> Dictionary:
	return SimulationContractScript.build_match_summary(
		String(record.get("termination", "")),
		String(record.get("winner", "")),
		int(record.get("player_kills", 0)),
		int(record.get("enemy_kills", 0)),
		_round_time(float(record.get("time", 0.0))),
		int(record.get("ticks", 0))
	)


func _normalize_snapshots(snapshots: Array) -> Array[Dictionary]:
	var normalized: Array[Dictionary] = []
	for snapshot in snapshots:
		if snapshot is Dictionary:
			var unit: Dictionary = snapshot
			normalized.append({
				"instance_id": int(unit.get("instance_id", 0)),
				"hero_id": String(unit.get("hero_id", "")),
				"display_name": String(unit.get("display_name", "")),
				"role": String(unit.get("role", "")),
				"team": String(unit.get("team", "")),
				"passive_id": String(unit.get("passive_id", "")),
				"alive": bool(unit.get("alive", false)),
				"hp": _round_float(float(unit.get("hp", 0.0))),
				"shield": _round_float(float(unit.get("shield", 0.0))),
				"position": _round_vector2(unit.get("position", Vector2.ZERO)),
				"target_id": int(unit.get("target_id", -1)),
				"in_range": bool(unit.get("in_range", false)),
				"kills": int(unit.get("kills", 0)),
				"deaths": int(unit.get("deaths", 0)),
				"assists": int(unit.get("assists", 0)),
				"damage_dealt": _round_float(float(unit.get("damage_dealt", 0.0))),
				"damage_dealt_auto": _round_float(float(unit.get("damage_dealt_auto", 0.0))),
				"damage_dealt_ability": _round_float(float(unit.get("damage_dealt_ability", 0.0))),
				"damage_dealt_ultimate": _round_float(float(unit.get("damage_dealt_ultimate", 0.0))),
				"damage_received": _round_float(float(unit.get("damage_received", 0.0))),
				"damage_mitigated": _round_float(float(unit.get("damage_mitigated", 0.0))),
				"healing_done": _round_float(float(unit.get("healing_done", 0.0))),
				"shielding_done": _round_float(float(unit.get("shielding_done", 0.0))),
				"auto_attacks_done": int(unit.get("auto_attacks_done", 0)),
				"abilities_used": int(unit.get("abilities_used", 0)),
				"ultimates_used": int(unit.get("ultimates_used", 0)),
				"stuns_dealt": int(unit.get("stuns_dealt", 0)),
			})
	normalized.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		return int(a.get("instance_id", 0)) < int(b.get("instance_id", 0))
	)
	return normalized


func _assert_true(condition: bool, label: String) -> void:
	if not condition:
		failures.append(label)


func _as_string_array(values: Array) -> Array[String]:
	var typed_values: Array[String] = []
	typed_values.resize(values.size())
	for index in range(values.size()):
		typed_values[index] = String(values[index])
	return typed_values


func _round_float(value: float) -> float:
	return snappedf(value, 0.000001)


func _round_time(value: float) -> float:
	return snappedf(value, 0.001)


func _round_vector2(value: Variant) -> Vector2:
	if value is Vector2:
		var vector := value as Vector2
		return Vector2(_round_float(vector.x), _round_float(vector.y))
	return Vector2.ZERO


func _ensure_native_extension_loaded() -> void:
	if ClassDB.class_exists("BatchMatchEngineNative"):
		return
	var extension_config: Resource = load(BatchCoreExtensionPath)
	if extension_config == null:
		return


func _snapshot_diff_message(native_snapshots: Array, fallback_snapshots: Array) -> String:
	var native_norm := _normalize_snapshots(native_snapshots)
	var fallback_norm := _normalize_snapshots(fallback_snapshots)
	if native_norm.size() != fallback_norm.size():
		return "Snapshot counts differ: native=%d fallback=%d" % [native_norm.size(), fallback_norm.size()]
	for index in range(min(native_norm.size(), fallback_norm.size())):
		if native_norm[index] != fallback_norm[index]:
			return "Snapshot %d diverged: native=%s fallback=%s" % [index, JSON.stringify(native_norm[index]), JSON.stringify(fallback_norm[index])]
	return "Snapshot divergence with identical normalized payloads"
