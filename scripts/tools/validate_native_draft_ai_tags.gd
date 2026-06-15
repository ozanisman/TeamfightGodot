extends SceneTree

## Validate that archetype tags are available in the current native draft AI path
## Tests the actual methods used by draft_strategy_native.gd

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const ChampionCatalog := preload("res://scripts/simulation/champion_catalog.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

var _backend: RefCounted = null
var _catalog: RefCounted = null
var _stats_dir: String = "res://stats_output_100k"

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	print("=== NATIVE DRAFT AI TAG VALIDATION ===")
	print("Stats directory: " + _stats_dir)
	print("")
	
	_backend = NativeSimulationBackendScript.new()
	_catalog = ChampionCatalog.new()
	
	if not _backend.is_available():
		push_error("Native backend unavailable")
		await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
		return
	
	var report_lines = []
	report_lines.append("# Native Draft AI Tag Validation Report")
	report_lines.append("")
	report_lines.append("## Call Path")
	report_lines.append("- draft_strategy_native.gd")
	report_lines.append("- native_simulation_backend.gd")
	report_lines.append("- teamfight_simulation_core.cpp")
	report_lines.append("- sim_draft_ai_* path (current production)")
	report_lines.append("")
	
	# Load champion schema for tag validation
	var schema: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://fixtures/goldens/champion_schema.json"))
	var all_champions: Array = schema.keys()
	var champion_tags_map: Dictionary = {}
	
	for champion_id in all_champions:
		if champion_id == "passives" or champion_id == "role_configs" or champion_id == "minions":
			continue
		var champion_data: Dictionary = schema[champion_id]
		var tags: Array = champion_data.get("tags", [])
		champion_tags_map[champion_id] = tags
	
	report_lines.append("## Tag Validation")
	report_lines.append("- Total champions: " + str(champion_tags_map.size()))
	report_lines.append("")
	
	# Test pick recommendations
	report_lines.append("## Pick Recommendation Test")
	var available: Array = all_champions.duplicate()
	available.erase("passives")
	available.erase("role_configs")
	available.erase("minions")
	
	var allies: Array = ["swordsman", "archer"]
	var enemies: Array = ["berserker", "cleric"]
	
	var pick_recs: Array = _backend.get_draft_ai_pick_recommendations(_stats_dir, available, allies, enemies, 3, -1, 0)
	
	if pick_recs.is_empty():
		report_lines.append("ERROR: No pick recommendations returned")
	else:
		var top_pick: Dictionary = pick_recs[0]
		var candidate: String = top_pick.candidate
		var candidate_tags: Array = top_pick.get("candidate_tags", [])
		
		report_lines.append("Top pick: " + candidate)
		report_lines.append("candidate_tags: " + str(candidate_tags))
		
		# Validate tags match schema
		var expected_tags: Array = champion_tags_map.get(candidate, [])
		if candidate_tags == expected_tags:
			report_lines.append("Tags match schema: PASS")
		else:
			report_lines.append("Tags match schema: FAIL")
			report_lines.append("  Expected: " + str(expected_tags))
			report_lines.append("  Got: " + str(candidate_tags))
		
		# Check for missing tags
		var missing_tags: Array = []
		for tag in expected_tags:
			if not tag in candidate_tags:
				missing_tags.append(tag)
		
		if missing_tags.is_empty():
			report_lines.append("Missing tags: 0 - PASS")
		else:
			report_lines.append("Missing tags: " + str(missing_tags.size()) + " - FAIL")
			report_lines.append("  " + str(missing_tags))
		
		# Check for unknown tags
		var valid_tags: Array = ["frontline", "backline", "dive", "poke", "burst", "sustain", "cc", "control", "summon", "aoe", "single_target", "protect", "mobility", "needs_review"]
		var unknown_tags: Array = []
		for tag in candidate_tags:
			if not tag in valid_tags:
				unknown_tags.append(tag)
		
		if unknown_tags.is_empty():
			report_lines.append("Unknown tags: 0 - PASS")
		else:
			report_lines.append("Unknown tags: " + str(unknown_tags.size()) + " - FAIL")
			report_lines.append("  " + str(unknown_tags))
	
	report_lines.append("")
	
	# Test ban recommendations
	report_lines.append("## Ban Recommendation Test")
	var ban_recs: Array = _backend.get_draft_ai_ban_recommendations(_stats_dir, available, allies, enemies, 3, -1, "blue", {}, 0)
	
	if ban_recs.is_empty():
		report_lines.append("ERROR: No ban recommendations returned")
	else:
		var top_ban: Dictionary = ban_recs[0]
		var candidate: String = top_ban.candidate
		var candidate_tags: Array = top_ban.get("candidate_tags", [])
		
		report_lines.append("Top ban: " + candidate)
		report_lines.append("candidate_tags: " + str(candidate_tags))
		
		# Validate tags match schema
		var expected_tags: Array = champion_tags_map.get(candidate, [])
		if candidate_tags == expected_tags:
			report_lines.append("Tags match schema: PASS")
		else:
			report_lines.append("Tags match schema: FAIL")
			report_lines.append("  Expected: " + str(expected_tags))
			report_lines.append("  Got: " + str(candidate_tags))
		
		# Check for missing tags
		var missing_tags: Array = []
		for tag in expected_tags:
			if not tag in candidate_tags:
				missing_tags.append(tag)
		
		if missing_tags.is_empty():
			report_lines.append("Missing tags: 0 - PASS")
		else:
			report_lines.append("Missing tags: " + str(missing_tags.size()) + " - FAIL")
			report_lines.append("  " + str(missing_tags))
		
		# Check for unknown tags
		var valid_tags: Array = ["frontline", "backline", "dive", "poke", "burst", "sustain", "cc", "control", "summon", "aoe", "single_target", "protect", "mobility", "needs_review"]
		var unknown_tags: Array = []
		for tag in candidate_tags:
			if not tag in valid_tags:
				unknown_tags.append(tag)
		
		if unknown_tags.is_empty():
			report_lines.append("Unknown tags: 0 - PASS")
		else:
			report_lines.append("Unknown tags: " + str(unknown_tags.size()) + " - FAIL")
			report_lines.append("  " + str(unknown_tags))
	
	report_lines.append("")
	report_lines.append("## Archetype Strategy Debug Field Test")
	var archetype_pick_recs: Array = _backend.get_draft_ai_pick_recommendations(_stats_dir, available, allies, enemies, 3, -1, 4)
	if archetype_pick_recs.is_empty():
		report_lines.append("ERROR: No native_archetype pick recommendations returned")
	else:
		var top_archetype_pick: Dictionary = archetype_pick_recs[0]
		report_lines.append("Top native_archetype pick: " + String(top_archetype_pick.candidate))
		report_lines.append("candidate_tags: " + str(top_archetype_pick.get("candidate_tags", [])))
		report_lines.append("archetype_score: " + str(top_archetype_pick.get("archetype_score", null)))
		report_lines.append("archetype_weight: " + str(top_archetype_pick.get("archetype_weight", null)))
		report_lines.append("archetype_contribution: " + str(top_archetype_pick.get("archetype_contribution", null)))
		report_lines.append("archetype_reasons: " + str(top_archetype_pick.get("archetype_reasons", [])))
		var missing_pick_fields: Array = []
		for field in ["candidate_tags", "archetype_score", "archetype_weight", "archetype_contribution", "archetype_reasons"]:
			if not top_archetype_pick.has(field):
				missing_pick_fields.append(field)
		report_lines.append("Pick debug fields present: " + ("PASS" if missing_pick_fields.is_empty() else "FAIL " + str(missing_pick_fields)))

	var archetype_ban_recs: Array = _backend.get_draft_ai_ban_recommendations(_stats_dir, available, allies, enemies, 3, -1, "blue", {}, 4)
	if archetype_ban_recs.is_empty():
		report_lines.append("ERROR: No native_archetype ban recommendations returned")
	else:
		var top_archetype_ban: Dictionary = archetype_ban_recs[0]
		report_lines.append("Top native_archetype ban: " + String(top_archetype_ban.candidate))
		report_lines.append("candidate_tags: " + str(top_archetype_ban.get("candidate_tags", [])))
		report_lines.append("enemy_archetype_score: " + str(top_archetype_ban.get("enemy_archetype_score", null)))
		report_lines.append("enemy_archetype_weight: " + str(top_archetype_ban.get("enemy_archetype_weight", null)))
		report_lines.append("enemy_archetype_contribution: " + str(top_archetype_ban.get("enemy_archetype_contribution", null)))
		report_lines.append("archetype_reasons: " + str(top_archetype_ban.get("archetype_reasons", [])))
		var missing_ban_fields: Array = []
		for field in ["candidate_tags", "enemy_archetype_score", "enemy_archetype_weight", "enemy_archetype_contribution", "archetype_reasons"]:
			if not top_archetype_ban.has(field):
				missing_ban_fields.append(field)
		report_lines.append("Ban debug fields present: " + ("PASS" if missing_ban_fields.is_empty() else "FAIL " + str(missing_ban_fields)))

	report_lines.append("")
	
	# Print report
	for line in report_lines:
		print(line)
	
	# Write report to file
	var report_content: String = "\n".join(report_lines)
	var file: FileAccess = FileAccess.open("native_draft_ai_tag_path_report.md", FileAccess.WRITE)
	if file:
		file.store_string(report_content)
		file.close()
		print("")
		print("Report written to native_draft_ai_tag_path_report.md")
	
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)
