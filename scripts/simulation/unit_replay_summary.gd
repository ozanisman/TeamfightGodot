class_name UnitReplaySummary
extends RefCounted

var instance_id: int = 0
var unit_id: StringName = &""
var team: StringName = &""
var won: bool = false
var damage_dealt: float = 0.0
var damage_dealt_auto: float = 0.0
var damage_dealt_ability: float = 0.0
var damage_dealt_ultimate: float = 0.0
var damage_dealt_passive: float = 0.0
var damage_received: float = 0.0
var damage_mitigated: float = 0.0
var healing_done: float = 0.0
var healing_done_auto: float = 0.0
var healing_done_ability: float = 0.0
var healing_done_ultimate: float = 0.0
var healing_done_passive: float = 0.0
var shielding_done: float = 0.0
var shielding_done_auto: float = 0.0
var shielding_done_ability: float = 0.0
var shielding_done_ultimate: float = 0.0
var shielding_done_passive: float = 0.0
var auto_attacks: int = 0
var abilities: int = 0
var ultimates: int = 0
var stuns: int = 0
var kills: int = 0
var deaths: int = 0
var assists: int = 0
## Optional native-produced bag; keys documented in [SimulationSchema.export_contract_schema].
var telemetry: Dictionary = {}

static func from_dict(unit_data: Dictionary) -> UnitReplaySummary:
	var unit_summary := new()
	unit_summary.instance_id = int(unit_data.get("instance_id", 0))
	var archetype_value = unit_data.get("archetype", unit_data.get("unit_id", ""))
	unit_summary.unit_id = StringName(String(archetype_value))
	unit_summary.team = StringName(String(unit_data.get("team", "")))
	unit_summary.won = bool(unit_data.get("won", false))
	unit_summary.damage_dealt = float(unit_data.get("damage_dealt", 0.0))
	unit_summary.damage_dealt_auto = float(unit_data.get("damage_dealt_auto", 0.0))
	unit_summary.damage_dealt_ability = float(unit_data.get("damage_dealt_ability", 0.0))
	unit_summary.damage_dealt_ultimate = float(unit_data.get("damage_dealt_ultimate", 0.0))
	unit_summary.damage_dealt_passive = float(unit_data.get("damage_dealt_passive", 0.0))
	unit_summary.damage_received = float(unit_data.get("damage_received", 0.0))
	unit_summary.damage_mitigated = float(unit_data.get("damage_mitigated", 0.0))
	unit_summary.healing_done = float(unit_data.get("healing_done", 0.0))
	unit_summary.healing_done_auto = float(unit_data.get("healing_done_auto", 0.0))
	unit_summary.healing_done_ability = float(unit_data.get("healing_done_ability", 0.0))
	unit_summary.healing_done_ultimate = float(unit_data.get("healing_done_ultimate", 0.0))
	unit_summary.healing_done_passive = float(unit_data.get("healing_done_passive", 0.0))
	unit_summary.shielding_done = float(unit_data.get("shielding_done", 0.0))
	unit_summary.shielding_done_auto = float(unit_data.get("shielding_done_auto", 0.0))
	unit_summary.shielding_done_ability = float(unit_data.get("shielding_done_ability", 0.0))
	unit_summary.shielding_done_ultimate = float(unit_data.get("shielding_done_ultimate", 0.0))
	unit_summary.shielding_done_passive = float(unit_data.get("shielding_done_passive", 0.0))
	unit_summary.auto_attacks = int(unit_data.get("auto_attacks", 0))
	unit_summary.abilities = int(unit_data.get("abilities", 0))
	unit_summary.ultimates = int(unit_data.get("ultimates", 0))
	unit_summary.stuns = int(unit_data.get("stuns", 0))
	unit_summary.kills = int(unit_data.get("kills", 0))
	unit_summary.deaths = int(unit_data.get("deaths", 0))
	unit_summary.assists = int(unit_data.get("assists", 0))
	var tel_raw: Variant = unit_data.get("telemetry", {})
	if tel_raw is Dictionary:
		unit_summary.telemetry = Dictionary(tel_raw).duplicate(true)
	else:
		unit_summary.telemetry = {}
	return unit_summary

func to_dict() -> Dictionary:
	var result := {
		"instance_id": instance_id,
		"archetype": String(unit_id),
		"team": String(team),
		"won": won,
		"damage_dealt": damage_dealt,
		"damage_dealt_auto": damage_dealt_auto,
		"damage_dealt_ability": damage_dealt_ability,
		"damage_dealt_ultimate": damage_dealt_ultimate,
		"damage_dealt_passive": damage_dealt_passive,
		"damage_received": damage_received,
		"damage_mitigated": damage_mitigated,
		"healing_done": healing_done,
		"healing_done_auto": healing_done_auto,
		"healing_done_ability": healing_done_ability,
		"healing_done_ultimate": healing_done_ultimate,
		"healing_done_passive": healing_done_passive,
		"shielding_done": shielding_done,
		"shielding_done_auto": shielding_done_auto,
		"shielding_done_ability": shielding_done_ability,
		"shielding_done_ultimate": shielding_done_ultimate,
		"shielding_done_passive": shielding_done_passive,
		"auto_attacks": auto_attacks,
		"abilities": abilities,
		"ultimates": ultimates,
		"stuns": stuns,
		"kills": kills,
		"deaths": deaths,
		"assists": assists,
	}
	if not telemetry.is_empty():
		result["telemetry"] = telemetry.duplicate(true)
	return result
