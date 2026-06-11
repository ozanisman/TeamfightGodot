extends "res://scripts/app/simulation_viewer_base.gd"

# Recommendation-specific constants
const RECOMMENDATION_STATS_DIR := "res://stats_output"
const RECOMMENDATION_ROLLOUTS_PER_CANDIDATE := 40
const RECOMMENDATION_BASE_SEED := 81000

# Recommendation UI references
var _recommendation_panel: Panel
var _recommendation_title: Label
var _recommendation_list: VBoxContainer


func _on_draft_shell_created() -> void:
	_recommendation_panel = _draft_shell.get_node_or_null("RecommendationPanel")
	_recommendation_title = _draft_shell.get_node_or_null("RecommendationPanel/RecommendationTitle")
	_recommendation_list = _draft_shell.get_node_or_null("RecommendationPanel/RecommendationList")
	_draft_shell.apply_layout(get_viewport_rect().size, true)


func _on_ready_extra() -> void:
	_update_draft_recommendations()


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


func _on_champion_clicked(champion_id: StringName) -> void:
	if _game_state != DRAFTING:
		return

	if _draft_step_index >= SimConstantsScript.DRAFT_SEQUENCE.size():
		return

	var current_turn: String = SimConstantsScript.DRAFT_SEQUENCE[_draft_step_index]

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
		_try_enemy_draft_ai()
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
		_try_enemy_draft_ai()


func _update_draft_recommendations() -> void:
	if _recommendation_panel == null or _recommendation_list == null:
		return

	# Clear previous recommendations
	for child in _recommendation_list.get_children():
		_recommendation_list.remove_child(child)
		child.free()

	# Hide panel if not in drafting state
	if _game_state != DRAFTING:
		_recommendation_panel.visible = false
		return

	# If draft is complete, show final prediction for 5v5 teams
	if _draft_step_index >= SimConstantsScript.DRAFT_SEQUENCE.size():
		_show_final_prediction()
		_recommendation_panel.visible = true
		return

	# Make panel visible during draft
	_recommendation_panel.visible = true

	# Determine which team is picking next
	var next_turn: String = SimConstantsScript.DRAFT_SEQUENCE[_draft_step_index] if _draft_step_index < SimConstantsScript.DRAFT_SEQUENCE.size() else ""

	# Set allies and enemies based on who is picking next
	var allies: Array[StringName] = []
	var enemies: Array[StringName] = []
	var team_label: String = ""

	if next_turn == "P1_PICK":
		allies = _player_picks
		enemies = _enemy_picks
		team_label = "PLAYER 1"
	elif next_turn == "P2_PICK":
		allies = _enemy_picks
		enemies = _player_picks
		team_label = "PLAYER 2"

	var available: Array[StringName] = _get_available_champions()

	if available.is_empty():
		_recommendation_panel.visible = false
		return

	# Update title with team info and color
	_recommendation_title.text = "RECOMMENDATIONS FOR " + team_label
	var title_color := COLOR_PLAYER if team_label == "PLAYER 1" else COLOR_ENEMY
	_recommendation_title.add_theme_color_override("font_color", title_color)

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
		"#" + COLOR_PLAYER.to_html(false),
		team1_prob,
		"#" + COLOR_ENEMY.to_html(false),
		team2_prob
	]
	prediction_label.add_theme_font_size_override("normal_font_size", 24)
	prediction_label.fit_content = true
	_recommendation_list.add_child(prediction_label)
	_add_prediction_model_label(prediction)


func _get_available_champions() -> Array[StringName]:
	var all_champions: Array[StringName] = ChampionCatalogScript.get_champion_ids()
	var available: Array[StringName] = []
	for champion_id in all_champions:
		if champion_id not in _player_picks and champion_id not in _enemy_picks and champion_id not in _player_bans and champion_id not in _enemy_bans:
			available.append(champion_id)
	return available


func _rollout_recommendations(allies: Array[StringName], enemies: Array[StringName], available: Array[StringName]) -> Array[Dictionary]:
	var recommendations: Array[Dictionary] = []
	if _backend == null or available.is_empty():
		return recommendations
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
				RECOMMENDATION_STATS_DIR
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
	while completed_allies.size() < MAX_TEAM_SIZE and not pool.is_empty():
		completed_allies.append(pool.pop_back())
	while completed_enemies.size() < MAX_TEAM_SIZE and not pool.is_empty():
		completed_enemies.append(pool.pop_back())
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
	model_label.add_theme_color_override("font_color", COLOR_SUBTLE)
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
		"#" + COLOR_PLAYER.to_html(false),
		team1_prob,
		"#" + COLOR_ENEMY.to_html(false),
		team2_prob
	]
	prediction_label.add_theme_font_size_override("normal_font_size", 24)
	prediction_label.fit_content = true
	_recommendation_list.add_child(prediction_label)
	_add_prediction_model_label(prediction)
