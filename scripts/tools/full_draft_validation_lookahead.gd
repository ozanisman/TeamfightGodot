extends SceneTree

## Full Snake Draft Validation for Native Draft AI with Lookahead
## Simulates complete League-style snake draft with picks and bans

const DraftStrategyNativeLookahead := preload("res://scripts/tools/draft_strategy_native_lookahead.gd")
const ChampionCatalog := preload("res://scripts/simulation/champion_catalog.gd")
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

var _strategy: RefCounted = null
var _catalog: RefCounted = null
var _stats_dir: String = "res://stats_output_100k"

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
	print("Full draft validation (lookahead): starting...")
	_strategy = DraftStrategyNativeLookahead.new(_stats_dir)
	print("Full draft validation (lookahead): strategy created")
	_catalog = ChampionCatalog.new()
	print("Full draft validation (lookahead): catalog created")

	var report_lines = _run_full_draft()
	_write_report(report_lines)

	print("Full draft validation (lookahead) complete. Report written to draft_validation_lookahead_report.txt")
	await HeadlessShutdownScript.teardown_extension_then_quit(self, 0)

func _run_full_draft() -> Array:
	var report_lines = []
	report_lines.append("=== FULL SNAKE DRAFT VALIDATION (LOOKAHEAD) ===")
	report_lines.append("Strategy: " + _strategy.get_strategy_name())
	report_lines.append("Stats directory: " + _stats_dir)
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
			invalid_selections.append(draft_step)
			continue

		if not selected_champion in available:
			report_lines.append("Step %d: %s %s - ERROR: Selected champion not available: %s" % [draft_step, side, action, selected_champion])
			invalid_selections.append(draft_step)
			continue

		# Record selection
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

		# Remove from available
		available.erase(selected_champion)

		report_lines.append("Step %d: %s %s - Selected: %s" % [draft_step, side, action, selected_champion])
		report_lines.append("  Available remaining: " + str(available.size()))

	report_lines.append("")
	report_lines.append("--- FINAL STATE ---")
	report_lines.append("Blue picks: " + str(blue_picks))
	report_lines.append("Red picks: " + str(red_picks))
	report_lines.append("Blue bans: " + str(blue_bans))
	report_lines.append("Red bans: " + str(red_bans))

	report_lines.append("")
	report_lines.append("--- VALIDITY CHECKS ---")

	var validity_pass = true

	# Check team sizes
	if blue_picks.size() == 5:
		report_lines.append("Blue team size: 5 - PASS")
	else:
		report_lines.append("Blue team size: %d - FAIL" % blue_picks.size())
		validity_pass = false

	if red_picks.size() == 5:
		report_lines.append("Red team size: 5 - PASS")
	else:
		report_lines.append("Red team size: %d - FAIL" % red_picks.size())
		validity_pass = false

	# Check ban count
	var total_bans = blue_bans.size() + red_bans.size()
	if total_bans == 10:
		report_lines.append("Total bans: 10 - PASS")
	else:
		report_lines.append("Total bans: %d - FAIL" % total_bans)
		validity_pass = false

	# Check for duplicates
	var duplicate_picks = []
	for pick in blue_picks:
		if pick in red_picks:
			duplicate_picks.append(pick)

	if duplicate_picks.is_empty():
		report_lines.append("Duplicate picks: [] - PASS")
	else:
		report_lines.append("Duplicate picks: " + str(duplicate_picks) + " - FAIL")
		validity_pass = false

	var duplicate_bans = []
	for ban in blue_bans:
		if ban in red_bans:
			duplicate_bans.append(ban)

	if duplicate_bans.is_empty():
		report_lines.append("Duplicate bans: [] - PASS")
	else:
		report_lines.append("Duplicate bans: " + str(duplicate_bans) + " - FAIL")
		validity_pass = false

	# Check for pick/ban overlap
	var pick_ban_overlap = []
	for pick in blue_picks:
		if pick in blue_bans or pick in red_bans:
			pick_ban_overlap.append(pick)
	for pick in red_picks:
		if pick in blue_bans or pick in red_bans:
			pick_ban_overlap.append(pick)

	if pick_ban_overlap.is_empty():
		report_lines.append("Pick/ban overlap: [] - PASS")
	else:
		report_lines.append("Pick/ban overlap: " + str(pick_ban_overlap) + " - FAIL")
		validity_pass = false

	report_lines.append("Invalid selections: %d" % invalid_selections.size())
	if invalid_selections.size() > 0:
		report_lines.append("Invalid steps: " + str(invalid_selections))
		validity_pass = false

	report_lines.append("")
	if validity_pass:
		report_lines.append("Overall validity: PASS")
	else:
		report_lines.append("Overall validity: FAIL")

	return report_lines

func _write_report(lines: Array) -> void:
	var file = FileAccess.open("res://logs/draft_validation_lookahead_report.txt", FileAccess.WRITE)
	if file:
		for line in lines:
			file.store_line(line)
		file.close()
