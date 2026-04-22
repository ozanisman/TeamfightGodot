extends RefCounted
class_name SimulationContract

const CombatData = preload("res://scripts/combat_data.gd")

const BATCH_SCHEMA_VERSION: int = 1
const SIMULATION_SCHEMA_VERSION: int = 1


static func normalize_roster(roster: Array) -> Array[String]:
	var normalized: Array[String] = []
	for entry in roster:
		var hero_id := String(entry).strip_edges()
		if not hero_id.is_empty():
			normalized.append(hero_id)
	return normalized


static func derive_match_seed(base_seed: int, match_index: int) -> int:
	return base_seed + match_index * CombatData.SIMULATION_SEED_TEAM_OFFSET


static func build_mode_flags(record_events: bool, include_unit_summaries: bool, summary_only: bool, compute_only: bool, benchmark: bool) -> Dictionary:
	return {
		"record_events": record_events,
		"include_unit_summaries": include_unit_summaries,
		"summary_only": summary_only,
		"compute_only": compute_only,
		"benchmark": benchmark,
	}


static func build_batch_header(
		matches: int,
		base_seed: int,
		tick_rate: float,
		max_ticks: int,
		record_events: bool,
		include_unit_summaries: bool,
		summary_only: bool,
		compute_only: bool,
		chunk_records: int,
		player_roster: Array[String],
		enemy_roster: Array[String],
		output_path: String,
		benchmark: bool
	) -> Dictionary:
	return {
		"kind": "batch_header",
		"schema_version": BATCH_SCHEMA_VERSION,
		"simulation_schema_version": SIMULATION_SCHEMA_VERSION,
		"matches": matches,
		"seed": base_seed,
		"tick_rate": tick_rate,
		"max_ticks": max_ticks,
		"mode_flags": build_mode_flags(record_events, include_unit_summaries, summary_only, compute_only, benchmark),
		"record_events": record_events,
		"include_unit_summaries": include_unit_summaries,
		"summary_only": summary_only,
		"compute_only": compute_only,
		"chunk_records": chunk_records,
		"player_roster": player_roster.duplicate(),
		"enemy_roster": enemy_roster.duplicate(),
		"output_path": output_path,
	}


static func build_benchmark_summary(batch_result: Dictionary, elapsed_seconds: float, benchmark: bool) -> Dictionary:
	return {
		"kind": "benchmark_summary",
		"schema_version": BATCH_SCHEMA_VERSION,
		"simulation_schema_version": SIMULATION_SCHEMA_VERSION,
		"matches": int(batch_result.get("matches", 0)),
		"elapsed_seconds": elapsed_seconds,
		"matches_per_second": float(batch_result.get("matches_per_second", 0.0)),
		"mode_flags": batch_result.get("mode_flags", {}),
		"benchmark": benchmark,
	}


static func build_match_summary(termination: String, winner: String, player_kills: int, enemy_kills: int, time: float, ticks: int) -> Dictionary:
	return {
		"kind": "match_summary",
		"schema_version": BATCH_SCHEMA_VERSION,
		"termination": termination,
		"winner": winner,
		"player_kills": player_kills,
		"enemy_kills": enemy_kills,
		"time": time,
		"duration_seconds": time,
		"ticks": ticks,
	}


static func snapshot_unit(unit: Object) -> Dictionary:
	return {
		"instance_id": int(unit.get("instance_id")),
		"hero_id": String(unit.get("hero_id")),
		"display_name": String(unit.get("display_name")),
		"role": String(unit.get("role")),
		"team": String(unit.get("team")),
		"passive_id": String(unit.get("passive_id")),
		"alive": bool(unit.get("alive")),
		"hp": float(unit.get("hp")),
		"shield": float(unit.get("shield")),
		"position": unit.get("global_position"),
		"target_id": int(unit.get("target_id")),
		"in_range": bool(unit.get("in_range")),
		"kills": int(unit.get("kills")),
		"deaths": int(unit.get("deaths")),
		"assists": int(unit.get("assists")),
		"damage_dealt": float(unit.get("damage_dealt")),
		"damage_received": float(unit.get("damage_received")),
		"damage_mitigated": float(unit.get("damage_mitigated")),
		"healing_done": float(unit.get("healing_done")),
		"shielding_done": float(unit.get("shielding_done")),
		"auto_attacks_done": int(unit.get("auto_attacks_done")),
		"abilities_used": int(unit.get("abilities_used")),
		"ultimates_used": int(unit.get("ultimates_used")),
		"stuns_dealt": int(unit.get("stuns_dealt")),
	}
