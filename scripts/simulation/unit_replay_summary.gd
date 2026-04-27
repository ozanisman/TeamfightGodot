class_name UnitReplaySummary
extends RefCounted

var instance_id: int = 0
var archetype_id: StringName = &""
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

func to_dict() -> Dictionary:
	return {
		"instance_id": instance_id,
		"archetype": String(archetype_id),
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
