class_name MatchReplaySummary
extends RefCounted

const UnitReplaySummaryScript := preload("res://scripts/simulation/unit_replay_summary.gd")

var seed: int = 0
var winner_team: StringName = &""
var duration: float = 0.0
var unit_stats: Array = []
var player_comp: Array[StringName] = []
var enemy_comp: Array[StringName] = []

func to_dict() -> Dictionary:
	return {
		"seed": seed,
		"winner_team": String(winner_team),
		"duration": duration,
		"unit_stats": unit_stats.map(func(value): return value.to_dict()),
		"player_comp": player_comp.map(func(value): return String(value)),
		"enemy_comp": enemy_comp.map(func(value): return String(value)),
	}

static func from_dict(data: Dictionary):
	var summary := new()
	summary.seed = int(data.get("seed", 0))
	var winner_value = data.get("winner_team", null)
	summary.winner_team = &"draw" if winner_value == null else StringName(String(winner_value))
	summary.duration = float(data.get("duration", 0.0))
	for item in Array(data.get("unit_stats", [])):
		var unit_summary = UnitReplaySummaryScript.new()
		var unit_data := Dictionary(item)
		unit_summary.instance_id = int(unit_data.get("instance_id", 0))
		var archetype_value = unit_data.get("archetype", unit_data.get("archetype_id", ""))
		unit_summary.archetype_id = StringName(String(archetype_value))
		unit_summary.team = StringName(String(unit_data.get("team", "")))
		unit_summary.won = bool(unit_data.get("won", false))
		unit_summary.damage_dealt = float(unit_data.get("damage_dealt", 0.0))
		unit_summary.damage_dealt_auto = float(unit_data.get("damage_dealt_auto", 0.0))
		unit_summary.damage_dealt_ability = float(unit_data.get("damage_dealt_ability", 0.0))
		unit_summary.damage_dealt_ultimate = float(unit_data.get("damage_dealt_ultimate", 0.0))
		unit_summary.damage_received = float(unit_data.get("damage_received", 0.0))
		unit_summary.damage_mitigated = float(unit_data.get("damage_mitigated", 0.0))
		unit_summary.healing_done = float(unit_data.get("healing_done", 0.0))
		unit_summary.shielding_done = float(unit_data.get("shielding_done", 0.0))
		unit_summary.auto_attacks = int(unit_data.get("auto_attacks", 0))
		unit_summary.abilities = int(unit_data.get("abilities", 0))
		unit_summary.ultimates = int(unit_data.get("ultimates", 0))
		unit_summary.stuns = int(unit_data.get("stuns", 0))
		unit_summary.kills = int(unit_data.get("kills", 0))
		unit_summary.deaths = int(unit_data.get("deaths", 0))
		unit_summary.assists = int(unit_data.get("assists", 0))
		summary.unit_stats.append(unit_summary)
	for comp in Array(data.get("player_comp", [])):
		summary.player_comp.append(StringName(String(comp)))
	for comp in Array(data.get("enemy_comp", [])):
		summary.enemy_comp.append(StringName(String(comp)))
	return summary
