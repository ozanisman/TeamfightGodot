class_name MatchReplaySummary
extends RefCounted
## End-state summary for a match (winner, duration, per-unit aggregates).
## Structured combat traces are not stored here; use [member MatchReplayInput.debug_combat_trace] and native [code]TeamfightSimulationCore.get_trace_events()[/code].

const UnitReplaySummaryScript := preload("res://scripts/simulation/unit_replay_summary.gd")

var seed: int = 0
var winner_team: StringName = &""
var duration: float = 0.0
var sudden_death_ticks: int = 0
var unit_stats: Array = []
var player_comp: Array[StringName] = []
var enemy_comp: Array[StringName] = []

func to_dict() -> Dictionary:
	return {
		"seed": seed,
		"winner_team": String(winner_team),
		"duration": duration,
		"sudden_death_ticks": sudden_death_ticks,
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
	summary.sudden_death_ticks = int(data.get("sudden_death_ticks", 0))
	for item in Array(data.get("unit_stats", [])):
		summary.unit_stats.append(UnitReplaySummaryScript.from_dict(Dictionary(item)))
	for comp in Array(data.get("player_comp", [])):
		summary.player_comp.append(StringName(String(comp)))
	for comp in Array(data.get("enemy_comp", [])):
		summary.enemy_comp.append(StringName(String(comp)))
	return summary
