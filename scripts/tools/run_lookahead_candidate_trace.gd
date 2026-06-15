extends SceneTree

## Lookahead Candidate Trace Generator
## Generates deterministic draft traces with detailed candidate scoring information

const ChampionCatalogScript := preload("res://scripts/simulation/champion_catalog.gd")
const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const DraftStrategyNativePath := "res://scripts/tools/draft_strategy_native.gd"
const DraftStrategyNativeLookaheadPath := "res://scripts/tools/draft_strategy_native_lookahead.gd"
const DraftStrategyNativeLookaheadPickPath := "res://scripts/tools/draft_strategy_native_lookahead_pick.gd"
const HeadlessShutdownScript := preload("res://scripts/tools/headless_shutdown.gd")

const TEAM_SIZE: int = 5
const FIXED_SEED: int = 42

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

var _stats_dir: String = "res://stats_output_100k"
var _backend: RefCounted = null
var _catalog: RefCounted = null


func _init() -> void:
	_backend = NativeSimulationBackendScript.new()
	_catalog = ChampionCatalogScript.new()


func _ready() -> void:
	print("=== LOOKAHEAD CANDIDATE TRACE ===")
	print("Fixed seed: %d" % FIXED_SEED)
	print("Stats directory: %s" % _stats_dir)
	print("")
	
	seed(FIXED_SEED)
	
	# Initialize backend
	if _backend == null or not _backend.is_available():
		print("ERROR: Backend not available")
		quit(1)
		return
	
	print("Backend available")
	
	# Initialize catalog
	if _catalog == null:
		print("ERROR: Catalog not available")
		quit(1)
		return
	
	print("Catalog available")
	
	# Run 4 matchups
	var matchups = [
		{"blue_strategy": "native_lookahead", "red_strategy": "native", "label": "native_lookahead Blue vs native Red"},
		{"blue_strategy": "native", "red_strategy": "native_lookahead", "label": "native Blue vs native_lookahead Red"},
		{"blue_strategy": "native_lookahead_pick_only", "red_strategy": "native", "label": "native_lookahead_pick_only Blue vs native Red"},
		{"blue_strategy": "native", "red_strategy": "native_lookahead_pick_only", "label": "native Blue vs native_lookahead_pick_only Red"}
	]
	
	print("Starting matchups...")
	for matchup in matchups:
		print("Running matchup: %s" % matchup.label)
		var trace = run_deterministic_draft(matchup.blue_strategy, matchup.red_strategy)
		print("--- Matchup: %s ---" % matchup.label)
		for line in trace:
			print(line)
		print("")
	
	print("=== TRACE COMPLETE ===")
	quit(0)


func write_report_via_native(lines: Array) -> bool:
	var content = "\n".join(lines)
	var file = FileAccess.open("lookahead_candidate_trace_report.txt", FileAccess.WRITE)
	if file:
		file.store_string(content)
		file.close()
		return true
	return false


func run_deterministic_draft(blue_strategy_name: String, red_strategy_name: String) -> Array:
	var trace = []
	
	# Instantiate strategies
	var blue_strategy
	var red_strategy
	
	match blue_strategy_name:
		"native":
			blue_strategy = load(DraftStrategyNativePath).new(_stats_dir)
		"native_lookahead":
			blue_strategy = load(DraftStrategyNativeLookaheadPath).new(_stats_dir)
		"native_lookahead_pick_only":
			blue_strategy = load(DraftStrategyNativeLookaheadPickPath).new(_stats_dir)
	
	match red_strategy_name:
		"native":
			red_strategy = load(DraftStrategyNativePath).new(_stats_dir)
		"native_lookahead":
			red_strategy = load(DraftStrategyNativeLookaheadPath).new(_stats_dir)
		"native_lookahead_pick_only":
			red_strategy = load(DraftStrategyNativeLookaheadPickPath).new(_stats_dir)
	
	# Get all champions
	var all_champions = _catalog.get_all_champion_names()
	var available = all_champions.duplicate()
	
	var blue_picks = []
	var red_picks = []
	var blue_bans = []
	var red_bans = []
	
	# Run draft
	for step_idx in range(_draft_sequence.size()):
		var step_info = _draft_sequence[step_idx]
		var side = step_info.side
		var action = step_info.action
		
		var acting_strategy = blue_strategy if side == "B" else red_strategy
		var acting_picks = blue_picks if side == "B" else red_picks
		var enemy_picks = red_picks if side == "B" else blue_picks
		
		if action == "PICK":
			# Determine strategy value based on acting side and strategy type
			var strategy_value = 0  # Default to NATIVE_FULL
			var acting_strategy_name = blue_strategy_name if side == "B" else red_strategy_name
			
			match acting_strategy_name:
				"native":
					strategy_value = 0  # NATIVE_FULL
				"native_lookahead":
					strategy_value = 1  # NATIVE_LOOKAHEAD
				"native_lookahead_pick_only":
					strategy_value = 2  # NATIVE_LOOKAHEAD_PICK
			
			var recommendations = _backend.get_draft_ai_pick_recommendations(
				_stats_dir, available, acting_picks, enemy_picks, 8, step_idx, strategy_value
			)
			
			if not recommendations.is_empty():
				var selected = recommendations[0]
				var champion = StringName(selected.candidate)
				
				# Log detailed trace for pick steps
				trace.append_array(log_pick_trace(step_idx, side, blue_strategy_name if side == "B" else red_strategy_name, selected, recommendations))
				
				# Apply selection
				acting_picks.append(champion)
				available.erase(champion)
		
		elif action == "BAN":
			var champion = acting_strategy.recommend_next_ban(acting_picks, enemy_picks, available, step_idx, side)
			
			if champion != StringName(""):
				var acting_bans = blue_bans if side == "B" else red_bans
				acting_bans.append(champion)
				available.erase(champion)
	
	return trace


func log_pick_trace(step: int, side: String, strategy: String, selected: Dictionary, all_recommendations: Array) -> Array:
	var lines = []
	
	lines.append("Step %d: %s_PICK (strategy: %s)" % [step, side, strategy])
	lines.append("  Selected: %s" % selected.candidate)
	lines.append("  Immediate score: %.4f" % selected.get("immediate_score", 0.0))
	lines.append("  Lookahead adjustment: %.4f" % selected.get("lookahead_adjustment", 0.0))
	lines.append("  Total score: %.4f" % selected.total_score)
	lines.append("  Next pick step: %d" % selected.get("next_pick_step", -1))
	lines.append("  Next pick side: %s" % selected.get("next_pick_side", "N/A"))
	lines.append("  Next pick is enemy: %s" % selected.get("next_pick_is_enemy", false))
	
	if selected.has("opponent_response_candidate"):
		lines.append("  Opponent response: %s (score: %.4f)" % [selected.opponent_response_candidate, selected.opponent_response_score])
	
	# Log top 8 before and after lookahead
	lines.append("  Top 8 candidates:")
	for i in range(all_recommendations.size()):
		var rec = all_recommendations[i]
		var rank_change = ""
		if i == 0:
			rank_change = " [SELECTED]"
		lines.append("    %d. %s: %.4f (immediate: %.4f, adjustment: %.4f)%s" % [
			i + 1, rec.candidate, rec.total_score, 
			rec.get("immediate_score", 0.0), 
			rec.get("lookahead_adjustment", 0.0),
			rank_change
		])
	
	lines.append("")
	
	return lines
