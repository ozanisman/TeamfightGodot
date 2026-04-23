class_name ChampionSpec
extends RefCounted

var stats
var description: String = ""
var ability_desc: String = ""
var ultimate_desc: String = ""
var passive_desc: String = ""
var ability = null
var ultimate = null
var passive_ids: Array[StringName] = []

func _init(
	_stats,
	_description: String = "",
	_ability_desc: String = "",
	_ultimate_desc: String = "",
	_passive_desc: String = "",
	_ability = null,
	_ultimate = null,
	_passive_ids: Array[StringName] = []
):
	stats = _stats
	description = _description
	ability_desc = _ability_desc
	ultimate_desc = _ultimate_desc
	passive_desc = _passive_desc
	ability = _ability
	ultimate = _ultimate
	passive_ids = _passive_ids.duplicate()

func to_dict() -> Dictionary:
	return {
		"unit_id": String(stats.unit_id),
		"name": String(stats.name),
		"role": String(stats.role),
		"stats": stats.to_dict(),
		"description": description,
		"ability_desc": ability_desc,
		"ultimate_desc": ultimate_desc,
		"passive_desc": passive_desc,
		"ability": null if ability == null else ability.to_dict(),
		"ultimate": null if ultimate == null else ultimate.to_dict(),
		"passive_ids": passive_ids.map(func(value): return String(value)),
	}
