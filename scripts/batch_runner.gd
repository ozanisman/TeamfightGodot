extends Node
class_name BatchRunner

const CombatData = preload("res://scripts/combat_data.gd")
const BatchMatchEngineScript = preload("res://scripts/batch_match_engine.gd")
const SimulationContractScript = preload("res://scripts/simulation_contract.gd")
const SimulationStatsSinkScript = preload("res://scripts/simulation_stats_sink.gd")

const BATCH_SCHEMA_VERSION: int = SimulationContractScript.BATCH_SCHEMA_VERSION
const DEFAULT_PLAYER_ROSTER: Array[String] = ["swordsman", "archer", "oracle", "mage", "guardian"]
const DEFAULT_ENEMY_ROSTER: Array[String] = ["assassin", "sniper", "warlock", "rogue", "colossus"]

var auto_run: bool = false


func _ready() -> void:
	if auto_run and not Engine.is_editor_hint():
		run_from_cli()


func run_from_cli() -> void:
	var config := parse_cli_args(OS.get_cmdline_user_args())
	var started_usec := Time.get_ticks_usec()
	var result := run_batch(config)
	var elapsed_sec := float(Time.get_ticks_usec() - started_usec) / 1000000.0
	result["elapsed_seconds"] = elapsed_sec
	if elapsed_sec > 0.0:
		result["matches_per_second"] = float(result.get("matches", 0)) / elapsed_sec
	print("BATCH COMPLETE: %s" % JSON.stringify(result))
	if bool(config.get("benchmark", false)):
		result["benchmark"] = SimulationContractScript.build_benchmark_summary(result, elapsed_sec, true)
		print("BATCH BENCHMARK: %s" % JSON.stringify(result["benchmark"]))
	if get_tree() != null:
		get_tree().quit(int(result.get("exit_code", 0)))


func parse_cli_args(args: PackedStringArray) -> Dictionary:
	var config: Dictionary = {
		"matches": CombatData.DEFAULT_SIMULATION_ROUNDS,
		"seed": CombatData.SIMULATION_BASE_SEED,
		"tick_rate": CombatData.SIMULATION_TICK_RATE,
		"max_ticks": CombatData.DEFAULT_MAX_SIM_TICKS,
		"chunk_records": CombatData.SIMULATION_STATS_CHUNK_RECORDS,
		"output_path": "%s/%s/batch_stats.jsonl" % [ProjectSettings.globalize_path("res://").trim_suffix("/"), CombatData.SIMULATION_STATS_DIR],
		"record_events": false,
		"include_unit_summaries": false,
		"summary_only": false,
		"benchmark": false,
		"compute_only": false,
		"player_roster": DEFAULT_PLAYER_ROSTER.duplicate(),
		"enemy_roster": DEFAULT_ENEMY_ROSTER.duplicate(),
	}
	for arg in args:
		if not arg.begins_with("--"):
			continue
		var key_value := arg.substr(2)
		var pieces := key_value.split("=", false, 2)
		var key := pieces[0]
		var value := pieces[1] if pieces.size() > 1 else "true"
		match key:
			"matches":
				config["matches"] = max(1, int(value))
			"seed":
				config["seed"] = int(value)
			"tick-rate":
				config["tick_rate"] = maxf(CombatData.EPSILON, float(value))
			"max-ticks":
				config["max_ticks"] = max(1, int(value))
			"output":
				config["output_path"] = value
			"record-events":
				config["record_events"] = _parse_bool(value)
			"unit-summaries":
				config["include_unit_summaries"] = _parse_bool(value)
			"chunk-records", "chunk-size":
				config["chunk_records"] = max(0, int(value))
			"player-roster":
				config["player_roster"] = _parse_roster(value)
			"enemy-roster":
				config["enemy_roster"] = _parse_roster(value)
			"summary-only":
				config["summary_only"] = _parse_bool(value)
			"benchmark":
				config["benchmark"] = _parse_bool(value)
			"compute-only", "benchmark-compute":
				config["compute_only"] = _parse_bool(value)
	config["player_roster"] = SimulationContractScript.normalize_roster(config.get("player_roster", DEFAULT_PLAYER_ROSTER))
	config["enemy_roster"] = SimulationContractScript.normalize_roster(config.get("enemy_roster", DEFAULT_ENEMY_ROSTER))
	return config


func run_batch(config: Dictionary) -> Dictionary:
	var output_path := String(config.get("output_path", "user://%s/batch_stats.jsonl" % CombatData.SIMULATION_STATS_DIR))
	var compute_only := bool(config.get("compute_only", false))
	var sink: Object = null
	if not compute_only:
		sink = SimulationStatsSinkScript.new()

	var matches: int = config.get("matches", 1)
	if matches < 1:
		matches = 1
	var base_seed: int = config.get("seed", CombatData.SIMULATION_BASE_SEED)
	var tick_rate: float = config.get("tick_rate", CombatData.SIMULATION_TICK_RATE)
	if tick_rate < CombatData.EPSILON:
		tick_rate = CombatData.EPSILON
	var max_ticks: int = config.get("max_ticks", CombatData.DEFAULT_MAX_SIM_TICKS)
	if max_ticks < 1:
		max_ticks = 1
	var chunk_records: int = config.get("chunk_records", CombatData.SIMULATION_STATS_CHUNK_RECORDS)
	if chunk_records < 0:
		chunk_records = 0
	var player_roster: Array[String] = []
	player_roster.assign(SimulationContractScript.normalize_roster(config.get("player_roster", DEFAULT_PLAYER_ROSTER)))
	var enemy_roster: Array[String] = []
	enemy_roster.assign(SimulationContractScript.normalize_roster(config.get("enemy_roster", DEFAULT_ENEMY_ROSTER)))
	var record_events := bool(config.get("record_events", false))
	var include_unit_summaries := bool(config.get("include_unit_summaries", false))
	var summary_only := bool(config.get("summary_only", false))
	if summary_only:
		record_events = false
		include_unit_summaries = false

	var aggregates: Dictionary = {}
	var team_aggregates: Dictionary = {}
	var role_aggregates: Dictionary = {}
	var passive_aggregates: Dictionary = {}
	var termination_counts: Dictionary = {"winner": 0, "timeout": 0}
	var wins := {"player": 0, "enemy": 0, "draw": 0}
	var timeouts := 0
	var benchmark := bool(config.get("benchmark", false))
	var match_engine := BatchMatchEngineScript.new()

	if sink != null and not sink.open(output_path, SimulationContractScript.build_batch_header(
		matches,
		base_seed,
		tick_rate,
		max_ticks,
		record_events,
		include_unit_summaries,
		summary_only,
		compute_only,
		chunk_records,
		player_roster,
		enemy_roster,
		output_path,
		benchmark
	), chunk_records):
		return {
			"exit_code": 1,
			"error": "Failed to open output path: %s" % output_path,
		}
	if record_events and sink != null:
		match_engine.combat_event_recorded.connect(_on_combat_event_recorded.bind(sink))
	for match_index in range(matches):
		var match_seed := SimulationContractScript.derive_match_seed(base_seed, match_index)
		var match_result := _run_match(
			match_engine,
			match_index,
			match_seed,
			tick_rate,
			max_ticks,
			player_roster,
			enemy_roster,
			record_events,
			include_unit_summaries,
			sink,
			aggregates,
			team_aggregates,
			role_aggregates,
			passive_aggregates,
			termination_counts
		)
		var winner := String(match_result.get("winner", ""))
		if wins.has(winner):
			wins[winner] = int(wins[winner]) + 1
		else:
			timeouts += 1
		if sink != null:
			sink.write_record(match_result)

	if sink != null:
		for key in _sorted_aggregate_keys(aggregates):
			sink.write_record(_normalized_aggregate_record(aggregates[key]))

		for key in _sorted_team_aggregate_keys(team_aggregates):
			sink.write_record(_normalized_aggregate_record(team_aggregates[key]))

		for key in _sorted_role_aggregate_keys(role_aggregates):
			sink.write_record(_normalized_aggregate_record(role_aggregates[key]))

		for key in _sorted_passive_aggregate_keys(passive_aggregates):
			sink.write_record(_normalized_aggregate_record(passive_aggregates[key]))

	var batch_result := {
		"exit_code": 0,
		"matches": matches,
		"wins": wins,
		"timeouts": timeouts,
		"outcome_counts": {
			"player": int(wins.get("player", 0)),
			"enemy": int(wins.get("enemy", 0)),
			"draw": int(wins.get("draw", 0)),
			"timeout": timeouts,
		},
		"termination_counts": termination_counts,
		"output_path": output_path,
		"schema_version": BATCH_SCHEMA_VERSION,
		"simulation_schema_version": SimulationContractScript.SIMULATION_SCHEMA_VERSION,
		"summary_only": summary_only,
		"chunk_records": chunk_records,
		"mode_flags": SimulationContractScript.build_mode_flags(record_events, include_unit_summaries, summary_only, compute_only, benchmark),
	}
	if sink != null:
		sink.write_record({
			"kind": "batch_summary",
			"schema_version": BATCH_SCHEMA_VERSION,
			"simulation_schema_version": SimulationContractScript.SIMULATION_SCHEMA_VERSION,
			"matches": matches,
			"wins": wins,
			"timeouts": timeouts,
			"outcome_counts": {
				"player": int(wins.get("player", 0)),
				"enemy": int(wins.get("enemy", 0)),
				"draw": int(wins.get("draw", 0)),
				"timeout": timeouts,
			},
			"termination_counts": termination_counts,
			"output_path": output_path,
			"chunk_records": chunk_records,
			"mode_flags": SimulationContractScript.build_mode_flags(record_events, include_unit_summaries, summary_only, compute_only, benchmark),
		})
		sink.close()
	return batch_result


func _run_match(
		match_engine: Object,
		match_index: int,
		match_seed: int,
		tick_rate: float,
		max_ticks: int,
		player_roster: Array[String],
		enemy_roster: Array[String],
	record_events: bool,
	include_unit_summaries: bool,
	sink: Object,
	aggregates: Dictionary,
	team_aggregates: Dictionary,
	role_aggregates: Dictionary,
		passive_aggregates: Dictionary,
		termination_counts: Dictionary
	) -> Dictionary:
	var units: Array = match_engine.build_units(player_roster, enemy_roster)
	match_engine.set_seed(match_seed)
	match_engine.set_units(units)
	var result: Dictionary = {}
	var ticks_completed := 0
	var termination := "timeout"
	for tick_index in range(max_ticks):
		match_engine.step(tick_rate)
		ticks_completed = tick_index + 1
		result = match_engine.get_match_result()
		if not result.is_empty():
			termination = String(result.get("termination", "winner"))
			break
	if result.is_empty():
		result = {
			"winner": "timeout",
			"player_kills": 0,
			"enemy_kills": 0,
			"time": float(ticks_completed) * tick_rate,
		}
	var match_record := SimulationContractScript.build_match_summary(termination, String(result.get("winner", "timeout")), int(result.get("player_kills", 0)), int(result.get("enemy_kills", 0)), float(result.get("time", 0.0)), ticks_completed)
	match_record["match_index"] = match_index
	match_record["seed"] = match_seed
	match_record["player_roster"] = player_roster.duplicate()
	match_record["enemy_roster"] = enemy_roster.duplicate()
	var unit_snapshots: Array[Dictionary] = match_engine.snapshot_units()
	var match_termination := String(match_record.get("termination", "timeout"))
	termination_counts[match_termination] = int(termination_counts.get(match_termination, 0)) + 1
	var team_totals := _match_team_totals_from_snapshots(unit_snapshots)

	if include_unit_summaries:
		for unit_snapshot in unit_snapshots:
			sink.write_record(_unit_summary_record_from_snapshot(unit_snapshot, match_index, match_seed, match_record, team_totals))

	_accumulate_aggregates_from_snapshots(aggregates, team_aggregates, role_aggregates, passive_aggregates, unit_snapshots, match_record)
	match_engine.clear()
	return match_record


func _unit_summary_record_from_snapshot(unit: Dictionary, match_index: int, match_seed: int, match_record: Dictionary, team_totals: Dictionary) -> Dictionary:
	var team_key: String = _snapshot_team(unit)
	var team_record: Dictionary = team_totals.get(team_key, {})
	var match_record_totals: Dictionary = team_totals.get("__match__", {})
	var team_damage_total: float = float(team_record.get("damage_dealt", 0.0))
	var match_damage_total: float = float(match_record_totals.get("damage_dealt", 0.0))
	var damage_dealt: float = _snapshot_damage_dealt(unit)
	return {
		"kind": "unit_summary",
		"schema_version": BATCH_SCHEMA_VERSION,
		"match_index": match_index,
		"seed": match_seed,
		"winner": String(match_record.get("winner", "timeout")),
		"hero_id": _snapshot_hero_id(unit),
		"hero_name": _snapshot_display_name(unit),
		"team": team_key,
		"alive": bool(unit.get("alive", false)),
		"hp": float(unit.get("hp", 0.0)),
		"shield": float(unit.get("shield", 0.0)),
		"role": _snapshot_role(unit),
		"passive_id": _snapshot_passive_id(unit),
		"kills": int(unit.get("kills", 0)),
		"deaths": int(unit.get("deaths", 0)),
		"assists": int(unit.get("assists", 0)),
		"damage_dealt": damage_dealt,
		"damage_dealt_auto": float(unit.get("damage_dealt_auto", 0.0)),
		"damage_dealt_ability": float(unit.get("damage_dealt_ability", 0.0)),
		"damage_dealt_ultimate": float(unit.get("damage_dealt_ultimate", 0.0)),
		"damage_received": float(unit.get("damage_received", 0.0)),
		"damage_mitigated": float(unit.get("damage_mitigated", 0.0)),
		"healing_done": float(unit.get("healing_done", 0.0)),
		"shielding_done": float(unit.get("shielding_done", 0.0)),
		"auto_attacks_done": int(unit.get("auto_attacks_done", 0)),
		"abilities_used": int(unit.get("abilities_used", 0)),
		"ultimates_used": int(unit.get("ultimates_used", 0)),
		"stuns_dealt": int(unit.get("stuns_dealt", 0)),
		"damage_share_of_team": damage_dealt / team_damage_total if team_damage_total > 0.0 else 0.0,
		"damage_share_of_match": damage_dealt / match_damage_total if match_damage_total > 0.0 else 0.0,
	}


func _accumulate_aggregates_from_snapshots(aggregates: Dictionary, team_aggregates: Dictionary, role_aggregates: Dictionary, passive_aggregates: Dictionary, units: Array[Dictionary], match_record: Dictionary) -> void:
	var winner := String(match_record.get("winner", "timeout"))
	for unit in units:
		var team: String = _snapshot_team(unit)
		var hero_id: String = _snapshot_hero_id(unit)
		var key := "%s:%s" % [team, hero_id]
		if not aggregates.has(key):
			aggregates[key] = {
				"kind": "hero_aggregate",
				"schema_version": BATCH_SCHEMA_VERSION,
				"team": team,
				"hero_id": hero_id,
				"hero_name": _snapshot_display_name(unit),
				"role": _snapshot_role(unit),
				"passive_id": _snapshot_passive_id(unit),
				"matches": 0,
				"wins": 0,
				"losses": 0,
				"draws": 0,
				"kills": 0,
				"deaths": 0,
				"assists": 0,
				"damage_dealt": 0.0,
				"damage_dealt_auto": 0.0,
				"damage_dealt_ability": 0.0,
				"damage_dealt_ultimate": 0.0,
				"damage_received": 0.0,
				"damage_mitigated": 0.0,
				"healing_done": 0.0,
				"shielding_done": 0.0,
				"auto_attacks_done": 0,
				"abilities_used": 0,
				"ultimates_used": 0,
				"stuns_dealt": 0,
				"duration_seconds_total": 0.0,
			}
		var aggregate: Dictionary = aggregates[key]
		aggregate["matches"] = int(aggregate["matches"]) + 1
		if winner == "draw" or winner == "timeout":
			aggregate["draws"] = int(aggregate["draws"]) + 1
		elif winner == team:
			aggregate["wins"] = int(aggregate["wins"]) + 1
		else:
			aggregate["losses"] = int(aggregate["losses"]) + 1
		_accumulate_snapshot_fields(aggregate, unit)
		aggregate["duration_seconds_total"] = float(aggregate["duration_seconds_total"]) + float(match_record.get("duration_seconds", 0.0))
		aggregates[key] = aggregate

		var team_key: String = team
		if not team_aggregates.has(team_key):
			team_aggregates[team_key] = {
				"kind": "team_aggregate",
				"schema_version": BATCH_SCHEMA_VERSION,
				"team": team_key,
				"matches": 0,
				"wins": 0,
				"losses": 0,
				"draws": 0,
				"kills": 0,
				"deaths": 0,
				"assists": 0,
				"damage_dealt": 0.0,
				"damage_dealt_auto": 0.0,
				"damage_dealt_ability": 0.0,
				"damage_dealt_ultimate": 0.0,
				"damage_received": 0.0,
				"damage_mitigated": 0.0,
				"healing_done": 0.0,
				"shielding_done": 0.0,
				"auto_attacks_done": 0,
				"abilities_used": 0,
				"ultimates_used": 0,
				"stuns_dealt": 0,
				"duration_seconds_total": 0.0,
			}
		var team_aggregate: Dictionary = team_aggregates[team_key]
		team_aggregate["matches"] = int(team_aggregate["matches"]) + 1
		if winner == "draw" or winner == "timeout":
			team_aggregate["draws"] = int(team_aggregate["draws"]) + 1
		elif winner == team_key:
			team_aggregate["wins"] = int(team_aggregate["wins"]) + 1
		else:
			team_aggregate["losses"] = int(team_aggregate["losses"]) + 1
		_accumulate_snapshot_fields(team_aggregate, unit)
		team_aggregate["duration_seconds_total"] = float(team_aggregate["duration_seconds_total"]) + float(match_record.get("duration_seconds", 0.0))
		team_aggregates[team_key] = team_aggregate

		var role_key: String = "%s:%s" % [team, _snapshot_role(unit)]
		if not role_aggregates.has(role_key):
			role_aggregates[role_key] = {
				"kind": "role_aggregate",
				"schema_version": BATCH_SCHEMA_VERSION,
				"team": team,
				"role": _snapshot_role(unit),
				"matches": 0,
				"wins": 0,
				"losses": 0,
				"draws": 0,
				"kills": 0,
				"deaths": 0,
				"assists": 0,
				"damage_dealt": 0.0,
				"damage_dealt_auto": 0.0,
				"damage_dealt_ability": 0.0,
				"damage_dealt_ultimate": 0.0,
				"damage_received": 0.0,
				"damage_mitigated": 0.0,
				"healing_done": 0.0,
				"shielding_done": 0.0,
				"auto_attacks_done": 0,
				"abilities_used": 0,
				"ultimates_used": 0,
				"stuns_dealt": 0,
				"duration_seconds_total": 0.0,
			}
		var role_aggregate: Dictionary = role_aggregates[role_key]
		_accumulate_snapshot_fields(role_aggregate, unit)
		_accumulate_outcome_fields(role_aggregate, team, winner)
		role_aggregate["duration_seconds_total"] = float(role_aggregate["duration_seconds_total"]) + float(match_record.get("duration_seconds", 0.0))
		role_aggregates[role_key] = role_aggregate

		var passive_key: String = "%s:%s" % [team, _snapshot_passive_id(unit)]
		if not passive_aggregates.has(passive_key):
			passive_aggregates[passive_key] = {
				"kind": "passive_aggregate",
				"schema_version": BATCH_SCHEMA_VERSION,
				"team": team,
				"passive_id": _snapshot_passive_id(unit),
				"matches": 0,
				"wins": 0,
				"losses": 0,
				"draws": 0,
				"kills": 0,
				"deaths": 0,
				"assists": 0,
				"damage_dealt": 0.0,
				"damage_dealt_auto": 0.0,
				"damage_dealt_ability": 0.0,
				"damage_dealt_ultimate": 0.0,
				"damage_received": 0.0,
				"damage_mitigated": 0.0,
				"healing_done": 0.0,
				"shielding_done": 0.0,
				"auto_attacks_done": 0,
				"abilities_used": 0,
				"ultimates_used": 0,
				"stuns_dealt": 0,
				"duration_seconds_total": 0.0,
			}
		var passive_aggregate: Dictionary = passive_aggregates[passive_key]
		_accumulate_snapshot_fields(passive_aggregate, unit)
		_accumulate_outcome_fields(passive_aggregate, team, winner)
		passive_aggregate["duration_seconds_total"] = float(passive_aggregate["duration_seconds_total"]) + float(match_record.get("duration_seconds", 0.0))
		passive_aggregates[passive_key] = passive_aggregate


func _snapshot_team(unit: Dictionary) -> String:
	return String(unit.get("team", ""))


func _snapshot_hero_id(unit: Dictionary) -> String:
	return String(unit.get("hero_id", ""))


func _snapshot_role(unit: Dictionary) -> String:
	return String(unit.get("role", ""))


func _snapshot_passive_id(unit: Dictionary) -> String:
	return String(unit.get("passive_id", ""))


func _snapshot_display_name(unit: Dictionary) -> String:
	return String(unit.get("display_name", ""))


func _snapshot_damage_dealt(unit: Dictionary) -> float:
	return float(unit.get("damage_dealt", 0.0))


func _snapshot_damage_dealt_auto(unit: Dictionary) -> float:
	return float(unit.get("damage_dealt_auto", 0.0))


func _snapshot_damage_dealt_ability(unit: Dictionary) -> float:
	return float(unit.get("damage_dealt_ability", 0.0))


func _snapshot_damage_dealt_ultimate(unit: Dictionary) -> float:
	return float(unit.get("damage_dealt_ultimate", 0.0))


func _accumulate_snapshot_fields(record: Dictionary, unit: Dictionary) -> void:
	record["kills"] = int(record["kills"]) + int(unit.get("kills", 0))
	record["deaths"] = int(record["deaths"]) + int(unit.get("deaths", 0))
	record["assists"] = int(record["assists"]) + int(unit.get("assists", 0))
	record["damage_dealt"] = float(record["damage_dealt"]) + _snapshot_damage_dealt(unit)
	record["damage_dealt_auto"] = float(record["damage_dealt_auto"]) + _snapshot_damage_dealt_auto(unit)
	record["damage_dealt_ability"] = float(record["damage_dealt_ability"]) + _snapshot_damage_dealt_ability(unit)
	record["damage_dealt_ultimate"] = float(record["damage_dealt_ultimate"]) + _snapshot_damage_dealt_ultimate(unit)
	record["damage_received"] = float(record["damage_received"]) + float(unit.get("damage_received", 0.0))
	record["damage_mitigated"] = float(record["damage_mitigated"]) + float(unit.get("damage_mitigated", 0.0))
	record["healing_done"] = float(record["healing_done"]) + float(unit.get("healing_done", 0.0))
	record["shielding_done"] = float(record["shielding_done"]) + float(unit.get("shielding_done", 0.0))
	record["auto_attacks_done"] = int(record["auto_attacks_done"]) + int(unit.get("auto_attacks_done", 0))
	record["abilities_used"] = int(record["abilities_used"]) + int(unit.get("abilities_used", 0))
	record["ultimates_used"] = int(record["ultimates_used"]) + int(unit.get("ultimates_used", 0))
	record["stuns_dealt"] = int(record["stuns_dealt"]) + int(unit.get("stuns_dealt", 0))


func _accumulate_outcome_fields(record: Dictionary, team: String, winner: String) -> void:
	if winner == "draw" or winner == "timeout":
		record["draws"] = int(record["draws"]) + 1
	elif winner == team:
		record["wins"] = int(record["wins"]) + 1
	else:
		record["losses"] = int(record["losses"]) + 1


func _match_team_totals_from_snapshots(units: Array[Dictionary]) -> Dictionary:
	var totals: Dictionary = {"__match__": {"damage_dealt": 0.0}}
	for unit in units:
		var team := _snapshot_team(unit)
		if not totals.has(team):
			totals[team] = {"damage_dealt": 0.0}
		var team_totals: Dictionary = totals[team]
		team_totals["damage_dealt"] = float(team_totals.get("damage_dealt", 0.0)) + _snapshot_damage_dealt(unit)
		totals[team] = team_totals
		var match_totals: Dictionary = totals["__match__"]
		match_totals["damage_dealt"] = float(match_totals.get("damage_dealt", 0.0)) + _snapshot_damage_dealt(unit)
		totals["__match__"] = match_totals
	return totals


func _sorted_aggregate_keys(aggregates: Dictionary) -> Array[String]:
	var keys: Array[String] = []
	for key in aggregates.keys():
		keys.append(String(key))
	keys.sort_custom(func(a: String, b: String) -> bool:
		var a_record: Dictionary = aggregates[a]
		var b_record: Dictionary = aggregates[b]
		if String(a_record.get("team", "")) == String(b_record.get("team", "")):
			return String(a_record.get("hero_id", "")) < String(b_record.get("hero_id", ""))
		return String(a_record.get("team", "")) < String(b_record.get("team", ""))
	)
	return keys


func _sorted_role_aggregate_keys(role_aggregates: Dictionary) -> Array[String]:
	var keys: Array[String] = []
	for key in role_aggregates.keys():
		keys.append(String(key))
	keys.sort_custom(func(a: String, b: String) -> bool:
		var a_record: Dictionary = role_aggregates[a]
		var b_record: Dictionary = role_aggregates[b]
		if String(a_record.get("team", "")) == String(b_record.get("team", "")):
			return String(a_record.get("role", "")) < String(b_record.get("role", ""))
		return String(a_record.get("team", "")) < String(b_record.get("team", ""))
	)
	return keys


func _sorted_passive_aggregate_keys(passive_aggregates: Dictionary) -> Array[String]:
	var keys: Array[String] = []
	for key in passive_aggregates.keys():
		keys.append(String(key))
	keys.sort_custom(func(a: String, b: String) -> bool:
		var a_record: Dictionary = passive_aggregates[a]
		var b_record: Dictionary = passive_aggregates[b]
		if String(a_record.get("team", "")) == String(b_record.get("team", "")):
			return String(a_record.get("passive_id", "")) < String(b_record.get("passive_id", ""))
		return String(a_record.get("team", "")) < String(b_record.get("team", ""))
	)
	return keys


func _sorted_team_aggregate_keys(team_aggregates: Dictionary) -> Array[String]:
	var keys: Array[String] = []
	for key in team_aggregates.keys():
		keys.append(String(key))
	keys.sort()
	return keys


func _normalized_aggregate_record(record: Dictionary) -> Dictionary:
	var normalized: Dictionary = record.duplicate(true)
	var matches: int = max(1, int(normalized.get("matches", 1)))
	normalized["damage_dealt_per_match"] = float(normalized.get("damage_dealt", 0.0)) / float(matches)
	normalized["damage_received_per_match"] = float(normalized.get("damage_received", 0.0)) / float(matches)
	normalized["damage_mitigated_per_match"] = float(normalized.get("damage_mitigated", 0.0)) / float(matches)
	normalized["healing_done_per_match"] = float(normalized.get("healing_done", 0.0)) / float(matches)
	normalized["shielding_done_per_match"] = float(normalized.get("shielding_done", 0.0)) / float(matches)
	normalized["kills_per_match"] = float(normalized.get("kills", 0)) / float(matches)
	normalized["deaths_per_match"] = float(normalized.get("deaths", 0)) / float(matches)
	normalized["assists_per_match"] = float(normalized.get("assists", 0)) / float(matches)
	normalized["auto_attacks_per_match"] = float(normalized.get("auto_attacks_done", 0)) / float(matches)
	normalized["abilities_used_per_match"] = float(normalized.get("abilities_used", 0)) / float(matches)
	normalized["ultimates_used_per_match"] = float(normalized.get("ultimates_used", 0)) / float(matches)
	normalized["stuns_dealt_per_match"] = float(normalized.get("stuns_dealt", 0)) / float(matches)
	var duration_total: float = float(normalized.get("duration_seconds_total", 0.0))
	normalized["duration_seconds_per_match"] = duration_total / float(matches)
	if duration_total > 0.0:
		normalized["damage_dealt_per_second"] = float(normalized.get("damage_dealt", 0.0)) / duration_total
		normalized["damage_received_per_second"] = float(normalized.get("damage_received", 0.0)) / duration_total
		normalized["damage_mitigated_per_second"] = float(normalized.get("damage_mitigated", 0.0)) / duration_total
		normalized["healing_done_per_second"] = float(normalized.get("healing_done", 0.0)) / duration_total
		normalized["shielding_done_per_second"] = float(normalized.get("shielding_done", 0.0)) / duration_total
	normalized["win_rate"] = float(normalized.get("wins", 0)) / float(matches)
	normalized["loss_rate"] = float(normalized.get("losses", 0)) / float(matches)
	normalized["draw_rate"] = float(normalized.get("draws", 0)) / float(matches)
	return normalized


func _on_combat_event_recorded(event: Dictionary, sink: Object) -> void:
	var record := event.duplicate(true)
	record["kind"] = "event"
	sink.write_record(record)


func _parse_roster(value: String) -> Array[String]:
	var roster: Array[String] = []
	for entry in value.split(",", false):
		var hero_id := entry.strip_edges()
		if not hero_id.is_empty():
			roster.append(hero_id)
	return roster


func _parse_bool(value: String) -> bool:
	var normalized := value.to_lower()
	return normalized == "1" or normalized == "true" or normalized == "yes" or normalized == "on"
