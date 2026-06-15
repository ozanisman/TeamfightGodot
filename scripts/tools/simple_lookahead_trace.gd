extends SceneTree

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
const DraftStrategyNativePath := "res://scripts/tools/draft_strategy_native.gd"
const DraftStrategyNativeLookaheadPath := "res://scripts/tools/draft_strategy_native_lookahead.gd"
const DraftStrategyNativeLookaheadPickPath := "res://scripts/tools/draft_strategy_native_lookahead_pick.gd"

var _backend: RefCounted = null
var _stats_dir: String = "res://stats_output_100k"
const FIXED_SEED: int = 42

var _draft_sequence = [
	{"side": "B", "action": "BAN"}, {"side": "R", "action": "BAN"}, {"side": "B", "action": "BAN"}, {"side": "R", "action": "BAN"}, {"side": "B", "action": "BAN"}, {"side": "R", "action": "BAN"},
	{"side": "B", "action": "PICK"}, {"side": "R", "action": "PICK"}, {"side": "R", "action": "PICK"}, {"side": "B", "action": "PICK"}, {"side": "B", "action": "PICK"}, {"side": "R", "action": "PICK"},
	{"side": "R", "action": "BAN"}, {"side": "B", "action": "BAN"}, {"side": "R", "action": "BAN"}, {"side": "B", "action": "BAN"},
	{"side": "R", "action": "PICK"}, {"side": "B", "action": "PICK"}, {"side": "B", "action": "PICK"}, {"side": "R", "action": "PICK"}
]

func _ready() -> void:
	print("=== SIMPLE LOOKAHEAD TRACE ===")
	seed(FIXED_SEED)
	_backend = NativeSimulationBackendScript.new()
	
	print("Backend created")
	
	if _backend == null or not _backend.is_available():
		print("ERROR: Backend not available")
		quit(1)
		return
	
	print("Backend available")
	
	var blue_strategy = load(DraftStrategyNativeLookaheadPath).new(_stats_dir)
	var red_strategy = load(DraftStrategyNativePath).new(_stats_dir)
	
	print("Strategies loaded")
	
	var all_champions = ["archer", "berserker", "cleric", "colossus", "disarmer", "earthbender", "frost_mage", "guardian", "mirror_knight", "monk", "necromancer", "ninja", "oracle", "paladin", "rogue", "siren", "sniper", "swordsman", "valkyrie", "warlock", "wraith", "wizard", "windcaller", "mistcaller", "artillery", "silencer"]
	var available = all_champions.duplicate()
	var blue_picks = []
	var red_picks = []
	var blue_bans = []
	var red_bans = []
	
	print("Starting draft...")
	
	for step_idx in range(_draft_sequence.size()):
		var step_info = _draft_sequence[step_idx]
		var side = step_info.side
		var action = step_info.action
		
		var acting_strategy = blue_strategy if side == "B" else red_strategy
		var acting_picks = blue_picks if side == "B" else red_picks
		var enemy_picks = red_picks if side == "B" else blue_picks
		
		if action == "PICK":
			var strategy_value = 1 if side == "B" else 0  # Blue uses lookahead, Red uses baseline
			var recommendations = _backend.get_draft_ai_pick_recommendations(_stats_dir, available, acting_picks, enemy_picks, 8, step_idx, strategy_value)
			
			if not recommendations.is_empty():
				var selected = recommendations[0]
				print("Step %d %s_PICK: %s (immediate: %.4f, adjustment: %.4f, total: %.4f, next_is_enemy: %s)" % [
					step_idx, side, selected.candidate, 
					selected.get("immediate_score", 0.0),
					selected.get("lookahead_adjustment", 0.0),
					selected.total_score,
					selected.get("next_pick_is_enemy", false)
				])
				
				acting_picks.append(StringName(selected.candidate))
				available.erase(StringName(selected.candidate))
			else:
				print("Step %d %s_PICK: No recommendations" % [step_idx, side])
		else:
			var champion = acting_strategy.recommend_next_ban(acting_picks, enemy_picks, available, step_idx, side)
			if champion != StringName(""):
				var acting_bans = blue_bans if side == "B" else red_bans
				acting_bans.append(champion)
				available.erase(champion)
	
	print("Draft complete")
	quit(0)
