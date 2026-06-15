extends SceneTree

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")

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
	var report := validate_tags()
	print_report(report)
	write_report_to_file(report)
	quit()

func validate_tags() -> Dictionary:
	var report := {
		"champion_count": 0,
		"tag_counts": {},
		"champions_missing_tags": [],
		"champions_with_unknown_tags": [],
		"champions_with_too_many_tags": [],
		"champions_with_needs_review": [],
		"champion_tags": {}
	}
	
	# Initialize tag counts
	for tag in APPROVED_TAGS:
		report["tag_counts"][tag] = 0
	
	# Get champion catalog
	var catalog := ChampionCatalogScript.build_catalog()
	report["champion_count"] = catalog.size()
	
	# Validate each champion
	for champion_id in catalog.keys():
		var champion = catalog[champion_id]
		var tags = champion.tags
		
		# Store champion tags
		report["champion_tags"][String(champion_id)] = tags.map(func(t): return String(t))
		
		# Check if champion has tags
		if tags.is_empty():
			report["champions_missing_tags"].append(String(champion_id))
			continue
		
		# Check tag count (1-5)
		if tags.size() > 5:
			report["champions_with_too_many_tags"].append({
				"champion": String(champion_id),
				"count": tags.size()
			})
		
		# Check for unknown tags and count approved tags
		for tag in tags:
			var tag_str = String(tag)
			if not APPROVED_TAGS.has(tag_str):
				report["champions_with_unknown_tags"].append({
					"champion": String(champion_id),
					"unknown_tag": tag_str
				})
			else:
				report["tag_counts"][tag_str] += 1
		
		# Check for needs_review
		if tags.has(&"needs_review"):
			report["champions_with_needs_review"].append(String(champion_id))
	
	return report

func print_report(report: Dictionary) -> void:
	print("============================================================")
	print("ARCHETYPE TAG VALIDATION REPORT")
	print("============================================================")
	print("")
	print("Champion Count: %d" % report["champion_count"])
	print("")
	print("Tag Distribution:")
	for tag in APPROVED_TAGS:
		var count = report["tag_counts"][tag]
		print("  %s: %d" % [tag, count])
	print("")
	print("Champions Missing Tags: %d" % report["champions_missing_tags"].size())
	for champion in report["champions_missing_tags"]:
		print("  - %s" % champion)
	print("")
	print("Champions with Unknown Tags: %d" % report["champions_with_unknown_tags"].size())
	for entry in report["champions_with_unknown_tags"]:
		print("  - %s: %s" % [entry["champion"], entry["unknown_tag"]])
	print("")
	print("Champions with Too Many Tags (>5): %d" % report["champions_with_too_many_tags"].size())
	for entry in report["champions_with_too_many_tags"]:
		print("  - %s: %d tags" % [entry["champion"], entry["count"]])
	print("")
	print("Champions with needs_review: %d" % report["champions_with_needs_review"].size())
	for champion in report["champions_with_needs_review"]:
		print("  - %s" % champion)
	print("")
	print("Champion-to-Tag Mapping:")
	for champion_id in report["champion_tags"]:
		var tags = report["champion_tags"][champion_id]
		print("  %s: %s" % [champion_id, str(tags)])
	print("")
	print("============================================================")

func write_report_to_file(report: Dictionary) -> void:
	var file_path := "archetype_tag_validation_report.md"
	var file := FileAccess.open(file_path, FileAccess.WRITE)
	if file == null:
		push_error("Failed to open validation report file for writing: %s" % file_path)
		return
	
	var content := "# Archetype Tag Validation Report\n\n"
	content += "## Champion Count\n%d\n\n" % report["champion_count"]
	
	content += "## Tag Distribution\n\n"
	for tag in APPROVED_TAGS:
		var count = report["tag_counts"][tag]
		content += "- %s: %d\n" % [tag, count]
	content += "\n"
	
	content += "## Champions Missing Tags\n\n"
	if report["champions_missing_tags"].is_empty():
		content += "None\n\n"
	else:
		for champion in report["champions_missing_tags"]:
			content += "- %s\n" % champion
		content += "\n"
	
	content += "## Champions with Unknown Tags\n\n"
	if report["champions_with_unknown_tags"].is_empty():
		content += "None\n\n"
	else:
		for entry in report["champions_with_unknown_tags"]:
			content += "- %s: %s\n" % [entry["champion"], entry["unknown_tag"]]
		content += "\n"
	
	content += "## Champions with Too Many Tags (>5)\n\n"
	if report["champions_with_too_many_tags"].is_empty():
		content += "None\n\n"
	else:
		for entry in report["champions_with_too_many_tags"]:
			content += "- %s: %d tags\n" % [entry["champion"], entry["count"]]
		content += "\n"
	
	content += "## Champions with needs_review\n\n"
	if report["champions_with_needs_review"].is_empty():
		content += "None\n\n"
	else:
		for champion in report["champions_with_needs_review"]:
			content += "- %s\n" % champion
		content += "\n"
	
	content += "## Champion-to-Tag Mapping\n\n"
	for champion_id in report["champion_tags"]:
		var tags = report["champion_tags"][champion_id]
		content += "- %s: %s\n" % [champion_id, str(tags)]
	content += "\n"
	
	file.store_string(content)
	file.close()
	print("Validation report written to: res://archetype_tag_validation_report.md")
