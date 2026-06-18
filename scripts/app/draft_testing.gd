extends "res://scripts/app/simulation_viewer_base.gd"

# Draft testing: skip ban phases, only picks
const DRAFT_SEQUENCE: Array[String] = [
	"B_PICK", "R_PICK", "R_PICK", "B_PICK", "B_PICK", "R_PICK",
	"R_PICK", "B_PICK", "B_PICK", "R_PICK"
]

# Recommendation-specific constants
const RECOMMENDATION_STATS_DIR := "res://model_stats/certified_pairwise_testing_250k"
const RECOMMENDATION_ROLLOUTS_PER_CANDIDATE := 40
const RECOMMENDATION_BASE_SEED := 81000

# Native draft AI configuration
@export var native_draft_stats_dir := "res://model_stats/stats_output_100k"
const NATIVE_STRATEGY_BASELINE := 0

# Required stats files for native draft AI
const REQUIRED_STATS_FILES := ["combat_stats.csv", "matchup_with.csv", "matchup_vs.csv"]
const OPTIONAL_STATS_FILES := ["role_combinations.csv"]

# Error state tracking
var _validated_stats_dir := ""
var _stats_validation_error := ""

# Recommendation UI node paths
@export var _recommendation_panel_path: NodePath = NodePath("RecommendationPanel")
@export var _recommendation_title_path: NodePath = NodePath("RecommendationPanel/RecommendationTitle")
@export var _recommendation_list_path: NodePath = NodePath("RecommendationPanel/RecommendationList")
@export var _native_ai_toggle_path: NodePath = NodePath("RecommendationPanel/NativeAIToggle")
@export var _native_ai_strategy_label_path: NodePath = NodePath("RecommendationPanel/NativeAIStrategyLabel")
@export var _native_ai_strategy_selector_path: NodePath = NodePath("RecommendationPanel/NativeAIStrategySelector")
@export var _compare_baseline_toggle_path: NodePath = NodePath("RecommendationPanel/CompareBaselineToggle")

# Recommendation UI references
var _recommendation_panel: Panel
var _recommendation_title: Label
var _recommendation_list: VBoxContainer
var _native_ai_toggle: CheckBox
var _native_ai_strategy_label: Label
var _native_ai_strategy_selector: OptionButton
var _compare_baseline_toggle: CheckBox

# Native draft AI backend
var _native_backend: RefCounted = null
var _use_native_ai: bool = false


func _on_draft_shell_created() -> void:
	_recommendation_panel = _draft_shell.get_node_or_null(_recommendation_panel_path)
	_recommendation_title = _draft_shell.get_node_or_null(_recommendation_title_path)
	_recommendation_list = _draft_shell.get_node_or_null(_recommendation_list_path)
	
	# Initialize native backend
	const NativeSimulationBackendScript := preload("res://scripts/simulation/native_simulation_backend.gd")
	_native_backend = NativeSimulationBackendScript.new()
	
	# Try to get native AI toggle if it exists
	_native_ai_toggle = _draft_shell.get_node_or_null(_native_ai_toggle_path)
	if _native_ai_toggle != null:
		_native_ai_toggle.toggled.connect(_on_native_ai_toggle_changed)
		# Enable native AI by default if backend is available
		if _native_backend != null and _native_backend.is_available():
			_native_ai_toggle.button_pressed = true
			_use_native_ai = true

	_native_ai_strategy_label = _draft_shell.get_node_or_null(_native_ai_strategy_label_path)
	_native_ai_strategy_selector = _draft_shell.get_node_or_null(_native_ai_strategy_selector_path)
	if _native_ai_strategy_selector != null:
		_native_ai_strategy_selector.clear()
		_native_ai_strategy_selector.add_item("Native baseline", NATIVE_STRATEGY_BASELINE)
		_native_ai_strategy_selector.select(0)
		_native_ai_strategy_selector.item_selected.connect(_on_native_ai_strategy_selected)
	_compare_baseline_toggle = _draft_shell.get_node_or_null(_compare_baseline_toggle_path)
	if _compare_baseline_toggle != null:
		_compare_baseline_toggle.button_pressed = false
		_compare_baseline_toggle.toggled.connect(_on_compare_baseline_toggled)
	
	# Only show toggle if native backend is available
	if _native_ai_toggle != null:
		_native_ai_toggle.visible = _native_backend != null and _native_backend.is_available()
		if not _native_ai_toggle.visible:
			push_warning("Native draft AI backend unavailable, toggle hidden")
	_update_native_ai_strategy_visibility()
	
	_draft_shell.apply_layout(get_viewport_rect().size, true)


func _on_ready_extra() -> void:
	_update_draft_recommendations()


func _on_native_ai_toggle_changed(toggled_on: bool) -> void:
	_use_native_ai = toggled_on
	_update_native_ai_strategy_visibility()
	_update_draft_recommendations()


func _on_native_ai_strategy_selected(_index: int) -> void:
	_update_native_ai_strategy_visibility()
	_update_draft_recommendations()


func _on_compare_baseline_toggled(_toggled_on: bool) -> void:
	_update_draft_recommendations()


func _update_native_ai_strategy_visibility() -> void:
	var should_show_selector: bool = _use_native_ai and _native_backend != null and _native_backend.is_available()
	if _native_ai_strategy_label != null:
		_native_ai_strategy_label.visible = should_show_selector
	if _native_ai_strategy_selector != null:
		_native_ai_strategy_selector.visible = should_show_selector
	var should_show_compare := should_show_selector and _selected_native_ai_strategy() != NATIVE_STRATEGY_BASELINE
	if _compare_baseline_toggle != null:
		_compare_baseline_toggle.visible = should_show_compare
		if not should_show_compare:
			_compare_baseline_toggle.button_pressed = false


func _selected_native_ai_strategy() -> int:
	if _native_ai_strategy_selector == null:
		return NATIVE_STRATEGY_BASELINE
	var selected_id := _native_ai_strategy_selector.get_selected_id()
	if selected_id < 0:
		return NATIVE_STRATEGY_BASELINE
	return selected_id


func _should_compare_with_baseline() -> bool:
	return _compare_baseline_toggle != null and _compare_baseline_toggle.visible and _compare_baseline_toggle.button_pressed


func _on_reset_to_draft_extra() -> void:
	if _recommendation_list != null:
		for child in _recommendation_list.get_children():
			_recommendation_list.remove_child(child)
			child.free()
	if _recommendation_panel != null:
		_recommendation_panel.visible = false
	_update_draft_recommendations()


func _on_before_battle_start() -> void:
	if _recommendation_panel != null:
		_recommendation_panel.visible = false


func _on_random_draft_clicked() -> void:
	while _draft_step_index < DRAFT_SEQUENCE.size():
		var taken: Array[StringName] = _player_picks + _enemy_picks + _player_bans + _enemy_bans
		var champion_ids: Array[StringName] = ChampionCatalogScript.get_champion_ids()
		var available: Array[StringName] = []
		for cid in champion_ids:
			if not cid in taken:
				available.append(cid)

		if available.is_empty():
			break

		var random_champion: StringName = available[randi() % available.size()]
		_on_champion_clicked(random_champion)


func _on_champion_clicked(champion_id: StringName) -> void:
	if _game_state != GameState.DRAFTING:
		return

	if _draft_step_index >= DRAFT_SEQUENCE.size():
		return

	var current_turn: String = DRAFT_SEQUENCE[_draft_step_index]

	# Handle ban phases
	if current_turn.ends_with("_BAN"):
		if champion_id not in _player_bans and champion_id not in _enemy_bans and champion_id not in _player_picks and champion_id not in _enemy_picks:
			if current_turn.begins_with("B_"):
				_player_bans.append(champion_id)
			elif current_turn.begins_with("R_"):
				_enemy_bans.append(champion_id)
		_draft_step_index += 1
		_update_turn_display()
		_update_team_rosters()
		_update_champion_button_style(champion_id)
		_update_start_match_enabled()
		_update_draft_recommendations()
	# Handle pick phases
	elif current_turn.ends_with("_PICK"):
		if current_turn.begins_with("B_"):
			if _player_picks.size() < MAX_TEAM_SIZE:
				_player_picks.append(champion_id)
		elif current_turn.begins_with("R_"):
			if _enemy_picks.size() < MAX_TEAM_SIZE:
				_enemy_picks.append(champion_id)

		_draft_step_index += 1
		_update_turn_display()
		_update_team_rosters()
		_update_champion_button_style(champion_id)
		_update_start_match_enabled()
		_update_draft_recommendations()


func _update_draft_recommendations() -> void:
	if _recommendation_panel == null or _recommendation_list == null:
		return

	# Clear previous recommendations
	for child in _recommendation_list.get_children():
		_recommendation_list.remove_child(child)
		child.free()

	# Hide panel if not in drafting state
	if _game_state != GameState.DRAFTING:
		_recommendation_panel.visible = false
		return

	# If draft is complete, show final prediction for 5v5 teams
	if _draft_step_index >= DRAFT_SEQUENCE.size():
		_show_final_prediction()
		_recommendation_panel.visible = true
		return

	# Make panel visible during draft
	_recommendation_panel.visible = true

	var draft_step := _draft_step_index
	var is_ban_turn := _is_ban_step(draft_step)
	var acting_side := _acting_side_for_step(draft_step)

	# Set allies and enemies based on who is acting
	var allies: Array[StringName] = []
	var enemies: Array[StringName] = []
	var team_label: String = ""

	if acting_side == "B":
		allies = _player_picks
		enemies = _enemy_picks
		team_label = "PLAYER 1" + (" (BAN)" if is_ban_turn else " (PICK)")
	else:
		allies = _enemy_picks
		enemies = _player_picks
		team_label = "PLAYER 2" + (" (BAN)" if is_ban_turn else " (PICK)")

	var available: Array[StringName] = _get_available_champions()

	if available.is_empty():
		_recommendation_panel.visible = false
		return

	# Update title with team info and color
	_recommendation_title.text = "RECOMMENDATIONS FOR " + team_label
	var title_color := UiTokensScript.COLOR_PLAYER if team_label.begins_with("PLAYER 1") else UiTokensScript.COLOR_ENEMY
	_recommendation_title.add_theme_color_override("font_color", title_color)

	# Use native AI if enabled and available
	if _use_native_ai and _native_backend != null and _native_backend.is_available():
		# Validate stats directory
		var validation_error := _validate_native_stats_dir()
		if not validation_error.is_empty():
			_show_native_error_message(validation_error)
		else:
			_show_native_recommendations(allies, enemies, available, draft_step, acting_side)
	else:
		_show_legacy_recommendations(allies, enemies, available)


func _get_available_champions() -> Array[StringName]:
	var all_champions: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	var available: Array[StringName] = []
	for champion_id in all_champions:
		if champion_id not in _player_picks and champion_id not in _enemy_picks and champion_id not in _player_bans and champion_id not in _enemy_bans:
			available.append(champion_id)
	return available


# Draft step helper functions
func _is_ban_step(step: int) -> bool:
	# Ban steps: 0-5 (phase 1), 12-15 (phase 2)
	return (step >= 0 and step <= 5) or (step >= 12 and step <= 15)


func _is_pick_step(step: int) -> bool:
	# Pick steps: 6-11 (phase 1), 16-19 (phase 2)
	return (step >= 6 and step <= 11) or (step >= 16 and step <= 19)


func _acting_side_for_step(step: int) -> String:
	# Return "B" or "R" based on fixed draft order
	# 0 B_BAN, 1 R_BAN, 2 B_BAN, 3 R_BAN, 4 B_BAN, 5 R_BAN
	# 6 B_PICK, 7 R_PICK, 8 R_PICK, 9 B_PICK, 10 B_PICK, 11 R_PICK
	# 12 R_BAN, 13 B_BAN, 14 R_BAN, 15 B_BAN
	# 16 R_PICK, 17 B_PICK, 18 B_PICK, 19 R_PICK
	
	# Blue acts on: 0, 2, 4, 6, 9, 10, 13, 15, 17, 18
	# Red acts on: 1, 3, 5, 7, 8, 11, 12, 14, 16, 19
	
	var blue_steps := [0, 2, 4, 6, 9, 10, 13, 15, 17, 18]
	if step in blue_steps:
		return "B"
	else:
		return "R"


func _validate_native_stats_dir() -> String:
	# Check if stats directory exists and contains required files
	# Returns empty string if valid, error message otherwise
	# Caches validation result by stats directory path
	
	# Return cached result if stats dir unchanged
	if native_draft_stats_dir == _validated_stats_dir:
		return _stats_validation_error
	
	# Revalidate if stats dir changed
	if not DirAccess.dir_exists_absolute(native_draft_stats_dir):
		_validated_stats_dir = native_draft_stats_dir
		_stats_validation_error = "Native Draft AI unavailable: stats directory not found."
		return _stats_validation_error
	
	var missing_files := []
	for file_name in REQUIRED_STATS_FILES:
		var file_path := native_draft_stats_dir.path_join(file_name)
		if not FileAccess.file_exists(file_path):
			missing_files.append(file_name)
	
	if not missing_files.is_empty():
		_validated_stats_dir = native_draft_stats_dir
		_stats_validation_error = "Native Draft AI unavailable: missing required files: " + ", ".join(missing_files)
		return _stats_validation_error
	
	# Validation passed
	_validated_stats_dir = native_draft_stats_dir
	_stats_validation_error = ""
	return ""  # No error


func _show_native_error_message(error_message: String) -> void:
	var error_label := Label.new()
	error_label.text = error_message
	error_label.add_theme_color_override("font_color", UiTokensScript.COLOR_WARNING)
	error_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_recommendation_list.add_child(error_label)


func _show_native_recommendations(allies: Array[StringName], enemies: Array[StringName], available: Array[StringName], draft_step: int, acting_side: String) -> void:
	var is_ban_turn := _is_ban_step(draft_step)
	var strategy := _selected_native_ai_strategy()
	var recommendations := _get_native_recommendations(allies, enemies, available, draft_step, acting_side, strategy)
	
	# Handle empty recommendations
	if recommendations.is_empty():
		var empty_label := Label.new()
		empty_label.text = "Native Draft AI returned no recommendations."
		empty_label.add_theme_color_override("font_color", UiTokensScript.COLOR_SUBTLE)
		_recommendation_list.add_child(empty_label)
		return

	var compare_recommendations: Array = []
	if strategy != NATIVE_STRATEGY_BASELINE and _should_compare_with_baseline():
		compare_recommendations = _get_native_recommendations(
			allies,
			enemies,
			available,
			draft_step,
			acting_side,
			NATIVE_STRATEGY_BASELINE
		)

	if not compare_recommendations.is_empty():
		var selected_top := String((recommendations[0] as Dictionary).get("candidate", ""))
		var baseline_top := String((compare_recommendations[0] as Dictionary).get("candidate", ""))
		if selected_top != baseline_top:
			var change_label := Label.new()
			change_label.text = "changed top recommendation: %s -> %s" % [baseline_top, selected_top]
			change_label.add_theme_color_override("font_color", UiTokensScript.COLOR_WARNING)
			_recommendation_list.add_child(change_label)

	_render_native_recommendation_block(_native_strategy_display_name(strategy), recommendations, is_ban_turn, strategy)

	if not compare_recommendations.is_empty():
		var separator_between_models := HSeparator.new()
		_recommendation_list.add_child(separator_between_models)
		_render_native_recommendation_block(
			_native_strategy_display_name(NATIVE_STRATEGY_BASELINE),
			compare_recommendations,
			is_ban_turn,
			NATIVE_STRATEGY_BASELINE
		)

	# Add separator
	var separator := HSeparator.new()
	_recommendation_list.add_child(separator)

	# Add model label with explanation
	var model_label := Label.new()
	model_label.text = "Model: Native Draft AI (higher score is better)"
	model_label.add_theme_color_override("font_color", UiTokensScript.COLOR_SUBTLE)
	_recommendation_list.add_child(model_label)


func _get_native_recommendations(allies: Array[StringName], enemies: Array[StringName], available: Array[StringName], draft_step: int, acting_side: String, strategy: int) -> Array:
	if _is_ban_step(draft_step):
		return _native_backend.get_draft_ai_ban_recommendations(
			native_draft_stats_dir,
			available,
			allies,
			enemies,
			5,  # max_results
			draft_step,
			acting_side,
			{},  # weight_overrides (empty for default)
			strategy
		)
	return _native_backend.get_draft_ai_pick_recommendations(
		native_draft_stats_dir,
		available,
		allies,
		enemies,
		5,  # max_results
		draft_step,
		strategy
	)


func _render_native_recommendation_block(title: String, recommendations: Array, is_ban_turn: bool, strategy: int) -> void:
	var section_label := Label.new()
	section_label.text = title
	section_label.add_theme_font_size_override("font_size", 18)
	_recommendation_list.add_child(section_label)

	for i in range(mini(5, recommendations.size())):
		var recommendation: Dictionary = recommendations[i]
		var label := Label.new()
		var champion_name := String(recommendation.get("candidate", ""))
		var score := float(recommendation.get("total_score", 0.0))
		label.text = "%d. %s (score: %.3f)" % [i + 1, champion_name, score]
		_recommendation_list.add_child(label)

		var breakdown_label := Label.new()
		if is_ban_turn:
			breakdown_label.text = _format_ban_breakdown(recommendation)
		else:
			breakdown_label.text = _format_pick_breakdown(recommendation)
		breakdown_label.add_theme_color_override("font_color", UiTokensScript.COLOR_SUBTLE)
		breakdown_label.add_theme_font_size_override("font_size", 16)
		_recommendation_list.add_child(breakdown_label)



func _native_strategy_display_name(_strategy: int) -> String:
	return "Native baseline"


func _format_pick_breakdown(rec: Dictionary) -> String:
	var parts := []
	
	var base_power: float = rec.get("base_power")
	if base_power != null:
		parts.append("base %+.3f" % float(base_power))
	
	var ally_synergy: float = rec.get("ally_synergy")
	if ally_synergy != null:
		parts.append("syn %+.3f" % float(ally_synergy))
	
	var enemy_counter_value: float = rec.get("enemy_counter_value")
	if enemy_counter_value != null:
		parts.append("ctr %+.3f" % float(enemy_counter_value))
	
	var counter_risk: float = rec.get("counter_risk")
	if counter_risk != null:
		parts.append("risk %+.3f" % float(counter_risk))
	
	var role_fit: float = rec.get("role_fit")
	if role_fit != null:
		parts.append("role %+.3f" % float(role_fit))
	
	var comp_fit: float = rec.get("comp_fit")
	if comp_fit != null:
		parts.append("comp %+.3f" % float(comp_fit))
	
	if parts.is_empty():
		return ""
	return "   " + " | ".join(parts)


func _format_ban_breakdown(rec: Dictionary) -> String:
	var parts := []
	
	var denial_value: float = rec.get("denial_value")
	if denial_value != null:
		parts.append("denial %+.3f" % float(denial_value))
	
	var enemy_synergy: float = rec.get("enemy_synergy")
	if enemy_synergy != null:
		parts.append("syn %+.3f" % float(enemy_synergy))
	
	var counters_my_team: float = rec.get("counters_my_team")
	if counters_my_team != null:
		parts.append("counters %+.3f" % float(counters_my_team))
	
	var fills_enemy_role_need: float = rec.get("fills_enemy_role_need")
	if fills_enemy_role_need != null:
		parts.append("role %+.3f" % float(fills_enemy_role_need))
	
	var enemy_comp_fit: float = rec.get("enemy_comp_fit")
	if enemy_comp_fit != null:
		parts.append("comp %+.3f" % float(enemy_comp_fit))
	
	var early_ban_fallback: float = rec.get("early_ban_fallback_component")
	if early_ban_fallback != null:
		parts.append("fallback %+.3f" % float(early_ban_fallback))
	
	if parts.is_empty():
		return ""
	return "   " + " | ".join(parts)


func _show_legacy_recommendations(allies: Array[StringName], enemies: Array[StringName], available: Array[StringName]) -> void:
	var recommendations: Array[Dictionary] = _rollout_recommendations(allies, enemies, available)
	var using_rollout := not recommendations.is_empty()
	if not using_rollout:
		push_error("Draft recommendations: rollout scorer failed, falling back to legacy recommender")
		recommendations = _legacy_recommendations(allies, enemies, available)

	for i in range(mini(5, recommendations.size())):
		var recommendation: Dictionary = recommendations[i]
		var label := Label.new()
		if using_rollout:
			label.text = "%d. %s (expected: %.1f%%)" % [
				i + 1,
				String(recommendation.get("champion", "")),
				float(recommendation.get("expected_win", 0.5)) * 100.0
			]
		else:
			label.text = "%d. %s (base: %.3f, synergy: %.3f, counter: %.3f, final: %.3f)" % [
				i + 1,
				String(recommendation.get("champion", "")),
				float(recommendation.get("base", 0.0)),
				float(recommendation.get("synergy", 0.0)),
				float(recommendation.get("counter", 0.0)),
				float(recommendation.get("final", 0.0))
			]
		_recommendation_list.add_child(label)

	# Add separator between recommendations and prediction
	var separator := HSeparator.new()
	_recommendation_list.add_child(separator)

	# Calculate and display game prediction
	var prediction: Dictionary = _backend.predict_draft_winner(
		_player_picks,
		_enemy_picks,
		RECOMMENDATION_STATS_DIR,
		0.50,  # base_weight
		0.25,  # synergy_weight
		0.25,  # counter_weight
		0.25,  # matchup_weight
		0.0,   # composition_weight
		10.0,  # logistic_k
		true   # include_breakdown
	)
	var team1_prob: float = prediction.get("team1_prob", 0.5) * 100.0
	var team2_prob: float = prediction.get("team2_prob", 0.5) * 100.0
	var prediction_label := RichTextLabel.new()
	prediction_label.bbcode_enabled = true
	prediction_label.text = "Game Prediction: [color=%s]P1 [%.1f%%][/color] vs [color=%s][%.1f%%] P2[/color]" % [
		"#" + UiTokensScript.COLOR_PLAYER.to_html(false),
		team1_prob,
		"#" + UiTokensScript.COLOR_ENEMY.to_html(false),
		team2_prob
	]
	prediction_label.add_theme_font_size_override("normal_font_size", 24)
	prediction_label.fit_content = true
	_recommendation_list.add_child(prediction_label)
	_add_prediction_model_label(prediction)


func _rollout_recommendations(allies: Array[StringName], enemies: Array[StringName], available: Array[StringName]) -> Array[Dictionary]:
	var recommendations: Array[Dictionary] = []
	if _backend == null or available.is_empty():
		return recommendations
	
	# Calculate global draft position (1-10) from current step index
	var draft_position := SimConstantsScript.get_global_pick_position(_draft_step_index)
	
	var state_seed := _stable_draft_seed(allies, enemies, available)
	for candidate_index in range(available.size()):
		var candidate: StringName = available[candidate_index]
		var score_sum: float = 0.0
		var valid_rollouts: int = 0
		for rollout_index in range(RECOMMENDATION_ROLLOUTS_PER_CANDIDATE):
			var completion := _complete_recommendation_rollout(
				allies,
				enemies,
				available,
				candidate,
				state_seed + candidate_index * 1000 + rollout_index
			)
			if completion["allies"].size() != MAX_TEAM_SIZE or completion["enemies"].size() != MAX_TEAM_SIZE:
				continue
			var prediction: Dictionary = _backend.predict_draft_winner(
				completion["allies"],
				completion["enemies"],
				RECOMMENDATION_STATS_DIR,
				0.50,  # base_weight
				0.25,  # synergy_weight
				0.25,  # counter_weight
				0.25,  # matchup_weight
				0.0,   # composition_weight
				10.0,  # logistic_k
				true,  # include_breakdown
				1.2,   # synergy_amplification
				1.2,   # matchup_amplification
				0,     # scoring_mode (CERTIFIED_PAIRWISE_PROBABILITY)
				0.0,   # variance_weight
				0.0,   # cc_weight
				0.0,   # mobility_weight
				0.0,   # sustain_weight
				0.0,   # best_counter_weight
				0.0,   # worst_counter_weight
				0.0,   # best_synergy_weight
				0.0,   # worst_synergy_weight
				0,     # synergy_aggregation
				0,     # counter_aggregation
				false, # use_decorrelated_scoring
				draft_position,
				0.7,   # early_pick_base_weight
				0.4    # late_pick_counter_weight
			)
			if prediction.is_empty() or not prediction.has("scoring_mode"):
				continue
			score_sum += float(prediction.get("team1_prob", 0.5))
			valid_rollouts += 1
		if valid_rollouts > 0:
			recommendations.append({
				"champion": candidate,
				"expected_win": score_sum / float(valid_rollouts),
			})
	recommendations.sort_custom(func(a, b):
		if is_equal_approx(float(a["expected_win"]), float(b["expected_win"])):
			return String(a["champion"]) < String(b["champion"])
		return float(a["expected_win"]) > float(b["expected_win"])
	)
	return recommendations


func _complete_recommendation_rollout(allies: Array[StringName], enemies: Array[StringName], available: Array[StringName], candidate: StringName, seed: int) -> Dictionary:
	var completed_allies: Array[StringName] = allies.duplicate()
	var completed_enemies: Array[StringName] = enemies.duplicate()
	completed_allies.append(candidate)
	var pool: Array[StringName] = []
	for champion_id in available:
		if champion_id != candidate:
			pool.append(champion_id)
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	_shuffle_recommendation_pool(pool, rng)
	
	# Simulate snake draft order for remaining picks
	# Full pick sequence: B_PICK, R_PICK, R_PICK, B_PICK, B_PICK, R_PICK, R_PICK, B_PICK, B_PICK, R_PICK
	var pick_sequence := ["B_PICK", "R_PICK", "R_PICK", "B_PICK", "B_PICK", "R_PICK", "R_PICK", "B_PICK", "B_PICK", "R_PICK"]
	
	# Calculate starting position in sequence based on current team sizes
	var total_picks := completed_allies.size() + completed_enemies.size()
	var sequence_index := total_picks
	
	while completed_allies.size() < MAX_TEAM_SIZE or completed_enemies.size() < MAX_TEAM_SIZE:
		if sequence_index >= pick_sequence.size():
			break
		
		var turn: String = pick_sequence[sequence_index]
		if turn == "B_PICK" and completed_allies.size() < MAX_TEAM_SIZE:
			if not pool.is_empty():
				completed_allies.append(pool.pop_back())
		elif turn == "R_PICK" and completed_enemies.size() < MAX_TEAM_SIZE:
			if not pool.is_empty():
				completed_enemies.append(pool.pop_back())
		sequence_index += 1
	
	return {"allies": completed_allies, "enemies": completed_enemies}


func _legacy_recommendations(allies: Array[StringName], enemies: Array[StringName], available: Array[StringName]) -> Array[Dictionary]:
	if _backend == null:
		return []
	var rows: Array = _backend.get_draft_recommendations_with_breakdowns(
		allies,
		enemies,
		available,
		5,
		RECOMMENDATION_STATS_DIR,
		0.50,
		0.25,
		0.25
	)
	var recommendations: Array[Dictionary] = []
	for row in rows:
		recommendations.append(Dictionary(row))
	return recommendations


func _stable_draft_seed(allies: Array[StringName], enemies: Array[StringName], available: Array[StringName]) -> int:
	var hash_value: int = RECOMMENDATION_BASE_SEED + _draft_step_index * 7919
	for champion_id in allies:
		hash_value = hash_value * 31 + String(champion_id).hash()
	for champion_id in enemies:
		hash_value = hash_value * 37 + String(champion_id).hash()
	hash_value = hash_value * 41 + available.size()
	return absi(hash_value)


func _shuffle_recommendation_pool(values: Array[StringName], rng: RandomNumberGenerator) -> void:
	for i in range(values.size() - 1, 0, -1):
		var j := rng.randi_range(0, i)
		var tmp := values[i]
		values[i] = values[j]
		values[j] = tmp


func _add_prediction_model_label(prediction: Dictionary) -> void:
	var model_label := Label.new()
	if int(prediction.get("scoring_mode", -1)) == 3:
		model_label.text = "Prediction model: certified pairwise probability"
	else:
		model_label.text = "Prediction model: legacy scorer"
	model_label.add_theme_color_override("font_color", UiTokensScript.COLOR_SUBTLE)
	_recommendation_list.add_child(model_label)


func _show_final_prediction() -> void:
	var title_label := Label.new()
	title_label.text = "Final 5v5 Prediction"
	title_label.add_theme_font_size_override("font_size", 24)
	_recommendation_list.add_child(title_label)

	# Calculate and display game prediction
	var prediction: Dictionary = _backend.predict_draft_winner(
		_player_picks,
		_enemy_picks,
		RECOMMENDATION_STATS_DIR,
		0.50,  # base_weight
		0.25,  # synergy_weight
		0.25,  # counter_weight
		0.25,  # matchup_weight
		0.0,   # composition_weight
		10.0,  # logistic_k
		true   # include_breakdown
	)
	var team1_prob: float = prediction.get("team1_prob", 0.5) * 100.0
	var team2_prob: float = prediction.get("team2_prob", 0.5) * 100.0
	var prediction_label := RichTextLabel.new()
	prediction_label.bbcode_enabled = true
	prediction_label.text = "Game Prediction: [color=%s]P1 [%.1f%%][/color] vs [color=%s][%.1f%%] P2[/color]" % [
		"#" + UiTokensScript.COLOR_PLAYER.to_html(false),
		team1_prob,
		"#" + UiTokensScript.COLOR_ENEMY.to_html(false),
		team2_prob
	]
	prediction_label.add_theme_font_size_override("normal_font_size", 24)
	prediction_label.fit_content = true
	_recommendation_list.add_child(prediction_label)
	_add_prediction_model_label(prediction)
