extends RefCounted
class_name TargetingSystem

const CombatData = preload("res://scripts/combat_data.gd")

func get_strategy_for(unit: Node2D) -> Dictionary:
	var role := String(unit.get("role"))
	match role:
		"tank":
			return _make_strategy({
				"name": "Protector",
				"distance_weight": 1.5,
				"hp_weight": 0.0,
				"stickiness_bonus": 2.0,
				"prefers_kiting": false,
				"role_priorities": _scaled_priorities({"assassin": -5.0, "fighter": -2.0}),
				"tank_penalty": 0.4,
				"in_range_bonus": 1.0,
				"switch_margin": 0.75,
				"threat_response_weight": 2.0,
				"prey_instinct_weight": 0.0,
				"cluster_weight": 1.5,
				"spacing_weight": 0.0,
				"ally_role_priorities": _scaled_priorities({"marksman": -5.0, "mage": -5.0, "support": -3.0}),
				"ally_hp_weight": 0.0,
				"ally_distance_weight": 1.0,
				"ally_threat_weight": 25.0,
				"bodyguard_weight": 3.0,
				"execute_bonus_weight": 0.0,
				"projectile_time_weight": 0.0,
				"carry_peel_weight": 0.0,
				"obscurance_weight": 0.0,
				"flanking_weight": 0.0,
				"bucket_order": ["commit", "peel", "burst", "objective", "kite"],
			})
		"assassin":
			return _make_strategy({
				"name": "Diver",
				"distance_weight": 0.15,
				"hp_weight": 2.0,
				"stickiness_bonus": 2.0,
				"prefers_kiting": false,
				"role_priorities": _scaled_priorities({"marksman": -15.0, "mage": -15.0, "support": -10.0, "fighter": 10.0}),
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
				"obscurance_weight": 0.0,
				"flanking_weight": 2.0,
				"bucket_order": ["commit", "burst", "objective", "peel", "kite"],
			})
		"marksman":
			return _make_strategy({
				"name": "Kiter",
				"distance_weight": 1.0,
				"hp_weight": 0.5,
				"stickiness_bonus": 5.0,
				"prefers_kiting": true,
				"role_priorities": _scaled_priorities({}),
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
				"obscurance_weight": 4.5,
				"flanking_weight": 0.0,
				"bucket_order": ["commit", "peel", "burst", "objective", "kite"],
			})
		"fighter":
			return _make_strategy({
				"name": "Brawler",
				"distance_weight": 3.0,
				"hp_weight": 0.5,
				"stickiness_bonus": 2.0,
				"prefers_kiting": false,
				"role_priorities": _scaled_priorities({"marksman": -1.0, "mage": -1.0}),
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
				"obscurance_weight": 0.0,
				"flanking_weight": 0.0,
				"bucket_order": ["commit", "objective", "peel", "burst", "kite"],
			})
		"mage":
			return _make_strategy({
				"name": "Spellcaster",
				"distance_weight": 1.2,
				"hp_weight": 2.5,
				"stickiness_bonus": 2.0,
				"prefers_kiting": true,
				"role_priorities": _scaled_priorities({"marksman": -4.0, "support": -2.0}),
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
				"obscurance_weight": 4.5,
				"flanking_weight": 0.0,
				"bucket_order": ["commit", "peel", "objective", "burst", "kite"],
			})
		"support":
			return _make_strategy({
				"name": "Enchanter",
				"distance_weight": 1.5,
				"hp_weight": 0.0,
				"stickiness_bonus": 1.0,
				"prefers_kiting": true,
				"role_priorities": _scaled_priorities({"assassin": -8.0, "fighter": -4.0}),
				"tank_penalty": 1.5,
				"in_range_bonus": 0.4,
				"switch_margin": 0.75,
				"threat_response_weight": 1.7,
				"prey_instinct_weight": 0.0,
				"cluster_weight": 0.0,
				"spacing_weight": 1.0,
				"ally_role_priorities": _scaled_priorities({"marksman": -5.0, "mage": -5.0, "fighter": -2.0}),
				"ally_hp_weight": 5.0,
				"ally_distance_weight": 1.0,
				"ally_threat_weight": 25.0,
				"bodyguard_weight": 4.0,
				"execute_bonus_weight": 0.0,
				"projectile_time_weight": 0.3,
				"carry_peel_weight": 0.0,
				"obscurance_weight": 0.0,
				"flanking_weight": 0.0,
				"bucket_order": ["commit", "peel", "objective", "kite", "burst"],
			})
		_:
			return _make_strategy({
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
				"obscurance_weight": 0.0,
				"flanking_weight": 0.0,
				"bucket_order": ["objective", "burst", "peel", "kite", "commit"],
			})


func _scaled_priorities(priorities: Dictionary) -> Dictionary:
	var scaled: Dictionary = {}
	for key in priorities.keys():
		scaled[key] = float(priorities[key]) * CombatData.ROLE_PRIORITY_GLOBAL_SCALE
	return scaled


func select_ally(unit: Node2D, allies: Array[Node2D]) -> Dictionary:
	var strategy := get_strategy_for(unit)
	var alive_allies: Array[Node2D] = []
	for ally in allies:
		if bool(ally.get("alive")):
			alive_allies.append(ally)
	if alive_allies.is_empty():
		return {}

	var critical_allies: Array[Node2D] = []
	for ally in alive_allies:
		var max_hp := float(ally.get("max_hp"))
		if max_hp > CombatData.EPSILON:
			var hp_ratio := float(ally.get("hp")) / max_hp
			if hp_ratio <= CombatData.ALLY_CRITICAL_HP_RATIO:
				critical_allies.append(ally)

	var pool := critical_allies if not critical_allies.is_empty() else alive_allies
	var best: Dictionary = _score_ally_candidate(unit, pool[0], strategy)
	for ally in pool:
		var candidate := _score_ally_candidate(unit, ally, strategy)
		if _is_better_ally_candidate(candidate, best):
			best = candidate
	if not critical_allies.is_empty():
		best["reason"] = "%s, critical ally" % String(best.get("reason", "supportive"))
	return best


func select_target(unit: Node2D, enemies: Array[Node2D], allies: Array[Node2D] = []) -> Dictionary:
	var alive_enemies: Array[Node2D] = []
	for enemy in enemies:
		if bool(enemy.get("alive")):
			alive_enemies.append(enemy)
	if alive_enemies.is_empty():
		return {}

	var strategy := get_strategy_for(unit)
	var forced_candidate := _get_forced_target_candidate(unit, alive_enemies, allies)
	if not forced_candidate.is_empty():
		return forced_candidate
	var best_candidate := _score_candidate(unit, alive_enemies[0], strategy, allies, alive_enemies)
	for enemy in alive_enemies:
		var candidate := _score_candidate(unit, enemy, strategy, allies, alive_enemies)
		if _is_better_target_candidate(unit, candidate, best_candidate, strategy):
			best_candidate = candidate
	return best_candidate


func evaluate(unit: Node2D, enemies: Array[Node2D], allies: Array[Node2D] = []) -> Array[Dictionary]:
	var alive_enemies: Array[Node2D] = []
	for enemy in enemies:
		if bool(enemy.get("alive")):
			alive_enemies.append(enemy)
	if alive_enemies.is_empty():
		return []

	var strategy := get_strategy_for(unit)
	var forced_candidate := _get_forced_target_candidate(unit, alive_enemies, allies)
	if not forced_candidate.is_empty():
		return [forced_candidate]

	var candidates: Array[Dictionary] = []
	for enemy in alive_enemies:
		candidates.append(_score_candidate(unit, enemy, strategy, allies, alive_enemies))
	_sort_target_candidates(unit, candidates, strategy)
	return candidates


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
		var commit_window := minf(CombatData.SWITCH_COMMIT_WINDOW_SECONDS, swing * CombatData.SWITCH_COMMIT_WINDOW_SWING_FRACTION)
		if float(unit.get("attack_cooldown_remaining")) <= commit_window:
			return false
	var margin := float(strategy.get("switch_margin", CombatData.TARGET_SWITCH_MARGIN))
	return new_score <= (current_score - margin)


func _score_ally(unit: Node2D, ally: Node2D, strategy: Dictionary) -> float:
	var dist := unit.global_position.distance_to(ally.global_position)
	var max_hp := maxf(float(ally.get("max_hp")), CombatData.EPSILON)
	var hp_ratio := float(ally.get("hp")) / max_hp
	var score := dist * float(strategy.get("ally_distance_weight", 1.0))
	score += hp_ratio * float(strategy.get("ally_hp_weight", 0.0)) * 10.0
	score -= float(ally.get("perceived_threat")) * float(strategy.get("ally_threat_weight", 0.0))
	var ally_roles: Dictionary = strategy.get("ally_role_priorities", {})
	score += float(ally_roles.get(String(ally.get("role")), 0.0))
	return score


func _score_ally_candidate(unit: Node2D, ally: Node2D, strategy: Dictionary) -> Dictionary:
	var dist := unit.global_position.distance_to(ally.global_position)
	var max_hp := maxf(float(ally.get("max_hp")), CombatData.EPSILON)
	var hp_ratio := float(ally.get("hp")) / max_hp
	var score := _score_ally(unit, ally, strategy)
	return {
		"target": ally,
		"score": score,
		"distance": dist,
		"hp_ratio": hp_ratio,
		"bucket": "objective",
		"reason": _ally_reason_string(ally),
	}


func _score_candidate(unit: Node2D, enemy: Node2D, strategy: Dictionary, allies: Array[Node2D], enemies: Array[Node2D]) -> Dictionary:
	var dist := unit.global_position.distance_to(enemy.global_position)
	var max_hp := maxf(float(enemy.get("max_hp")), CombatData.EPSILON)
	var hp_ratio := float(enemy.get("hp")) / max_hp
	var attack_range := float(unit.get("attack_range"))
	var ranged_threshold := float(unit.get("combat_ranged_threshold"))
	var contact_buffer := float(unit.get("combat_contact_buffer"))
	var effective_range := CombatData.effective_attack_range(attack_range, ranged_threshold, contact_buffer)
	var in_range := CombatData.is_melee_in_contact(dist, attack_range, ranged_threshold, contact_buffer)
	var range_gap := maxf(0.0, dist - effective_range)
	var score := _score_distance(unit, enemy, strategy, dist, in_range, effective_range)
	score += _score_environmental_tempo(unit, enemy, strategy, dist, effective_range)
	score += _score_vitality(unit, enemy, strategy, hp_ratio, in_range)
	score += _score_role_context(unit, enemy, strategy, enemies)
	score += _score_threat_response(unit, enemy, strategy, dist)
	score += _score_environmental_factors(unit, enemy, strategy, allies, enemies)
	score += _score_support_and_peel(unit, enemy, strategy, allies)
	score += _score_advanced_tactics(unit, enemy, strategy, dist, enemies)
	if _is_forced_target(unit, enemy):
		score += CombatData.TAUNT_SCORE_BONUS
	var is_current := int(unit.get("target_id")) == int(enemy.get("instance_id"))
	if is_current:
		var stick_scale := maxf(1.0, float(strategy.get("distance_weight", 1.0)))
		score -= float(strategy.get("stickiness_bonus", CombatData.STICKINESS_DEFAULT)) * stick_scale
	var bucket := _classify_bucket(unit, enemy, strategy, dist, hp_ratio, in_range, effective_range)
	return {
		"target": enemy,
		"score": score,
		"distance": dist,
		"hp_ratio": hp_ratio,
		"in_range": in_range,
		"bucket": bucket,
		"reason": _reason_string(unit, enemy, strategy, dist, hp_ratio, in_range, bucket, is_current),
	}


func _score_distance(unit: Node2D, enemy: Node2D, strategy: Dictionary, dist: float, in_range: bool, effective_range: float) -> float:
	var safe_range := maxf(effective_range, CombatData.EPSILON)
	var range_gap := maxf(0.0, dist - safe_range)
	var normalized_gap := range_gap / safe_range
	var score := pow(normalized_gap, CombatData.DISTANCE_EXPONENT) * float(strategy.get("distance_weight", 1.0)) * CombatData.SCORE_DISTANCE_WEIGHT_SCALE
	if in_range:
		score -= float(strategy.get("in_range_bonus", 0.0))
	if unit.stats.attack_range > CombatData.RANGED_THRESHOLD and unit.stats.projectile_speed > CombatData.EPSILON:
		score += (dist / maxf(unit.stats.projectile_speed, CombatData.EPSILON)) * float(strategy.get("projectile_time_weight", 0.0))
	return score


func _score_environmental_tempo(unit: Node2D, enemy: Node2D, strategy: Dictionary, dist: float, effective_range: float) -> float:
	var score := 0.0
	if bool(strategy.get("prefers_kiting", false)):
		var min_window := effective_range * CombatData.KITE_TARGET_WINDOW_MIN_FACTOR
		var max_window := effective_range * CombatData.KITE_TARGET_WINDOW_MAX_FACTOR
		if dist >= min_window and dist <= max_window and max_window > min_window:
			var kite_ratio := (dist - min_window) / (max_window - min_window)
			score -= kite_ratio * CombatData.SCORE_KITING_WEIGHT_SCALE
	if unit.stats.attack_range > CombatData.RANGED_THRESHOLD and unit.stats.projectile_speed > CombatData.EPSILON:
		var time_to_hit := dist / unit.stats.projectile_speed
		score += time_to_hit * float(strategy.get("projectile_time_weight", 0.0))
	return score


func _score_vitality(unit: Node2D, enemy: Node2D, strategy: Dictionary, hp_ratio: float, in_range: bool) -> float:
	var score := hp_ratio * float(strategy.get("hp_weight", 0.0)) * CombatData.SCORE_HP_WEIGHT_SCALE
	if float(strategy.get("execute_bonus_weight", 0.0)) > 0.0 and in_range and hp_ratio <= CombatData.TARGET_EXECUTE_HP_RATIO:
		score -= float(strategy.get("execute_bonus_weight", 0.0))
	return score


func _score_role_context(unit: Node2D, enemy: Node2D, strategy: Dictionary, enemies: Array[Node2D]) -> float:
	var score := 0.0
	score += float(strategy.get("role_priorities", {}).get(String(enemy.get("role")), 0.0))
	if String(enemy.get("role")) == "tank":
		score += float(strategy.get("tank_penalty", 0.0))
		if String(unit.get("role")) == "assassin":
			for other in enemies:
				if not is_instance_valid(other) or not bool(other.get("alive")):
					continue
				if int(other.get("instance_id")) == int(enemy.get("instance_id")):
					continue
				if String(other.get("role")) in ["marksman", "mage", "support"]:
					score += CombatData.ASSASSIN_TANK_CONTEXT_PENALTY
					break
	return score


func _score_threat_response(unit: Node2D, enemy: Node2D, strategy: Dictionary, dist: float) -> float:
	var enemy_target: Node2D = enemy.get("current_target")
	if enemy_target != null and is_instance_valid(enemy_target) and int(enemy_target.get("instance_id")) == int(unit.get("instance_id")):
		var falloff := 1.0 / (1.0 + maxf(0.0, dist - float(unit.get("attack_range"))) * CombatData.THREAT_RESPONSE_RANGE_FALLOFF)
		return -(float(strategy.get("threat_response_weight", 0.0)) * falloff)
	return 0.0


func _score_environmental_factors(unit: Node2D, enemy: Node2D, strategy: Dictionary, allies: Array[Node2D], enemies: Array[Node2D]) -> float:
	var score := 0.0
	var cluster_weight := float(strategy.get("cluster_weight", 0.0))
	if cluster_weight > 0.0:
		score -= float(_calculate_density(enemy, enemies)) * cluster_weight

	var prey_weight := float(strategy.get("prey_instinct_weight", 0.0))
	if prey_weight > 0.0:
		var prey_focus := float(enemy.get("incoming_target_count")) * CombatData.PREY_INCOMING_TARGET_SCALE
		prey_focus += float(enemy.get("perceived_threat")) * CombatData.PREY_PERCEIVED_THREAT_SCALE
		if String(enemy.get("role")) in ["tank", "fighter"]:
			prey_focus *= CombatData.PREY_FRONTLINE_SCALE
		score -= prey_focus * prey_weight

	var spacing_weight := float(strategy.get("spacing_weight", 0.0))
	if spacing_weight > 0.0:
		score += pow(maxf(0.0, float(enemy.get("incoming_target_count"))), CombatData.SPACING_EXPONENT) * spacing_weight

	return score


func _score_support_and_peel(unit: Node2D, enemy: Node2D, strategy: Dictionary, allies: Array[Node2D]) -> float:
	var score := 0.0
	var carry_peel_weight := float(strategy.get("carry_peel_weight", 0.0))
	var ally_target: Node2D = unit.get("current_ally_target")
	if String(unit.get("role")) in ["marksman", "mage"] and carry_peel_weight > 0.0:
		if ally_target != null and is_instance_valid(ally_target) and int(ally_target.get("instance_id")) == int(enemy.get("instance_id")):
			var falloff := 1.0 / (1.0 + maxf(0.0, enemy.global_position.distance_to(unit.global_position) - float(unit.get("attack_range"))) * CombatData.THREAT_RESPONSE_RANGE_FALLOFF)
			score -= carry_peel_weight * falloff

	if String(unit.get("role")) == "support" and ally_target != null and is_instance_valid(ally_target):
		var ally_under_threat := _is_under_threat(ally_target)
		if ally_under_threat:
			var enemy_target: Node2D = enemy.get("current_target")
			if enemy_target != null and is_instance_valid(enemy_target) and int(enemy_target.get("instance_id")) == int(ally_target.get("instance_id")):
				score -= CombatData.SUPPORT_PEEL_BOOST

	var bodyguard_weight := float(strategy.get("bodyguard_weight", 0.0))
	if bodyguard_weight > 0.0:
		for ally in allies:
			if not is_instance_valid(ally) or not bool(ally.get("alive")):
				continue
			if String(ally.get("role")) not in ["marksman", "mage"]:
				continue
			var ally_dist := enemy.global_position.distance_to(ally.global_position)
			if ally_dist < CombatData.BODYGUARD_RADIUS:
				var guard_bonus := (1.0 - (ally_dist / CombatData.BODYGUARD_RADIUS)) * bodyguard_weight
				score -= guard_bonus

	return score


func _score_advanced_tactics(unit: Node2D, enemy: Node2D, strategy: Dictionary, dist: float, enemies: Array[Node2D]) -> float:
	var score := 0.0
	var obscurance_weight := float(strategy.get("obscurance_weight", 0.0))
	if obscurance_weight > 0.0:
		var blockage_count := 0
		for other_enemy in enemies:
			if not is_instance_valid(other_enemy) or not bool(other_enemy.get("alive")):
				continue
			if int(other_enemy.get("instance_id")) == int(enemy.get("instance_id")):
				continue
			if String(other_enemy.get("role")) not in ["tank", "fighter"]:
				continue
			if other_enemy.global_position.distance_to(unit.global_position) >= dist:
				continue
			var line_dist_sq := _point_segment_distance_sq(other_enemy.global_position, unit.global_position, enemy.global_position)
			if line_dist_sq <= CombatData.OBSCURANCE_LINE_RADIUS * CombatData.OBSCURANCE_LINE_RADIUS:
				blockage_count += 1
		score += blockage_count * obscurance_weight

	var flanking_weight := float(strategy.get("flanking_weight", 0.0))
	if flanking_weight > 0.0:
		var alive_enemies: Array[Node2D] = []
		for other_enemy in enemies:
			if is_instance_valid(other_enemy) and bool(other_enemy.get("alive")):
				alive_enemies.append(other_enemy)
		if not alive_enemies.is_empty():
			var center := Vector2.ZERO
			for other_enemy in alive_enemies:
				center += other_enemy.global_position
			center /= float(alive_enemies.size())
			var to_target := enemy.global_position - center
			var to_attacker := unit.global_position - enemy.global_position
			if to_target.length_squared() > CombatData.EPSILON and to_attacker.length_squared() > CombatData.EPSILON:
				var flank_alignment := maxf(0.0, to_target.normalized().dot(to_attacker.normalized()))
				var isolation_scale := minf(1.0, enemy.global_position.distance_to(center) * CombatData.FLANKING_TEAM_CENTER_SCALE)
				score -= flank_alignment * isolation_scale * flanking_weight

	return score


func _classify_bucket(unit: Node2D, enemy: Node2D, strategy: Dictionary, dist: float, hp_ratio: float, in_range: bool, effective_range: float) -> String:
	if _is_forced_target(unit, enemy):
		return "commit"
	if float(strategy.get("carry_peel_weight", 0.0)) > 0.0 and enemy.get("current_target") != null and int(enemy.get("current_target").get("instance_id")) == int(unit.get("instance_id")):
		return "peel"
	var ally_target: Node2D = unit.get("current_ally_target")
	if ally_target != null and is_instance_valid(ally_target):
		var ally_under_threat := _is_under_threat(ally_target)
		if ally_under_threat and enemy.get("current_target") != null and int(enemy.get("current_target").get("instance_id")) == int(ally_target.get("instance_id")):
			return "peel"
	if float(strategy.get("execute_bonus_weight", 0.0)) > 0.0 and (hp_ratio <= CombatData.TARGET_EXECUTE_HP_RATIO or float(enemy.get("hp")) <= float(unit.get("attack_damage"))):
		return "burst"
	if bool(strategy.get("prefers_kiting", false)) and not _is_under_threat(unit):
		var min_window := effective_range * CombatData.KITE_TARGET_WINDOW_MIN_FACTOR
		var max_window := effective_range * CombatData.KITE_TARGET_WINDOW_MAX_FACTOR
		if dist >= min_window and dist <= max_window:
			return "kite"
	return "objective"


func _reason_string(unit: Node2D, enemy: Node2D, strategy: Dictionary, dist: float, hp_ratio: float, in_range: bool, bucket: String, is_current: bool) -> String:
	var parts: Array[String] = []
	if is_current:
		parts.append("sticky")
	if _is_forced_target(unit, enemy):
		parts.append("forced")
	parts.append(bucket)
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


func _calculate_density(target: Node2D, enemies: Array[Node2D], radius: float = CombatData.AOE_DENSITY_RADIUS) -> int:
	var count := 0
	var radius_sq := radius * radius
	for enemy in enemies:
		if not is_instance_valid(enemy) or not bool(enemy.get("alive")):
			continue
		if int(enemy.get("instance_id")) == int(target.get("instance_id")):
			continue
		if target.global_position.distance_squared_to(enemy.global_position) <= radius_sq:
			count += 1
	return count


func _point_segment_distance_sq(point: Vector2, start: Vector2, end: Vector2) -> float:
	var segment := end - start
	var seg_len_sq := segment.length_squared()
	if seg_len_sq <= CombatData.EPSILON:
		return point.distance_squared_to(start)
	var rel := point - start
	var dot := rel.dot(segment)
	var t := clampf(dot / seg_len_sq, 0.0, 1.0)
	var projection := start + segment * t
	return point.distance_squared_to(projection)


func _is_forced_target(unit: Node2D, enemy: Node2D) -> bool:
	return unit.has_method("get_forced_target_id") and int(unit.call("get_forced_target_id")) == int(enemy.get("instance_id"))


func _is_under_threat(unit: Node2D) -> bool:
	return float(unit.get("incoming_target_count")) >= CombatData.SUPPORT_PEEL_THREAT_THRESHOLD or float(unit.get("perceived_threat")) >= CombatData.SUPPORT_PEEL_THREAT_THRESHOLD


func _bucket_rank(strategy: Dictionary, bucket: String) -> int:
	var order: Array = strategy.get("bucket_order", ["objective", "burst", "peel", "kite", "commit"])
	var index := order.find(bucket)
	return index if index >= 0 else order.size()


func _bucket_margin_for(strategy: Dictionary) -> float:
	return float(strategy.get("bucket_margin", CombatData.TARGET_BUCKET_MARGIN))


func _is_better_target_candidate(unit: Node2D, candidate: Dictionary, current: Dictionary, strategy: Dictionary) -> bool:
	if current.is_empty():
		return true
	var candidate_key := _candidate_sort_key(candidate, strategy)
	var current_key := _candidate_sort_key(current, strategy)
	if not is_equal_approx(float(candidate_key[0]), float(current_key[0])):
		return float(candidate_key[0]) < float(current_key[0])
	if not is_equal_approx(float(candidate_key[1]), float(current_key[1])):
		return float(candidate_key[1]) < float(current_key[1])
	if int(candidate_key[2]) != int(current_key[2]):
		return int(candidate_key[2]) < int(current_key[2])
	if not is_equal_approx(float(candidate_key[3]), float(current_key[3])):
		return float(candidate_key[3]) < float(current_key[3])
	return int(candidate_key[4]) < int(current_key[4])


func _sort_target_candidates(unit: Node2D, candidates: Array[Dictionary], strategy: Dictionary) -> void:
	for i in range(candidates.size()):
		var best_index := i
		for j in range(i + 1, candidates.size()):
			if _is_better_target_candidate(unit, candidates[j], candidates[best_index], strategy):
				best_index = j
		if best_index != i:
			var tmp: Dictionary = candidates[i]
			candidates[i] = candidates[best_index]
			candidates[best_index] = tmp


func _candidate_sort_key(candidate: Dictionary, strategy: Dictionary) -> Array:
	var bucket_rank := _bucket_rank(strategy, String(candidate.get("bucket", "objective")))
	var adjusted_score := float(candidate.get("score", 0.0)) + (float(bucket_rank) * _bucket_margin_for(strategy))
	var target: Node2D = candidate.get("target")
	var instance_id := -1 if target == null else int(target.get("instance_id"))
	return [
		adjusted_score,
		float(candidate.get("score", 0.0)),
		bucket_rank,
		float(candidate.get("distance", 0.0)),
		instance_id,
	]


func _is_better_ally_candidate(candidate: Dictionary, current: Dictionary) -> bool:
	if current.is_empty():
		return true
	var candidate_hp_ratio := float(candidate.get("hp_ratio", 1.0))
	var current_hp_ratio := float(current.get("hp_ratio", 1.0))
	if not is_equal_approx(candidate_hp_ratio, current_hp_ratio):
		return candidate_hp_ratio < current_hp_ratio
	var candidate_score := float(candidate.get("score", 0.0))
	var current_score := float(current.get("score", 0.0))
	if not is_equal_approx(candidate_score, current_score):
		return candidate_score < current_score
	var candidate_dist := float(candidate.get("distance", 0.0))
	var current_dist := float(current.get("distance", 0.0))
	if not is_equal_approx(candidate_dist, current_dist):
		return candidate_dist < current_dist
	return int(candidate.get("target").get("instance_id")) < int(current.get("target").get("instance_id"))


func _ally_reason_string(ally: Node2D) -> String:
	var reason := "supportive"
	if float(ally.get("perceived_threat")) > 0.0:
		reason += " (threat: %.1f)" % float(ally.get("perceived_threat"))
	return reason


func _get_forced_target_candidate(unit: Node2D, enemies: Array[Node2D], allies: Array[Node2D]) -> Dictionary:
	var forced_id := int(unit.call("get_forced_target_id"))
	if forced_id < 0:
		return {}
	for enemy in enemies:
		if not is_instance_valid(enemy) or not bool(enemy.get("alive")):
			continue
		if int(enemy.get("instance_id")) != forced_id:
			continue
		return _score_candidate(unit, enemy, get_strategy_for(unit), allies, enemies)
	return {}


func _make_strategy(base: Dictionary) -> Dictionary:
	var strategy := {
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
		"obscurance_weight": 0.0,
		"flanking_weight": 0.0,
		"bucket_order": ["objective", "burst", "peel", "kite", "commit"],
	}
	for key in base.keys():
		strategy[key] = base[key]
	return strategy
