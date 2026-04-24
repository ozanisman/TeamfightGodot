extends SceneTree

## Synthetic summaries → CSV → StatsDashboardLoader. Run: .\run_godot.ps1 -- --check-stats-aggregator

const StatsCsvAggregatorScript := preload("res://scripts/tools/stats_csv_aggregator.gd")
const StatsDashboardLoaderScript := preload("res://scripts/tools/stats_dashboard_loader.gd")

const _TEST_DIR := "user://stats_agg_roundtrip_test"


func _init() -> void:
	call_deferred("_run")


func _unit(
	archetype: String,
	team: String,
	won: bool,
	dmg: float = 10.0
) -> Dictionary:
	return {
		"archetype": archetype,
		"team": team,
		"won": won,
		"damage_dealt": dmg,
		"damage_dealt_auto": dmg * 0.4,
		"damage_dealt_ability": dmg * 0.35,
		"damage_dealt_ultimate": dmg * 0.25,
		"damage_received": 8.0,
		"damage_mitigated": 1.0,
		"healing_done": 0.5,
		"shielding_done": 0.25,
		"stuns": 1,
		"kills": 1,
		"deaths": 1,
		"assists": 1,
	}


func _run() -> void:
	var agg := StatsCsvAggregatorScript.new()
	agg.reset()
	agg.consume_summary(
		1,
		{
			"winner_team": "player",
			"player_comp": ["archer"],
			"enemy_comp": ["swordsman"],
			"unit_stats": [_unit("archer", "player", true, 20.0), _unit("swordsman", "enemy", false, 15.0)],
		}
	)
	agg.consume_summary(
		1,
		{
			"winner_team": "enemy",
			"player_comp": ["archer"],
			"enemy_comp": ["swordsman"],
			"unit_stats": [_unit("archer", "player", false, 12.0), _unit("swordsman", "enemy", true, 18.0)],
		}
	)
	agg.consume_summary(
		2,
		{
			"winner_team": "player",
			"player_comp": ["archer", "guardian"],
			"enemy_comp": ["swordsman", "mage"],
			"unit_stats": [
				_unit("archer", "player", true),
				_unit("guardian", "player", true),
				_unit("swordsman", "enemy", false),
				_unit("mage", "enemy", false),
			],
		}
	)
	var abs_base := ProjectSettings.globalize_path(_TEST_DIR)
	if DirAccess.dir_exists_absolute(abs_base):
		_delete_tree(abs_base)
	var werr: Error = agg.write_to_dir(_TEST_DIR)
	if werr != OK:
		push_error("check_stats_aggregator_roundtrip: write failed %s" % error_string(werr))
		quit(1)
		return
	var loader := StatsDashboardLoaderScript.new()
	var lerr: Error = loader.load_from_dir(_TEST_DIR)
	if lerr != OK:
		push_error("check_stats_aggregator_roundtrip: load failed %s" % error_string(lerr))
		quit(1)
		return
	if not loader.summary_stats.has(1) or not loader.summary_stats.has(2):
		push_error("check_stats_aggregator_roundtrip: missing summary keys")
		quit(1)
		return
	if not loader.all_stats[1]["units"].has("archer"):
		push_error("check_stats_aggregator_roundtrip: missing hero aggregate")
		quit(1)
		return
	quit(0)


func _delete_tree(abs_path: String) -> void:
	var da := DirAccess.open(abs_path)
	if da == null:
		return
	da.list_dir_begin()
	var name := da.get_next()
	while name != "":
		if name != "." and name != "..":
			var full := "%s/%s" % [abs_path.rstrip("/"), name]
			if da.current_is_dir():
				_delete_tree(full)
			else:
				DirAccess.remove_absolute(full)
		name = da.get_next()
	da.list_dir_end()
	DirAccess.remove_absolute(abs_path)
