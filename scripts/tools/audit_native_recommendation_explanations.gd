extends SceneTree

## Native Recommendation Explanation Audit
## Audits native recommendation debug output for completeness and validity
## Run from the project root:
##   C:\Godot\godot.exe --headless --path . --script res://scripts/tools/audit_native_recommendation_explanations.gd
## Writes res://logs/native_recommendation_explanations_audit_report.md

const ChampionCatalog := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

const STATS_DIR := "res://model_stats/stats_output_100k"
const STRATEGY_NATIVE := 0
const REPORT_PATH := "res://logs/native_recommendation_explanations_audit_report.md"

var _backend: RefCounted
var _missing_fields: Array[String] = []
var _suspicious_zero_fields: Array[String] = []
var _invalid_values: Array[String] = []
var _missing_explanation_inputs: Array[String] = []

# Required pick debug fields
const PICK_REQUIRED_FIELDS := [
	"candidate",
	"total_score",
	"base_power",
	"ally_synergy",
	"enemy_counter_value",
	"counter_risk",
	"role_fit",
	"comp_fit",
	"candidate_role",
	"phase_label",
]

# Required ban debug fields
const BAN_REQUIRED_FIELDS := [
	"candidate",
	"total_score",
	"enemy_pick_value",
	"own_pick_value",
	"denial_value",
	"enemy_synergy",
	"counters_my_team",
	"fills_enemy_role_need",
	"enemy_comp_fit",
	"early_ban_fallback_component",
	"phase_label",
]


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	_backend = NativeSimulationBackendScript.new()
	if _backend == null or not _backend.is_available():
		push_error("audit_native_recommendation_explanations: native backend unavailable")
		quit(1)
		return

	var states := _test_states()
	var lines: Array[String] = []
	lines.append("# Native Recommendation Explanation Audit")
	lines.append("")
	lines.append("Audits native recommendation debug output for completeness and validity. This report is diagnostic only; no scoring, weights, draft order, UI behavior, or stats files were changed.")
	lines.append("")
	lines.append("## Summary")
	lines.append("")
	lines.append("- Total states audited: %d" % states.size())
	lines.append("- Missing required fields: %d" % _missing_fields.size())
	lines.append("- Suspicious zero fields: %d" % _suspicious_zero_fields.size())
	lines.append("- Invalid values: %d" % _invalid_values.size())
	lines.append("- Missing explanation inputs: %d" % _missing_explanation_inputs.size())
	lines.append("")

	if not _missing_fields.is_empty():
		lines.append("### Missing Required Fields")
		lines.append("")
		for issue in _missing_fields:
			lines.append("- " + issue)
		lines.append("")

	if not _suspicious_zero_fields.is_empty():
		lines.append("### Suspicious Zero Fields")
		lines.append("")
		for issue in _suspicious_zero_fields:
			lines.append("- " + issue)
		lines.append("")

	if not _invalid_values.is_empty():
		lines.append("### Invalid Values")
		lines.append("")
		for issue in _invalid_values:
			lines.append("- " + issue)
		lines.append("")

	if not _missing_explanation_inputs.is_empty():
		lines.append("### Missing Explanation Inputs")
		lines.append("")
		for issue in _missing_explanation_inputs:
			lines.append("- " + issue)
		lines.append("")

	lines.append("## Detailed State Results")
	lines.append("")

	for state in states:
		var action := String(state["action"])
		var available := _available_for(state)
		var recommendations: Array

		if action == "PICK":
			var allies: Array = state.get("allies", [])
			var enemies: Array = state.get("enemies", [])
			var step := int(state["step"])
			recommendations = _backend.get_draft_ai_pick_recommendations(STATS_DIR, available, allies, enemies, 5, step, STRATEGY_NATIVE)
		else:
			var allies: Array = state.get("allies", [])
			var enemies: Array = state.get("enemies", [])
			var step := int(state["step"])
			var side := String(state["side"])
			recommendations = _backend.get_draft_ai_ban_recommendations(STATS_DIR, available, allies, enemies, 5, step, "blue" if side == "B" else "red", {}, STRATEGY_NATIVE)

		if recommendations.is_empty():
			lines.append("### State: %s (%s)" % [String(state["name"]), action])
			lines.append("No recommendations returned")
			lines.append("")
			continue

		var top_rec = recommendations[0] as Dictionary
		lines.append("### State: %s (%s)" % [String(state["name"]), action])
		lines.append("Top recommendation: %s" % String(top_rec.get("candidate", "unknown")))
		lines.append("")

		_audit_recommendation_fields(top_rec, action, String(state["name"]))
		lines.append("")

	_write_report(lines)
	print("audit_native_recommendation_explanations: wrote " + REPORT_PATH)
	quit(0)


func _test_states() -> Array[Dictionary]:
	return [
		{
			"name": "empty draft / first ban",
			"action": "BAN",
			"step": 0,
			"side": "B",
			"allies": [],
			"enemies": [],
			"bans": [],
		},
		{
			"name": "early ban after 1-2 bans",
			"action": "BAN",
			"step": 2,
			"side": "B",
			"allies": [],
			"enemies": [],
			"bans": ["wizard", "windcaller"],
		},
		{
			"name": "first pick",
			"action": "PICK",
			"step": 6,
			"side": "B",
			"allies": [],
			"enemies": [],
			"bans": ["wizard", "windcaller", "frost_mage", "necromancer", "warlock"],
		},
		{
			"name": "mid draft picks",
			"action": "PICK",
			"step": 10,
			"side": "B",
			"allies": ["colossus", "disarmer"],
			"enemies": ["berserker", "archer"],
			"bans": ["wizard", "windcaller", "frost_mage", "necromancer", "warlock"],
		},
		{
			"name": "phase-2 first ban",
			"action": "BAN",
			"step": 12,
			"side": "R",
			"allies": ["berserker", "archer", "swordsman"],
			"enemies": ["colossus", "disarmer", "cleric"],
			"bans": ["wizard", "windcaller", "frost_mage", "necromancer", "warlock"],
		},
		{
			"name": "late pick",
			"action": "PICK",
			"step": 18,
			"side": "B",
			"allies": ["colossus", "disarmer", "cleric", "monk"],
			"enemies": ["berserker", "archer", "swordsman", "wraith"],
			"bans": ["wizard", "windcaller", "frost_mage", "necromancer", "warlock", "guardian", "paladin", "valkyrie", "oracle"],
		},
		{
			"name": "near-complete draft",
			"action": "PICK",
			"step": 19,
			"side": "R",
			"allies": ["berserker", "archer", "swordsman", "wraith"],
			"enemies": ["colossus", "disarmer", "cleric", "monk", "rogue"],
			"bans": ["wizard", "windcaller", "frost_mage", "necromancer", "warlock", "guardian", "paladin", "valkyrie", "oracle"],
		},
	]


func _available_for(state: Dictionary) -> Array[StringName]:
	var available: Array[StringName] = ChampionCatalog.get_champion_ids()
	for group_key in ["allies", "enemies", "bans"]:
		for champion in state.get(group_key, []):
			available.erase(StringName(champion))
	return available


func _audit_recommendation_fields(rec: Dictionary, action: String, state_name: String) -> void:
	var required_fields := PICK_REQUIRED_FIELDS if action == "PICK" else BAN_REQUIRED_FIELDS

	for field in required_fields:
		if not rec.has(field):
			_missing_fields.append("`%s` (%s): missing field '%s'" % [state_name, action, field])
		else:
			var value = rec[field]

			# Check for suspicious zero values in fields that should be non-zero
			if field in ["total_score", "base_power", "ally_synergy", "enemy_counter_value", "denial_value", "enemy_pick_value", "own_pick_value"]:
				if value is float and is_zero_approx(float(value)):
					_suspicious_zero_fields.append("`%s` (%s): field '%s' is zero" % [state_name, action, field])

			# Check for invalid values
			if field == "candidate":
				if value == null or String(value).is_empty():
					_invalid_values.append("`%s` (%s): candidate is null or empty" % [state_name, action])

			# Check for missing explanation inputs (all zero values in explanation fields)
			if action == "PICK" and field in ["ally_synergy", "enemy_counter_value", "counter_risk", "role_fit", "comp_fit"]:
				if value is float and is_zero_approx(float(value)):
					_missing_explanation_inputs.append("`%s` (%s): field '%s' is zero (may indicate missing explanation input)" % [state_name, action, field])

			if action == "BAN" and field in ["enemy_pick_value", "own_pick_value", "denial_value", "enemy_synergy", "counters_my_team", "fills_enemy_role_need", "enemy_comp_fit"]:
				if value is float and is_zero_approx(float(value)):
					_missing_explanation_inputs.append("`%s` (%s): field '%s' is zero (may indicate missing explanation input)" % [state_name, action, field])


func _write_report(lines: Array[String]) -> void:
	var file := FileAccess.open(REPORT_PATH, FileAccess.WRITE)
	if file == null:
		push_error("audit_native_recommendation_explanations: failed to open report")
		return

	# Insert STATUS line after the first header line
	var report_content := ""
	for i in range(lines.size()):
		report_content += lines[i] + "\n"
		if i == 1:  # After the first empty line following the header
			var overall_pass := _missing_fields.is_empty() and _invalid_values.is_empty()
			report_content += "STATUS: " + ("PASS" if overall_pass else "FAIL") + "\n"

	file.store_string(report_content)
	file.close()
