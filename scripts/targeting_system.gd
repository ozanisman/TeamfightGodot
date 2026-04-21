extends RefCounted
class_name TargetingSystem

const CombatData = preload("res://scripts/combat_data.gd")

func get_strategy_for(unit: Node2D) -> Dictionary:
	var role := String(unit.get("role"))
	match role:
		"tank":
			return {
				"name": "Protector",
				"distance_weight": 1.5,
				"hp_weight": 0.0,
				"stickiness_bonus": 2.0,
				"prefers_kiting": false,
				"role_priorities": {"assassin": -5.0, "fighter": -2.0},
				"tank_penalty": 0.4,
				"in_range_bonus": 1.0,
				"switch_margin": 0.75,
				"threat_response_weight": 2.0,
				"prey_instinct_weight": 0.0,
				"cluster_weight": 1.5,
				"spacing_weight": 0.0,
				"ally_role_priorities": {"marksman": -5.0, "mage": -5.0, "support": -3.0},
				"ally_hp_weight": 0.0,
				"ally_distance_weight": 1.0,
				"ally_threat_weight": 25.0,
				"bodyguard_weight": 3.0,
				"execute_bonus_weight": 0.0,
				"projectile_time_weight": 0.0,
				"carry_peel_weight": 0.0,
			}
		"assassin":
			return {
				"name": "Diver",
				"distance_weight": 0.15,
				"hp_weight": 2.0,
				"stickiness_bonus": 2.0,
				"prefers_kiting": false,
				"role_priorities": {"marksman": -15.0, "mage": -15.0, "support": -10.0, "fighter": 10.0},
				"tank_penalty": 7.0,
				"in_range_bonus": 0.6,
				"switch_margin": 0.75,
				"threat_response_weight": 0.8,
				"prey_instinct_weight": 2.0,
				"cluster_weight": 0.0,
				"spacing_weight": 0.0,
				"ally_role_priorities": {},
				"ally_hp_weight": 0.0,
				"ally_distance_weight": 1.0,
				"ally_threat_weight": 0.0,
				"bodyguard_weight": 0.0,
				"execute_bonus_weight": 50.0,
				"projectile_time_weight": 0.0,
				"carry_peel_weight": 0.0,
			}
		"marksman":
			return {
				"name": "Kiter",
				"distance_weight": 1.0,
				"hp_weight": 0.5,
				"stickiness_bonus": 5.0,
				"prefers_kiting": true,
				"role_priorities": {},
				"tank_penalty": 2.6,
				"in_range_bonus": 0.35,
				"switch_margin": 0.75,
				"threat_response_weight": 1.0,
				"prey_instinct_weight": 0.0,
				"cluster_weight": 0.0,
				"spacing_weight": 2.5,
				"ally_role_priorities": {},
				"ally_hp_weight": 0.0,
				"ally_distance_weight": 1.0,
				"ally_threat_weight": 0.0,
				"bodyguard_weight": 0.0,
				"execute_bonus_weight": 50.0,
				"projectile_time_weight": 0.35,
				"carry_peel_weight": 60.0,
			}
		"fighter":
			return {
				"name": "Brawler",
				"distance_weight": 3.0,
				"hp_weight": 0.5,
				"stickiness_bonus": 2.0,
				"prefers_kiting": false,
				"role_priorities": {"marksman": -1.0, "mage": -1.0},
				"tank_penalty": 1.0,
				"in_range_bonus": 0.9,
				"switch_margin": 0.75,
				"threat_response_weight": 1.8,
				"prey_instinct_weight": 0.0,
				"cluster_weight": 0.0,
				"spacing_weight": 0.0,
				"ally_role_priorities": {},
				"ally_hp_weight": 0.0,
				"ally_distance_weight": 1.0,
				"ally_threat_weight": 0.0,
				"bodyguard_weight": 0.0,
				"execute_bonus_weight": 50.0,
				"projectile_time_weight": 0.0,
				"carry_peel_weight": 0.0,
			}
		"mage":
			return {
				"name": "Spellcaster",
				"distance_weight": 1.2,
				"hp_weight": 2.5,
				"stickiness_bonus": 2.0,
				"prefers_kiting": true,
				"role_priorities": {"marksman": -4.0, "support": -2.0},
				"tank_penalty": 2.3,
				"in_range_bonus": 0.45,
				"switch_margin": 0.75,
				"threat_response_weight": 1.1,
				"prey_instinct_weight": 0.0,
				"cluster_weight": 3.0,
				"spacing_weight": 1.5,
				"ally_role_priorities": {},
				"ally_hp_weight": 0.0,
				"ally_distance_weight": 1.0,
				"ally_threat_weight": 0.0,
				"bodyguard_weight": 0.0,
				"execute_bonus_weight": 50.0,
				"projectile_time_weight": 0.45,
				"carry_peel_weight": 60.0,
			}
		"support":
			return {
				"name": "Enchanter",
				"distance_weight": 1.5,
				"hp_weight": 0.0,
				"stickiness_bonus": 1.0,
				"prefers_kiting": true,
				"role_priorities": {"assassin": -8.0, "fighter": -4.0},
				"tank_penalty": 1.5,
				"in_range_bonus": 0.4,
				"switch_margin": 0.75,
				"threat_response_weight": 1.7,
				"prey_instinct_weight": 0.0,
				"cluster_weight": 0.0,
				"spacing_weight": 1.0,
				"ally_role_priorities": {"marksman": -5.0, "mage": -5.0, "fighter": -2.0},
				"ally_hp_weight": 5.0,
				"ally_distance_weight": 1.0,
				"ally_threat_weight": 25.0,
				"bodyguard_weight": 4.0,
				"execute_bonus_weight": 0.0,
				"projectile_time_weight": 0.3,
				"carry_peel_weight": 0.0,
			}
		_:
			return {
				"name": "Default",
				"distance_weight": CombatData.DISTANCE_WEIGHT_DEFAULT,
				"hp_weight": CombatData.HP_WEIGHT_DPS_DEFAULT,
				"stickiness_bonus": CombatData.STICKINESS_DEFAULT,
				"prefers_kiting": false,
				"role_priorities": {},
				"tank_penalty": CombatData.TANK_PENALTY_DEFAULT,
				"in_range_bonus": CombatData.IN_RANGE_SCORE_BONUS,
				"switch_margin": CombatData.TARGET_SWITCH_MARGIN,
				"threat_response_weight": 0.0,
				"prey_instinct_weight": 0.0,
				"cluster_weight": 0.0,
				"spacing_weight": 0.0,
				"ally_role_priorities": {},
				"ally_hp_weight": 0.0,
				"ally_distance_weight": 1.0,
				"ally_threat_weight": 0.0,
				"bodyguard_weight": 0.0,
				"execute_bonus_weight": 0.0,
				"projectile_time_weight": 0.0,
				"carry_peel_weight": 0.0,
			}


func select_ally(unit: Node2D, allies: Array[Node2D]) -> Node2D:
	var strategy := get_strategy_for(unit)
	var alive_allies: Array[Node2D] = []
	for ally in allies:
		if bool(ally.get("alive")):
			alive_allies.append(ally)
	if alive_allies.is_empty():
		return null

	var critical_allies: Array[Node2D] = []
	for ally in alive_allies:
		var max_hp := float(ally.get("max_hp"))
		if max_hp > CombatData.EPSILON:
			var hp_ratio := float(ally.get("hp")) / max_hp
			if hp_ratio <= 0.35:
				critical_allies.append(ally)

	var pool := critical_allies if not critical_allies.is_empty() else alive_allies
	var best: Node2D = pool[0]
	var best_score := _score_ally(unit, best, strategy)
	for ally in pool:
		var score := _score_ally(unit, ally, strategy)
		if score < best_score or (is_equal_approx(score, best_score) and int(ally.get("instance_id")) < int(best.get("instance_id"))):
			best = ally
			best_score = score
	return best


func select_target(unit: Node2D, enemies: Array[Node2D], allies: Array[Node2D] = []) -> Dictionary:
	var forced_id := int(unit.call("get_forced_target_id"))
	var alive_enemies: Array[Node2D] = []
	for enemy in enemies:
		if bool(enemy.get("alive")):
			alive_enemies.append(enemy)
	if alive_enemies.is_empty():
		return {}

	if forced_id >= 0:
		for enemy in alive_enemies:
			if int(enemy.get("instance_id")) == forced_id:
				return _score_candidate(unit, enemy, get_strategy_for(unit), allies, alive_enemies)

	var strategy := get_strategy_for(unit)
	var best_candidate := _score_candidate(unit, alive_enemies[0], strategy, allies, alive_enemies)
	for enemy in alive_enemies:
		var candidate := _score_candidate(unit, enemy, strategy, allies, alive_enemies)
		if _should_choose(unit, candidate, best_candidate, strategy):
			best_candidate = candidate
	return best_candidate


func score_target(unit: Node2D, enemy: Node2D, strategy: Dictionary, allies: Array[Node2D] = [], enemies: Array[Node2D] = []) -> Dictionary:
	return _score_candidate(unit, enemy, strategy, allies, enemies)


func should_switch(current_score: float, new_score: float, strategy: Dictionary, unit: Node2D) -> bool:
	if unit == null:
		return true
	if float(unit.get("target_switch_lock_timer")) > 0.0:
		return false
	if bool(unit.get("in_range")):
		var attack_speed := maxf(0.001, float(unit.get("attack_speed")))
		var swing := 1.0 / attack_speed
		var commit_window := minf(0.18, swing * 0.25)
		if float(unit.get("attack_cooldown_remaining")) <= commit_window:
			return false
	var margin := float(strategy.get("switch_margin", CombatData.TARGET_SWITCH_MARGIN))
	return new_score <= (current_score - margin)


func _should_choose(unit: Node2D, candidate: Dictionary, current: Dictionary, strategy: Dictionary) -> bool:
	if current.is_empty():
		return true
	return should_switch(float(current.get("score", 0.0)), float(candidate.get("score", 0.0)), strategy, unit)


func _score_ally(unit: Node2D, ally: Node2D, strategy: Dictionary) -> float:
	var dist := Vector2(unit.get("world_pos")).distance_to(Vector2(ally.get("world_pos")))
	var max_hp := maxf(float(ally.get("max_hp")), CombatData.EPSILON)
	var hp_ratio := float(ally.get("hp")) / max_hp
	var score := dist * float(strategy.get("ally_distance_weight", 1.0))
	score += hp_ratio * float(strategy.get("ally_hp_weight", 0.0)) * 10.0
	score -= float(ally.get("perceived_threat")) * float(strategy.get("ally_threat_weight", 0.0))
	var ally_roles: Dictionary = strategy.get("ally_role_priorities", {})
	score += float(ally_roles.get(String(ally.get("role")), 0.0))
	return score


func _score_candidate(unit: Node2D, enemy: Node2D, strategy: Dictionary, allies: Array[Node2D], enemies: Array[Node2D]) -> Dictionary:
	var dist := Vector2(unit.get("world_pos")).distance_to(Vector2(enemy.get("world_pos")))
	var max_hp := maxf(float(enemy.get("max_hp")), CombatData.EPSILON)
	var hp_ratio := float(enemy.get("hp")) / max_hp
	var attack_range := float(unit.get("attack_range"))
	var effective_range := CombatData.effective_attack_range(attack_range)
	var in_range := CombatData.is_melee_in_contact(dist, attack_range)
	var range_gap := maxf(0.0, dist - effective_range)
	var score := pow(range_gap / maxf(effective_range, CombatData.EPSILON), 1.5) * float(strategy.get("distance_weight", 1.0)) * 3.0
	if in_range:
		score -= float(strategy.get("in_range_bonus", 0.0))
	score += hp_ratio * float(strategy.get("hp_weight", 0.0)) * 10.0
	var priorities: Dictionary = strategy.get("role_priorities", {})
	score += float(priorities.get(String(enemy.get("role")), 0.0))
	if String(enemy.get("role")) == "tank":
		score += float(strategy.get("tank_penalty", 0.0))
	var enemy_target: Node2D = enemy.get("current_target")
	if enemy_target != null and is_instance_valid(enemy_target) and int(enemy_target.get("instance_id")) == int(unit.get("instance_id")):
		score -= float(strategy.get("threat_response_weight", 0.0)) / (1.0 + maxf(0.0, dist - attack_range) * 0.6)
	if bool(strategy.get("prefers_kiting", false)):
		var min_window := effective_range * 0.7
		var max_window := effective_range * 1.3
		if dist >= min_window and dist <= max_window:
			score -= float(strategy.get("distance_weight", 1.0)) * 0.5
	if unit.has_method("get_forced_target_id") and int(unit.call("get_forced_target_id")) == int(enemy.get("instance_id")):
		score += CombatData.TAUNT_SCORE_BONUS
	var is_current := int(unit.get("target_id")) == int(enemy.get("instance_id"))
	if is_current:
		score -= float(strategy.get("stickiness_bonus", CombatData.STICKINESS_DEFAULT))
	var ally_target: Node2D = unit.get("current_ally_target")
	if ally_target != null and is_instance_valid(ally_target) and int(ally_target.get("instance_id")) == int(enemy.get("instance_id")):
		score -= float(strategy.get("carry_peel_weight", 0.0))
	return {
		"target": enemy,
		"score": score,
		"distance": dist,
		"hp_ratio": hp_ratio,
		"in_range": in_range,
		"reason": _reason_string(unit, enemy, strategy, dist, hp_ratio, in_range),
	}


func _reason_string(unit: Node2D, enemy: Node2D, strategy: Dictionary, dist: float, hp_ratio: float, in_range: bool) -> String:
	var parts: Array[String] = []
	if in_range:
		parts.append("in range")
	if hp_ratio < CombatData.TARGET_WOUNDED_HP_RATIO:
		parts.append("wounded")
	if float(enemy.get("hp")) <= float(unit.get("attack_damage")):
		parts.append("killable")
	if bool(strategy.get("prefers_kiting", false)) and in_range:
		parts.append("kite window")
	var enemy_target: Node2D = enemy.get("current_target")
	if enemy_target != null and is_instance_valid(enemy_target) and int(enemy_target.get("instance_id")) == int(unit.get("instance_id")):
		parts.append("threatening me")
	if String(enemy.get("role")) == "tank":
		parts.append("tank")
	if parts.is_empty():
		return "target"
	return ", ".join(parts)
