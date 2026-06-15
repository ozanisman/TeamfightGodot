extends SceneTree

## Native Ban Quality Audit
## Audits native ban recommendations for quality and suspicious patterns
## Run from the project root:
##   C:\Godot\godot.exe --headless --path . --script res://scripts/tools/audit_native_ban_quality.gd
## Writes res://logs/native_ban_quality_audit_report.md

const ChampionCatalog := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

const STATS_DIR := "res://stats_output_100k"
const STRATEGY_NATIVE := 0
const REPORT_PATH := "res://logs/native_ban_quality_audit_report.md"

var _backend: RefCounted
var _suspicious_notes: Array[String] = []
var _invalid_data: Array[String] = []


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	print("audit_native_ban_quality: starting")
	_backend = NativeSimulationBackendScript.new()
	if _backend == null or not _backend.is_available():
		push_error("audit_native_ban_quality: native backend unavailable")
		quit(1)
		return

	print("audit_native_ban_quality: backend available")
	var states := _test_states()
	print("audit_native_ban_quality: " + str(states.size()) + " states to audit")
	var lines: Array[String] = []
	lines.append("# Native Ban Quality Audit")
	lines.append("")
	lines.append("Audits native ban recommendations for quality and suspicious patterns. This report is diagnostic only; no scoring, weights, draft order, champion tags, UI behavior, or stats files were changed.")
	lines.append("")
	lines.append("## Summary")
	lines.append("")
	lines.append("- Total states audited: %d" % states.size())
	lines.append("- Suspicious notes: %d" % _suspicious_notes.size())
	lines.append("- Invalid data: %d" % _invalid_data.size())
	lines.append("")

	if not _suspicious_notes.is_empty():
		lines.append("### Suspicious Ban Patterns")
		lines.append("")
		for note in _suspicious_notes:
			lines.append("- " + note)
		lines.append("")

	if not _invalid_data.is_empty():
		lines.append("### Invalid Data")
		lines.append("")
		for issue in _invalid_data:
			lines.append("- " + issue)
		lines.append("")

	lines.append("## Detailed State Results")
	lines.append("")

	for state in states:
		print("audit_native_ban_quality: processing " + String(state["name"]))
		var available := _available_for(state)
		var allies: Array = state.get("allies", [])
		var enemies: Array = state.get("enemies", [])
		var step := int(state["step"])
		var side := String(state["side"])

		var recommendations = _backend.get_draft_ai_ban_recommendations(STATS_DIR, available, allies, enemies, 5, step, "blue" if side == "B" else "red", {}, STRATEGY_NATIVE)

		if recommendations.is_empty():
			lines.append("### State: %s" % String(state["name"]))
			lines.append("No recommendations returned")
			lines.append("")
			continue

		lines.append("### State: %s" % String(state["name"]))
		lines.append("")
		lines.append("| Rank | Candidate | Total Score | Enemy Pick | Own Pick | Denial | Enemy Synergy | Counters Us | Fills Enemy Role | Enemy Comp Fit | Early Fallback | Tags |")
		lines.append("|------|-----------|-------------|------------|---------|--------|---------------|-------------|------------------|----------------|---------------|------|")

		for i in range(min(5, recommendations.size())):
			var rec = recommendations[i] as Dictionary
			var candidate := String(rec.get("candidate", "unknown"))
			var total_score := float(rec.get("total_score", 0.0))
			var enemy_pick_value := float(rec.get("enemy_pick_value", 0.0))
			var own_pick_value := float(rec.get("own_pick_value", 0.0))
			var denial_value := float(rec.get("denial_value", 0.0))
			var enemy_synergy := float(rec.get("enemy_synergy", 0.0))
			var counters_my_team := float(rec.get("counters_my_team", 0.0))
			var fills_enemy_role_need := float(rec.get("fills_enemy_role_need", 0.0))
			var enemy_comp_fit := float(rec.get("enemy_comp_fit", 0.0))
			var early_ban_fallback := float(rec.get("early_ban_fallback_component", 0.0))
			var tags: Array = rec.get("candidate_tags", [])

			# Calculate report-only diagnostics
			var self_denial_risk := maxf(0.0, own_pick_value)
			var enemy_value := maxf(0.0, enemy_pick_value)
			var denial_ratio := denial_value / maxf(absf(total_score), 0.0001)

			lines.append("| %d | %s | %.4f | %.4f | %.4f | %.4f | %.4f | %.4f | %.4f | %.4f | %.4f | %s |" % [
				i + 1,
				candidate,
				total_score,
				enemy_pick_value,
				own_pick_value,
				denial_value,
				enemy_synergy,
				counters_my_team,
				fills_enemy_role_need,
				enemy_comp_fit,
				early_ban_fallback,
				str(tags)
			])

			# Check for suspicious patterns
			_audit_ban_quality(rec, String(state["name"]), i, denial_ratio)

		lines.append("")

	print("audit_native_ban_quality: writing report")
	_write_report(lines)
	print("audit_native_ban_quality: done")
	OS.delay_msec(1000)
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
			"name": "phase 1 final ban",
			"action": "BAN",
			"step": 5,
			"side": "R",
			"allies": [],
			"enemies": [],
			"bans": ["wizard", "windcaller", "frost_mage", "necromancer", "warlock"],
		},
		{
			"name": "phase 2 first ban",
			"action": "BAN",
			"step": 12,
			"side": "R",
			"allies": ["berserker", "archer", "swordsman"],
			"enemies": ["colossus", "disarmer", "cleric"],
			"bans": ["wizard", "windcaller", "frost_mage", "necromancer", "warlock"],
		},
		{
			"name": "phase 2 final ban",
			"action": "BAN",
			"step": 15,
			"side": "B",
			"allies": ["colossus", "disarmer", "cleric", "monk"],
			"enemies": ["berserker", "archer", "swordsman"],
			"bans": ["wizard", "windcaller", "frost_mage", "necromancer", "warlock", "guardian", "paladin", "valkyrie"],
		},
		{
			"name": "enemy frontline shell",
			"action": "BAN",
			"step": 2,
			"side": "B",
			"allies": [],
			"enemies": ["colossus", "guardian", "paladin"],
			"bans": ["wizard"],
		},
		{
			"name": "enemy backline/poke shell",
			"action": "BAN",
			"step": 2,
			"side": "B",
			"allies": [],
			"enemies": ["archer", "sniper", "wizard"],
			"bans": ["colossus"],
		},
		{
			"name": "enemy aoe/control shell",
			"action": "BAN",
			"step": 2,
			"side": "B",
			"allies": [],
			"enemies": ["frost_mage", "earthbender", "windcaller"],
			"bans": ["colossus"],
		},
		{
			"name": "ally team vulnerable to counter",
			"action": "BAN",
			"step": 2,
			"side": "B",
			"allies": ["cleric", "oracle", "siren"],
			"enemies": ["berserker", "ninja", "wraith"],
			"bans": ["colossus"],
		},
	]


func _available_for(state: Dictionary) -> Array[StringName]:
	var available: Array[StringName] = ChampionCatalog.get_champion_ids()
	for group_key in ["allies", "enemies", "bans"]:
		for champion in state.get(group_key, []):
			available.erase(StringName(champion))
	return available


func _audit_ban_quality(rec: Dictionary, state_name: String, rank: int, denial_ratio: float) -> void:
	var candidate := String(rec.get("candidate", "unknown"))
	var total_score := float(rec.get("total_score", 0.0))
	var enemy_pick_value := float(rec.get("enemy_pick_value", 0.0))
	var own_pick_value := float(rec.get("own_pick_value", 0.0))
	var denial_value := float(rec.get("denial_value", 0.0))
	var enemy_synergy := float(rec.get("enemy_synergy", 0.0))
	var counters_my_team := float(rec.get("counters_my_team", 0.0))
	var fills_enemy_role_need := float(rec.get("fills_enemy_role_need", 0.0))
	var enemy_comp_fit := float(rec.get("enemy_comp_fit", 0.0))
	var early_ban_fallback := float(rec.get("early_ban_fallback_component", 0.0))

	# Check for invalid data
	if candidate == "unknown" or candidate.is_empty():
		_invalid_data.append("`%s` rank %d: candidate is invalid" % [state_name, rank + 1])

	# Suspicious: own_pick_value high and enemy_pick_value low
	if own_pick_value > 0.05 and enemy_pick_value < 0.01:
		_suspicious_notes.append("`%s` rank %d (%s): own_pick_value (%.4f) high but enemy_pick_value (%.4f) low - candidate may be better for us than enemy" % [state_name, rank + 1, candidate, own_pick_value, enemy_pick_value])

	# Suspicious: denial_value negative or near zero but top ranked
	if rank == 0 and denial_value < 0.001:
		_suspicious_notes.append("`%s` top rank (%s): denial_value (%.4f) near zero but top ranked" % [state_name, candidate, denial_value])

	# Suspicious: candidate appears better for us than enemy
	if own_pick_value > enemy_pick_value * 2 and enemy_pick_value > 0.01:
		_suspicious_notes.append("`%s` rank %d (%s): own_pick_value (%.4f) much higher than enemy_pick_value (%.4f) - candidate may be better for us" % [state_name, rank + 1, candidate, own_pick_value, enemy_pick_value])

	# Suspicious: ban mostly driven by tiny role/comp terms
	var role_comp_sum := fills_enemy_role_need + enemy_comp_fit
	var other_terms := enemy_pick_value + own_pick_value + denial_value + enemy_synergy + counters_my_team + early_ban_fallback
	if role_comp_sum > 0.01 and other_terms < 0.01:
		_suspicious_notes.append("`%s` rank %d (%s): ban mostly driven by role/comp terms (%.4f) with tiny other terms (%.4f)" % [state_name, rank + 1, candidate, role_comp_sum, other_terms])


func _write_report(lines: Array[String]) -> void:
	var file := FileAccess.open(REPORT_PATH, FileAccess.WRITE)
	if file == null:
		push_error("audit_native_ban_quality: failed to open report at " + REPORT_PATH)
		return

	print("audit_native_ban_quality: writing " + str(lines.size()) + " lines")
	# Insert STATUS line after the first header line
	var report_content := ""
	for i in range(lines.size()):
		report_content += lines[i] + "\n"
		if i == 1:  # After the first empty line following the header
			var overall_pass := _invalid_data.is_empty()
			report_content += "STATUS: " + ("PASS" if overall_pass else "FAIL") + "\n"

	file.store_string(report_content)
	file.close()
	print("audit_native_ban_quality: report written successfully")
