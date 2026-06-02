class_name SpawnSpec
extends RefCounted

var unit_id: StringName = &""
var team: StringName = &""
var x: float = 0.0
var y: float = 0.0

func _init(_unit_id: StringName = &"", _team: StringName = &"", _x: float = 0.0, _y: float = 0.0) -> void:
	unit_id = _unit_id
	team = _team
	x = _x
	y = _y

func to_dict() -> Dictionary:
	return {
		"unit_id": String(unit_id),
		"team": String(team),
		"x": x,
		"y": y,
	}

static func from_dict(data: Dictionary):
	var spawn := new()
	spawn.unit_id = StringName(String(data.get("unit_id", "")))
	spawn.team = StringName(String(data.get("team", "")))
	spawn.x = float(data.get("x", 0.0))
	spawn.y = float(data.get("y", 0.0))
	return spawn
