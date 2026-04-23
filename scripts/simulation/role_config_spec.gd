class_name RoleConfigSpec
extends RefCounted

var stat_mods: Dictionary = {}
var passive_on_tick = null
var passive_post_take_damage = null

func _init(_stat_mods: Dictionary = {}, _passive_on_tick = null, _passive_post_take_damage = null):
	stat_mods = _stat_mods.duplicate(true)
	passive_on_tick = _passive_on_tick
	passive_post_take_damage = _passive_post_take_damage

func to_dict() -> Dictionary:
	return {
		"stat_mods": stat_mods.duplicate(true),
		"passive_on_tick": null if passive_on_tick == null else passive_on_tick.to_dict(),
		"passive_post_take_damage": null if passive_post_take_damage == null else passive_post_take_damage.to_dict(),
	}
