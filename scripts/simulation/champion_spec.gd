class_name ChampionSpec
extends RefCounted

var stats
var description: String = ""
var ability_desc: String = ""
var ultimate_desc: String = ""
var passive_desc: String = ""
var passive_name: String = ""
var ability_name: String = ""
var ultimate_name: String = ""
var ability = null
var ultimate = null
var passive_ids: Array[StringName] = []

func _init(
	_stats,
	_description: String = "",
	_ability_desc: String = "",
	_ultimate_desc: String = "",
	_passive_desc: String = "",
	_passive_name: String = "",
	_ability_name: String = "",
	_ultimate_name: String = "",
	_ability = null,
	_ultimate = null,
	_passive_ids: Array[StringName] = []
):
	stats = _stats
	description = _description
	ability_desc = _ability_desc
	ultimate_desc = _ultimate_desc
	passive_desc = _passive_desc
	passive_name = _passive_name
	ability_name = _ability_name
	ultimate_name = _ultimate_name
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
		"passive_name": passive_name,
		"ability_name": ability_name,
		"ultimate_name": ultimate_name,
		"ability": null if ability == null else ability.to_dict(),
		"ultimate": null if ultimate == null else ultimate.to_dict(),
		"passive_ids": passive_ids.map(func(value): return String(value)),
	}
