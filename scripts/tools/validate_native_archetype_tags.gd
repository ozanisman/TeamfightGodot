extends SceneTree

const SimulationSchemaScript := preload("res://scripts/simulation/simulation_schema.gd")

# Approved tag list
const APPROVED_TAGS = [
	"frontline",
	"backline",
	"dive",
	"poke",
	"burst",
	"sustain",
	"cc",
	"control",
	"summon",
	"aoe",
	"single_target",
	"protect",
	"mobility",
	"needs_review"
]

func _init():
	var report := validate_native_tags()
	print_report(report)
	write_report_to_file(report)
	quit()

func validate_native_tags() -> Dictionary:
	var report := {
		"champion_count": 0,
		"champions_with_tags": 0,
		"champions_missing_tags": [],
		"unknown_tags": [],
		"needs_review_champions": [],
		"tag_distribution": {},
		"champion_tags": {}
	}
	
	# Initialize tag distribution
	for tag in APPROVED_TAGS:
		report["tag_distribution"][tag] = 0
	
	# Load champion schema from JSON (native path)
	var json_string := FileAccess.get_file_as_string("res://fixtures/goldens/champion_schema.json")
	if json_string.is_empty():
		push_error("Failed to read champion_schema.json")
		return report
	
	var json := JSON.new()
	var parse_result := json.parse(json_string)
	if parse_result != OK:
		push_error("Failed to parse champion_schema.json: %s" % json.get_error_message())
		return report
	
	var schema: Dictionary = json.data
	
	# Count champions and validate tags
	var champion_keys: Array = schema.keys()
	for key in champion_keys:
		var key_str = String(key)
		
		# Skip non-champion entries
		if key_str == "passives" or key_str == "role_configs" or key_str == "minions":
			continue
		
		report["champion_count"] += 1
		
		var champion = schema[key]
		var tags = champion.get("tags", [])
		
		# Store champion tags
		report["champion_tags"][key_str] = tags
		
		# Check if champion has tags
		if tags is Array and not tags.is_empty():
			report["champions_with_tags"] += 1
		else:
			report["champions_missing_tags"].append(key_str)
			continue
		
		# Validate tags and count distribution
		for tag in tags:
			var tag_str = String(tag)
			if not APPROVED_TAGS.has(tag_str):
				report["unknown_tags"].append({
					"champion": key_str,
					"tag": tag_str
				})
			else:
				report["tag_distribution"][tag_str] += 1
		
		# Check for needs_review
		if tags.has("needs_review"):
			report["needs_review_champions"].append(key_str)
	
	return report

func print_report(report: Dictionary) -> void:
	print("============================================================")
	print("NATIVE ARCHETYPE TAG LOADING VALIDATION REPORT")
	print("============================================================")
	print("")
	print("Champion Count: %d" % report["champion_count"])
	print("Champions with Tags: %d" % report["champions_with_tags"])
	print("")
	print("Tag Distribution:")
	for tag in APPROVED_TAGS:
		var count = report["tag_distribution"][tag]
		print("  %s: %d" % [tag, count])
	print("")
	print("Champions Missing Tags: %d" % report["champions_missing_tags"].size())
	for champion in report["champions_missing_tags"]:
		print("  - %s" % champion)
	print("")
	print("Unknown Tags: %d" % report["unknown_tags"].size())
	for entry in report["unknown_tags"]:
		print("  - %s: %s" % [entry["champion"], entry["tag"]])
	print("")
	print("Champions with needs_review: %d" % report["needs_review_champions"].size())
	for champion in report["needs_review_champions"]:
		print("  - %s" % champion)
	print("")
	print("Champion-to-Tag Mapping:")
	for champion_id in report["champion_tags"]:
		var tags = report["champion_tags"][champion_id]
		print("  %s: %s" % [champion_id, str(tags)])
	print("")
	print("============================================================")

func write_report_to_file(report: Dictionary) -> void:
	var file_path := "native_archetype_tag_loading_report.md"
	var file := FileAccess.open(file_path, FileAccess.WRITE)
	if file == null:
		push_error("Failed to open validation report file for writing: %s" % file_path)
		return
	
	var content := "# Native Archetype Tag Loading Validation Report\n\n"
	content += "## Champion Count\n%d\n\n" % report["champion_count"]
	content += "## Champions with Tags\n%d\n\n" % report["champions_with_tags"]
	
	content += "## Tag Distribution\n\n"
	for tag in APPROVED_TAGS:
		var count = report["tag_distribution"][tag]
		content += "- %s: %d\n" % [tag, count]
	content += "\n"
	
	content += "## Champions Missing Tags\n\n"
	if report["champions_missing_tags"].is_empty():
		content += "None\n\n"
	else:
		for champion in report["champions_missing_tags"]:
			content += "- %s\n" % champion
		content += "\n"
	
	content += "## Unknown Tags\n\n"
	if report["unknown_tags"].is_empty():
		content += "None\n\n"
	else:
		for entry in report["unknown_tags"]:
			content += "- %s: %s\n" % [entry["champion"], entry["tag"]]
		content += "\n"
	
	content += "## Champions with needs_review\n\n"
	if report["needs_review_champions"].is_empty():
		content += "None\n\n"
	else:
		for champion in report["needs_review_champions"]:
			content += "- %s\n" % champion
		content += "\n"
	
	content += "## Champion-to-Tag Mapping\n\n"
	for champion_id in report["champion_tags"]:
		var tags = report["champion_tags"][champion_id]
		content += "- %s: %s\n" % [champion_id, str(tags)]
	content += "\n"
	
	file.store_string(content)
	file.close()
	print("Validation report written to: %s" % file_path)
