class_name SpawnSpec
extends RefCounted

var archetype_id: StringName = &""
var team: StringName = &""
var x: float = 0.0
var y: float = 0.0

func _init(_archetype_id: StringName = &"", _team: StringName = &"", _x: float = 0.0, _y: float = 0.0) -> void:
	archetype_id = _archetype_id
	team = _team
	x = _x
	y = _y

func to_dict() -> Dictionary:
	return {
		"archetype_id": String(archetype_id),
		"team": String(team),
		"x": x,
		"y": y,
	}

static func from_dict(data: Dictionary):
	var spawn := new()
	spawn.archetype_id = StringName(String(data.get("archetype_id", "")))
	spawn.team = StringName(String(data.get("team", "")))
	spawn.x = float(data.get("x", 0.0))
	spawn.y = float(data.get("y", 0.0))
	return spawn
