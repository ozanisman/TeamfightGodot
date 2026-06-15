extends RefCounted

## Test script for new draft AI
## Tests data layer, evaluator, recommender, and production API

const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")

func _init() -> void:
	pass

func run_test(stats_dir: String = "res://model_stats/stats_output_100k") -> void:
	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("Native backend unavailable")
		return

	print("=== Testing Draft AI Data Layer ===")
	print("Stats directory: %s" % stats_dir)

	if backend.has_method("debug_test_draft_ai_stats"):
		backend.call("debug_test_draft_ai_stats", stats_dir)
		print("Data layer test completed successfully")
	else:
		push_error("debug_test_draft_ai_stats method not found on native backend")

func test_pick_recommendations(stats_dir: String = "res://model_stats/stats_output_100k") -> void:
	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("Native backend unavailable")
		return

	print("=== Testing Draft AI Pick Recommendations API ===")
	print("Stats directory: %s" % stats_dir)

	var available := ["swordsman", "cleric", "colossus", "frost_mage"]
	var allies := ["archer"]
	var enemies := ["berserker"]

	if backend.has_method("get_draft_ai_pick_recommendations"):
		var results := backend.call("get_draft_ai_pick_recommendations", stats_dir, available, allies, enemies, 3)
		print("Got %d recommendations" % results.size())
		for i in range(results.size()):
			var rec = results[i]
			print("%d. %s (score: %.4f)" % [i + 1, rec.candidate, rec.total_score])
			print("   base_power: %.4f" % rec.base_power)
			print("   ally_synergy: %.4f" % rec.ally_synergy)
			print("   enemy_counter_value: %.4f" % rec.enemy_counter_value)
			print("   counter_risk: %.4f" % rec.counter_risk)
		print("Pick recommendations test completed successfully")
	else:
		push_error("get_draft_ai_pick_recommendations method not found on native backend")

func test_ban_recommendations(stats_dir: String = "res://model_stats/stats_output_100k") -> void:
	var backend := NativeSimulationBackendScript.new()
	if not backend.is_available():
		push_error("Native backend unavailable")
		return

	print("=== Testing Draft AI Ban Recommendations API ===")
	print("Stats directory: %s" % stats_dir)

	var available := ["cleric", "colossus", "frost_mage", "assassin"]
	var allies := ["swordsman", "archer"]
	var enemies := ["berserker"]

	if backend.has_method("get_draft_ai_ban_recommendations"):
		var results := backend.call("get_draft_ai_ban_recommendations", stats_dir, available, allies, enemies, 3)
		print("Got %d recommendations" % results.size())
		for i in range(results.size()):
			var rec = results[i]
			print("%d. %s (score: %.4f)" % [i + 1, rec.candidate, rec.total_score])
			print("   enemy_pick_value: %.4f" % rec.enemy_pick_value)
			print("   enemy_synergy: %.4f" % rec.enemy_synergy)
			print("   counters_my_team: %.4f" % rec.counters_my_team)
			print("   own_pick_value_penalty: %.4f" % rec.own_pick_value_penalty)
		print("Ban recommendations test completed successfully")
	else:
		push_error("get_draft_ai_ban_recommendations method not found on native backend")

func test_native_strategy(stats_dir: String = "res://model_stats/stats_output_100k") -> void:
	print("=== Testing Native Draft Strategy Integration ===")
	print("Stats directory: %s" % stats_dir)

	var DraftStrategyNativeScript := preload("res://scripts/tools/draft_strategy_native.gd")
	var strategy := DraftStrategyNativeScript.new(stats_dir)

	var available := ["swordsman", "cleric", "colossus", "frost_mage"]
	var allies := ["archer"]
	var enemies := ["berserker"]

	print("\nTesting pick recommendation...")
	var pick := strategy.recommend_next_pick(allies, enemies, available)
	print("Strategy recommended pick: %s" % pick)

	print("\nTesting ban recommendation...")
	var ban := strategy.recommend_next_ban(allies, enemies, available)
	print("Strategy recommended ban: %s" % ban)

	print("\nNative strategy integration test completed successfully")

# Command-line entry point
func _main() -> void:
	var args := OS.get_cmdline_user_args()
	var test_type := "native_strategy"
	var stats_dir := "res://model_stats/stats_output_100k"

	for arg in args:
		var arg_str := str(arg)
		if arg_str.begins_with("--test="):
			test_type = arg_str.substr(7)
		elif arg_str.begins_with("--stats-dir="):
			stats_dir = arg_str.substr(12)

	match test_type:
		"data_layer":
			run_test(stats_dir)
		"pick_recommendations":
			test_pick_recommendations(stats_dir)
		"ban_recommendations":
			test_ban_recommendations(stats_dir)
		"native_strategy":
			test_native_strategy(stats_dir)
		_:
			push_error("Unknown test type: %s" % test_type)
			print("Available tests: data_layer, pick_recommendations, ban_recommendations, native_strategy")

	quit(0)

# Auto-run if executed as script
if OS.has_feature("editor") == false:
	_main()
