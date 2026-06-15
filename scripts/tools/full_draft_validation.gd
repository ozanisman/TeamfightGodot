extends SceneTree

## Full Snake Draft Validation for Native Draft AI
## Simulates complete League-style snake draft with picks and bans

const DraftStrategyNative := preload("res://scripts/tools/draft_strategy_native.gd")
const ChampionCatalog := preload("res://scripts/simulation/champion_catalog.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

var _strategy: RefCounted = null
var _catalog: RefCounted = null
var _stats_dir: String = "res://stats_output_100k"


func _extract_argument(prefix: String, default_value: String) -> String:
	for a in OS.get_cmdline_user_args():
		var arg: String = str(a)
		if arg.begins_with(prefix):
			return arg.substr(prefix.length())
	return default_value

## Draft sequence: side (B=blue, R=red) and action type
var _draft_sequence = [
	{"side": "B", "action": "BAN"},  # Step 0
	{"side": "R", "action": "BAN"},  # Step 1
	{"side": "B", "action": "BAN"},  # Step 2
	{"side": "R", "action": "BAN"},  # Step 3
	{"side": "B", "action": "BAN"},  # Step 4
	{"side": "R", "action": "BAN"},  # Step 5
	{"side": "B", "action": "PICK"}, # Step 6
	{"side": "R", "action": "PICK"}, # Step 7
	{"side": "R", "action": "PICK"}, # Step 8
	{"side": "B", "action": "PICK"}, # Step 9
	{"side": "B", "action": "PICK"}, # Step 10
	{"side": "R", "action": "PICK"}, # Step 11
	{"side": "R", "action": "BAN"},  # Step 12
	{"side": "B", "action": "BAN"},  # Step 13
	{"side": "R", "action": "BAN"},  # Step 14
	{"side": "B", "action": "BAN"},  # Step 15
	{"side": "R", "action": "PICK"}, # Step 16
	{"side": "B", "action": "PICK"}, # Step 17
	{"side": "B", "action": "PICK"}, # Step 18
	{"side": "R", "action": "PICK"}  # Step 19
]

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	print("Full draft validation: starting...")
	var strategy_name := _extract_argument("--strategy=", "native")
	match strategy_name:
		"native":
			_strategy = DraftStrategyNative.new(_stats_dir)
		_:
			push_error("Full draft validation: unknown strategy '%s'" % strategy_name)
			await HeadlessShutdownScript.teardown_extension_then_quit(self, 1)
			return
	print("Full draft validation: strategy created")
	_catalog = ChampionCatalog.new()
	print("Full draft validation: catalog created")

	var report_lines = _run_full_draft()
	_write_report(report_lines)

	print("Full draft validation complete. Report written to full_draft_validation_report_" + strategy_name + ".txt")
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)

func _run_full_draft() -> Array:
	var report_lines = []
	report_lines.append("=== FULL SNAKE DRAFT VALIDATION ===")
	report_lines.append("Strategy: " + _strategy.get_strategy_name())
	report_lines.append("Stats directory: " + _stats_dir)
	report_lines.append("")
	report_lines.append("STATUS: PENDING")
	report_lines.append("")

	# Get all available champions
	var all_champions = _catalog.get_champion_ids()
	var available = all_champions.duplicate()

	var blue_picks = []
	var red_picks = []
	var blue_bans = []
	var red_bans = []

	report_lines.append("Initial available champions: " + str(available.size()))
	report_lines.append("")

	report_lines.append("--- DRAFT SEQUENCE ---")

	var invalid_selections = []

	for step_idx in range(_draft_sequence.size()):
		var step_data = _draft_sequence[step_idx]
		var side = step_data.side
		var action = step_data.action
		var draft_step = step_idx

		# Build allies and enemies based on current state
		var allies = []
		var enemies = []

		if side == "B":
			allies = blue_picks.duplicate()
			enemies = red_picks.duplicate()
		else:
			allies = red_picks.duplicate()
			enemies = blue_picks.duplicate()

		# Get recommendation
		var selected_champion = ""
		if action == "PICK":
			selected_champion = _strategy.recommend_next_pick(allies, enemies, available, draft_step)
		else:
			selected_champion = _strategy.recommend_next_ban(allies, enemies, available, draft_step)

		# Validate selection
		if selected_champion.is_empty():
			report_lines.append("Step %d: %s %s - ERROR: No champion selected" % [draft_step, side, action])
			invalid_selections.append({"step": draft_step, "side": side, "action": action, "error": "No champion selected"})
			continue

		if not selected_champion in available:
			report_lines.append("Step %d: %s %s - ERROR: %s not in available" % [draft_step, side, action, selected_champion])
			invalid_selections.append({"step": draft_step, "side": side, "action": action, "error": "Champion not available", "champion": selected_champion})
			continue

		# Remove from available and add to appropriate list
		available.erase(selected_champion)

		if action == "PICK":
			if side == "B":
				blue_picks.append(selected_champion)
			else:
				red_picks.append(selected_champion)
		else:
			if side == "B":
				blue_bans.append(selected_champion)
			else:
				red_bans.append(selected_champion)

		report_lines.append("Step %d: %s %s - Selected: %s" % [draft_step, side, action, selected_champion])
		report_lines.append("  Available remaining: " + str(available.size()))

	report_lines.append("")
	report_lines.append("--- FINAL STATE ---")
	report_lines.append("Blue picks: " + str(blue_picks))
	report_lines.append("Red picks: " + str(red_picks))
	report_lines.append("Blue bans: " + str(blue_bans))
	report_lines.append("Red bans: " + str(red_bans))
	report_lines.append("")

	# Validity checks
	report_lines.append("--- VALIDITY CHECKS ---")

	var all_picks = blue_picks + red_picks
	var all_bans = blue_bans + red_bans

	# Check team sizes
	var blue_picks_valid = blue_picks.size() == 5
	var red_picks_valid = red_picks.size() == 5
	report_lines.append("Blue team size: " + str(blue_picks.size()) + " - " + ("PASS" if blue_picks_valid else "FAIL"))
	report_lines.append("Red team size: " + str(red_picks.size()) + " - " + ("PASS" if red_picks_valid else "FAIL"))

	# Check ban count
	var total_bans = all_bans.size()
	var bans_valid = total_bans == 10
	report_lines.append("Total bans: " + str(total_bans) + " - " + ("PASS" if bans_valid else "FAIL"))

	# Check for duplicate picks
	var duplicate_picks = _find_duplicates(all_picks)
	var picks_unique = duplicate_picks.is_empty()
	report_lines.append("Duplicate picks: " + str(duplicate_picks) + " - " + ("PASS" if picks_unique else "FAIL"))

	# Check for duplicate bans
	var duplicate_bans = _find_duplicates(all_bans)
	var bans_unique = duplicate_bans.is_empty()
	report_lines.append("Duplicate bans: " + str(duplicate_bans) + " - " + ("PASS" if bans_unique else "FAIL"))

	# Check no overlap between picks and bans
	var overlap = []
	for pick in all_picks:
		if pick in all_bans:
			overlap.append(pick)
	var no_overlap = overlap.is_empty()
	report_lines.append("Pick/ban overlap: " + str(overlap) + " - " + ("PASS" if no_overlap else "FAIL"))

	# Check invalid selections
	var no_invalid = invalid_selections.is_empty()
	report_lines.append("Invalid selections: " + str(invalid_selections.size()) + " - " + ("PASS" if no_invalid else "FAIL"))
	if not invalid_selections.is_empty():
		for inv in invalid_selections:
			report_lines.append("  Step %d %s %s: %s" % [inv.step, inv.side, inv.action, inv.error])

	report_lines.append("")

	# Overall validity
	var all_valid = blue_picks_valid and red_picks_valid and bans_valid and picks_unique and bans_unique and no_overlap and no_invalid
	for line in report_lines:
		var normalized_line := String(line).strip_edges().to_upper()
		if normalized_line.find("FAIL") >= 0 or normalized_line.begins_with("ERROR") or normalized_line.begins_with("EXCEPTION") or normalized_line.begins_with("WARNING") or normalized_line.begins_with("WARN"):
			all_valid = false
			break
	report_lines.append("Overall validity: " + ("PASS" if all_valid else "FAIL"))

	# Update STATUS line
	for i in range(report_lines.size()):
		if report_lines[i] == "STATUS: PENDING":
			report_lines[i] = "STATUS: " + ("PASS" if all_valid else "FAIL")
			break

	return report_lines

func _find_duplicates(array: Array) -> Array:
	var seen = {}
	var duplicates = []
	for item in array:
		if seen.has(item):
			if not duplicates.has(item):
				duplicates.append(item)
		else:
			seen[item] = true
	return duplicates

func _write_report(lines: Array) -> void:
	var strategy_name = _strategy.get_strategy_name()
	var output_path = "res://logs/full_draft_validation_report_" + strategy_name + ".txt"
	var f = FileAccess.open(output_path, FileAccess.WRITE)
	if f:
		f.store_string("\n".join(lines))
		f.flush()
		f.close()
