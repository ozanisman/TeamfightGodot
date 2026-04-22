extends Node

const BatchRunnerScript = preload("res://scripts/batch_runner.gd")
const CombatData = preload("res://scripts/combat_data.gd")

var failures: Array[String] = []


func _ready() -> void:
	var code := _run_smoke()
	for entry in failures:
		push_error(entry)
	if failures.is_empty():
		print("BATCH SMOKE OK")
	else:
		print("BATCH SMOKE FAIL: %d issue(s)" % failures.size())
	get_tree().quit(code)


func _run_smoke() -> int:
	var runner := BatchRunnerScript.new()
	var config := {
		"matches": 1,
		"seed": 1234,
		"tick_rate": CombatData.SIMULATION_TICK_RATE,
		"max_ticks": CombatData.DEFAULT_MAX_SIM_TICKS,
		"record_events": false,
		"include_unit_summaries": false,
		"player_roster": ["swordsman", "archer", "oracle", "mage", "guardian"],
		"enemy_roster": ["assassin", "sniper", "warlock", "rogue", "colossus"],
	}

	var project_root := ProjectSettings.globalize_path("res://")
	var output_path := "%s/batch_smoke.jsonl" % project_root
	_remove_if_exists(output_path)

	config["output_path"] = output_path
	var result_a: Dictionary = runner.run_batch(config)
	_assert_true(int(result_a.get("exit_code", 1)) == 0, "First batch run failed")
	_assert_true(FileAccess.file_exists(output_path), "First batch stats file missing")
	var text_a := FileAccess.get_file_as_string(output_path)
	_assert_true(not text_a.is_empty(), "First batch stats file was empty")
	_assert_true(text_a.find("\"kind\":\"match_summary\"") >= 0, "First batch stats file lacked match summaries")
	var header_a := _parse_first_line(output_path)
	_assert_true(String(header_a.get("kind", "")) == "batch_header", "Batch header missing")
	_assert_true(int(header_a.get("simulation_schema_version", 0)) > 0, "Batch header lacked simulation schema version")
	_assert_true(header_a.has("mode_flags"), "Batch header lacked mode flags")

	var result_b: Dictionary = runner.run_batch(config)
	_assert_true(int(result_b.get("exit_code", 1)) == 0, "Second batch run failed")
	_assert_true(FileAccess.file_exists(output_path), "Second batch stats file missing")
	var text_b := FileAccess.get_file_as_string(output_path)
	_assert_true(text_a == text_b, "Batch output was not deterministic for a fixed seed")

	_remove_if_exists(output_path)
	runner.free()
	return 0 if failures.is_empty() else 1


func _assert_true(condition: bool, label: String) -> void:
	if not condition:
		failures.append(label)


func _remove_if_exists(path: String) -> void:
	var absolute_path := ProjectSettings.globalize_path(path)
	if FileAccess.file_exists(path):
		DirAccess.remove_absolute(absolute_path)


func _parse_first_line(path: String) -> Dictionary:
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null or file.eof_reached():
		return {}
	var line := file.get_line()
	var parsed: Variant = JSON.parse_string(line)
	if parsed is Dictionary:
		return parsed as Dictionary
	return {}
