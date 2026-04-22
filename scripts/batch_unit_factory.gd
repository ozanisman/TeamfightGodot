extends RefCounted
class_name BatchUnitFactory

const ChampionCatalog = preload("res://scripts/champion_catalog.gd")
const BatchUnitSpecScript = preload("res://scripts/batch_unit_spec.gd")


static func build_match_units(player_roster: Array[String], enemy_roster: Array[String]) -> Array:
	var units: Array = []
	units.append_array(_build_team_units(player_roster, "player"))
	units.append_array(_build_team_units(enemy_roster, "enemy"))
	return units


static func _build_team_units(hero_ids: Array[String], team: String) -> Array:
	var result: Array = []
	for index in range(hero_ids.size()):
		var hero_id := hero_ids[index]
		var hero := ChampionCatalog.get_by_id(hero_id)
		if hero.is_empty():
			continue
		result.append(BatchUnitSpecScript.from_hero(hero, team, index))
	return result
