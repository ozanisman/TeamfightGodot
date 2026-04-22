extends RefCounted
class_name TargetingSystem

const CombatData = preload("res://scripts/combat_data.gd")
const CombatUnitStateScript = preload("res://scripts/combat_unit_state.gd")
const TargetCandidateScript = preload("res://scripts/target_candidate.gd")

func get_strategy_for(unit: Object) -> Dictionary:
	var role := _unit_role(unit)
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


func select_ally(unit: Object, allies: Array) -> Object:
	var strategy := get_strategy_for(unit)
	var alive_allies: Array = []
	for ally in allies:
		if _unit_is_alive(ally):
			alive_allies.append(ally)
	if alive_allies.is_empty():
		return null

	var critical_allies: Array = []
	for ally in alive_allies:
		var max_hp := _unit_max_hp(ally)
		if max_hp > CombatData.EPSILON:
			var hp_ratio := _unit_hp(ally) / max_hp
			if hp_ratio <= CombatData.ALLY_CRITICAL_HP_RATIO:
				critical_allies.append(ally)

	var pool := critical_allies if not critical_allies.is_empty() else alive_allies
	var best: Object = _score_ally_candidate(unit, pool[0], strategy)
	for ally in pool:
		var candidate := _score_ally_candidate(unit, ally, strategy)
		if _is_better_ally_candidate(candidate, best):
			best = candidate
	if not critical_allies.is_empty():
		best.reason = "%s, critical ally" % String(best.reason if best.reason != "" else "supportive")
	return best


func select_target(unit: Object, enemies: Array, allies: Array = []) -> Object:
	var alive_enemies: Array = []
	for enemy in enemies:
		if _unit_is_alive(enemy):
			alive_enemies.append(enemy)
	if alive_enemies.is_empty():
		return null

	var strategy := get_strategy_for(unit)
	var forced_candidate := _get_forced_target_candidate(unit, alive_enemies, allies)
	if forced_candidate != null:
		return forced_candidate
	var best_candidate := _score_candidate(unit, alive_enemies[0], strategy, allies, alive_enemies)
	for enemy in alive_enemies:
		var candidate := _score_candidate(unit, enemy, strategy, allies, alive_enemies)
		if _is_better_target_candidate(unit, candidate, best_candidate, strategy):
			best_candidate = candidate
	return best_candidate


func evaluate(unit: Object, enemies: Array, allies: Array = []) -> Array:
	var alive_enemies: Array = []
	for enemy in enemies:
		if _unit_is_alive(enemy):
			alive_enemies.append(enemy)
	if alive_enemies.is_empty():
		return []

	var strategy := get_strategy_for(unit)
	var forced_candidate := _get_forced_target_candidate(unit, alive_enemies, allies)
	if forced_candidate != null:
		return [forced_candidate]

	var candidates: Array = []
	for enemy in alive_enemies:
		candidates.append(_score_candidate(unit, enemy, strategy, allies, alive_enemies))
	_sort_target_candidates(unit, candidates, strategy)
	return candidates


func score_target(unit: Object, enemy: Object, strategy: Dictionary, allies: Array = [], enemies: Array = [], include_reason: bool = true) -> Object:
	return _score_candidate(unit, enemy, strategy, allies, enemies, include_reason)


func should_switch(current_score: float, new_score: float, strategy: Dictionary, unit: Object) -> bool:
	if unit == null:
		return true
	if _unit_target_switch_lock_timer(unit) > 0.0:
		return false
	if _unit_in_range(unit):
		var attack_speed := maxf(0.001, _unit_attack_speed(unit))
		var swing := 1.0 / attack_speed
		var commit_window := minf(CombatData.SWITCH_COMMIT_WINDOW_SECONDS, swing * CombatData.SWITCH_COMMIT_WINDOW_SWING_FRACTION)
		if _unit_attack_cooldown_remaining(unit) <= commit_window:
			return false
	var margin := float(strategy.get("switch_margin", CombatData.TARGET_SWITCH_MARGIN))
	return new_score <= (current_score - margin)


func _score_ally(unit: Object, ally: Object, strategy: Dictionary) -> float:
	var ally_distance_weight := float(strategy.get("ally_distance_weight", 1.0))
	var ally_hp_weight := float(strategy.get("ally_hp_weight", 0.0))
	var ally_threat_weight := float(strategy.get("ally_threat_weight", 0.0))
	var ally_roles: Dictionary = strategy.get("ally_role_priorities", {})
	var dist: float = _unit_position(unit).distance_to(_unit_position(ally))
	var max_hp := maxf(_unit_max_hp(ally), CombatData.EPSILON)
	var hp_ratio := _unit_hp(ally) / max_hp
	var score: float = dist * ally_distance_weight
	score += hp_ratio * ally_hp_weight * 10.0
	score -= _unit_perceived_threat(ally) * ally_threat_weight
	score += float(ally_roles.get(_unit_role(ally), 0.0))
	return score


func _score_ally_candidate(unit: Object, ally: Object, strategy: Dictionary) -> Object:
	var dist: float = _unit_position(unit).distance_to(_unit_position(ally))
	var max_hp := maxf(_unit_max_hp(ally), CombatData.EPSILON)
	var hp_ratio := _unit_hp(ally) / max_hp
	var score: float = _score_ally(unit, ally, strategy)
	var candidate := TargetCandidateScript.new()
	candidate.target = ally
	candidate.score = score
	candidate.distance = dist
	candidate.hp_ratio = hp_ratio
	candidate.bucket = "objective"
	candidate.reason = _ally_reason_string(ally)
	return candidate


func _score_candidate(unit: Object, enemy: Object, strategy: Dictionary, allies: Array, enemies: Array, include_reason: bool = true) -> Object:
	var distance_weight := float(strategy.get("distance_weight", 1.0))
	var in_range_bonus := float(strategy.get("in_range_bonus", 0.0))
	var hp_weight := float(strategy.get("hp_weight", 0.0))
	var execute_bonus_weight := float(strategy.get("execute_bonus_weight", 0.0))
	var threat_response_weight := float(strategy.get("threat_response_weight", 0.0))
	var role_priorities: Dictionary = strategy.get("role_priorities", {})
	var tank_penalty := float(strategy.get("tank_penalty", 0.0))
	var carry_peel_weight := float(strategy.get("carry_peel_weight", 0.0))
	var bodyguard_weight := float(strategy.get("bodyguard_weight", 0.0))
	var cluster_weight := float(strategy.get("cluster_weight", 0.0))
	var prey_instinct_weight := float(strategy.get("prey_instinct_weight", 0.0))
	var spacing_weight := float(strategy.get("spacing_weight", 0.0))
	var obscurance_weight := float(strategy.get("obscurance_weight", 0.0))
	var flanking_weight := float(strategy.get("flanking_weight", 0.0))
	var prefers_kiting := bool(strategy.get("prefers_kiting", false))
	var projectile_time_weight := float(strategy.get("projectile_time_weight", 0.0))
	var dist: float = _unit_position(unit).distance_to(_unit_position(enemy))
	var max_hp := maxf(_unit_max_hp(enemy), CombatData.EPSILON)
	var hp_ratio := _unit_hp(enemy) / max_hp
	var attack_range := _unit_attack_range(unit)
	var ranged_threshold := _unit_combat_ranged_threshold(unit)
	var contact_buffer := _unit_combat_contact_buffer(unit)
	var effective_range := CombatData.effective_attack_range(attack_range, ranged_threshold, contact_buffer)
	var in_range := CombatData.is_melee_in_contact(dist, attack_range, ranged_threshold, contact_buffer)
	var range_gap := maxf(0.0, dist - effective_range)
	var score: float = _score_distance(unit, enemy, distance_weight, in_range_bonus, dist, in_range, effective_range)
	score += _score_environmental_tempo(unit, enemy, prefers_kiting, projectile_time_weight, dist, effective_range)
	score += _score_vitality(unit, enemy, hp_weight, execute_bonus_weight, hp_ratio, in_range)
	score += _score_role_context(unit, enemy, role_priorities, tank_penalty, enemies)
	score += _score_threat_response(unit, enemy, threat_response_weight, dist)
	score += _score_environmental_factors(unit, enemy, cluster_weight, prey_instinct_weight, spacing_weight, allies, enemies)
	score += _score_support_and_peel(unit, enemy, carry_peel_weight, bodyguard_weight, allies)
	score += _score_advanced_tactics(unit, enemy, obscurance_weight, flanking_weight, dist, enemies)
	if _is_forced_target(unit, enemy):
		score += CombatData.TAUNT_SCORE_BONUS
	var is_current := _unit_target_id(unit) == _unit_instance_id(enemy)
	if is_current:
		var stickiness_bonus := float(strategy.get("stickiness_bonus", CombatData.STICKINESS_DEFAULT))
		var stick_scale := maxf(1.0, distance_weight)
		score -= stickiness_bonus * stick_scale
	var bucket := _classify_bucket(unit, enemy, strategy, dist, hp_ratio, in_range, effective_range)
	var candidate := TargetCandidateScript.new()
	candidate.target = enemy
	candidate.score = score
	candidate.distance = dist
	candidate.hp_ratio = hp_ratio
	candidate.in_range = in_range
	candidate.bucket = bucket
	candidate.reason = _reason_string(unit, enemy, strategy, dist, hp_ratio, in_range, bucket, is_current) if include_reason else ""
	return candidate


func _score_distance(unit: Object, enemy: Object, distance_weight: float, in_range_bonus: float, dist: float, in_range: bool, effective_range: float) -> float:
	var safe_range := maxf(effective_range, CombatData.EPSILON)
	var range_gap := maxf(0.0, dist - safe_range)
	var normalized_gap := range_gap / safe_range
	var score := pow(normalized_gap, CombatData.DISTANCE_EXPONENT) * distance_weight * CombatData.SCORE_DISTANCE_WEIGHT_SCALE
	if in_range:
		score -= in_range_bonus
	return score


func _score_environmental_tempo(unit: Object, enemy: Object, prefers_kiting: bool, projectile_time_weight: float, dist: float, effective_range: float) -> float:
	var score := 0.0
	if prefers_kiting:
		var min_window := effective_range * CombatData.KITE_TARGET_WINDOW_MIN_FACTOR
		var max_window := effective_range * CombatData.KITE_TARGET_WINDOW_MAX_FACTOR
		if dist >= min_window and dist <= max_window and max_window > min_window:
			var kite_ratio := (dist - min_window) / (max_window - min_window)
			score -= kite_ratio * CombatData.SCORE_KITING_WEIGHT_SCALE
	var attack_range := _unit_attack_range(unit)
	var projectile_speed := _unit_projectile_speed(unit)
	if attack_range > CombatData.RANGED_THRESHOLD and projectile_speed > CombatData.EPSILON:
		var time_to_hit: float = dist / projectile_speed
		score += time_to_hit * projectile_time_weight
	return score


func _score_vitality(unit: Object, enemy: Object, hp_weight: float, execute_bonus_weight: float, hp_ratio: float, in_range: bool) -> float:
	var score := hp_ratio * hp_weight * CombatData.SCORE_HP_WEIGHT_SCALE
	if execute_bonus_weight > 0.0 and in_range and hp_ratio <= CombatData.TARGET_EXECUTE_HP_RATIO:
		score -= execute_bonus_weight
	return score


func _score_role_context(unit: Object, enemy: Object, role_priorities: Dictionary, tank_penalty: float, enemies: Array) -> float:
	var score := 0.0
	score += float(role_priorities.get(_unit_role(enemy), 0.0))
	if _unit_role(enemy) == "tank":
		score += tank_penalty
		if _unit_role(unit) == "assassin":
			for other in enemies:
				if not is_instance_valid(other) or not _unit_is_alive(other):
					continue
				if _unit_instance_id(other) == _unit_instance_id(enemy):
					continue
				if _unit_role(other) in ["marksman", "mage", "support"]:
					score += CombatData.ASSASSIN_TANK_CONTEXT_PENALTY
					break
	return score


func _score_threat_response(unit: Object, enemy: Object, threat_response_weight: float, dist: float) -> float:
	var enemy_target: Object = _unit_current_target(enemy)
	if enemy_target != null and is_instance_valid(enemy_target) and _unit_instance_id(enemy_target) == _unit_instance_id(unit):
		var falloff := 1.0 / (1.0 + maxf(0.0, dist - _unit_attack_range(unit)) * CombatData.THREAT_RESPONSE_RANGE_FALLOFF)
		return -(threat_response_weight * falloff)
	return 0.0


func _score_environmental_factors(unit: Object, enemy: Object, cluster_weight: float, prey_instinct_weight: float, spacing_weight: float, allies: Array, enemies: Array) -> float:
	var score := 0.0
	if cluster_weight > 0.0:
		score -= float(_calculate_density(enemy, enemies)) * cluster_weight

	if prey_instinct_weight > 0.0:
		var prey_focus := float(_unit_incoming_target_count(enemy)) * CombatData.PREY_INCOMING_TARGET_SCALE
		prey_focus += _unit_perceived_threat(enemy) * CombatData.PREY_PERCEIVED_THREAT_SCALE
		if _unit_role(enemy) in ["tank", "fighter"]:
			prey_focus *= CombatData.PREY_FRONTLINE_SCALE
		score -= prey_focus * prey_instinct_weight

	if spacing_weight > 0.0:
		var incoming_target_count: int = _unit_incoming_target_count(enemy)
		score += pow(maxf(0.0, incoming_target_count), CombatData.SPACING_EXPONENT) * spacing_weight

	return score


func _score_support_and_peel(unit: Object, enemy: Object, carry_peel_weight: float, bodyguard_weight: float, allies: Array) -> float:
	var score := 0.0
	var ally_target: Object = _unit_current_ally_target(unit)
	if _unit_role(unit) in ["marksman", "mage"] and carry_peel_weight > 0.0:
		if ally_target != null and is_instance_valid(ally_target) and _unit_instance_id(ally_target) == _unit_instance_id(enemy):
			var falloff := 1.0 / (1.0 + maxf(0.0, _unit_position(enemy).distance_to(_unit_position(unit)) - _unit_attack_range(unit)) * CombatData.THREAT_RESPONSE_RANGE_FALLOFF)
			score -= carry_peel_weight * falloff

	if _unit_role(unit) == "support" and ally_target != null and is_instance_valid(ally_target):
		var ally_under_threat := _is_under_threat(ally_target)
		if ally_under_threat:
			var enemy_target: Object = _unit_current_target(enemy)
			if enemy_target != null and is_instance_valid(enemy_target) and _unit_instance_id(enemy_target) == _unit_instance_id(ally_target):
				score -= CombatData.SUPPORT_PEEL_BOOST

	if bodyguard_weight > 0.0:
		for ally in allies:
			if not is_instance_valid(ally) or not _unit_is_alive(ally):
				continue
			if _unit_role(ally) not in ["marksman", "mage"]:
				continue
			var ally_dist: float = _unit_position(enemy).distance_to(_unit_position(ally))
			if ally_dist < CombatData.BODYGUARD_RADIUS:
				var guard_bonus: float = (1.0 - (ally_dist / CombatData.BODYGUARD_RADIUS)) * bodyguard_weight
				score -= guard_bonus

	return score


func _score_advanced_tactics(unit: Object, enemy: Object, obscurance_weight: float, flanking_weight: float, dist: float, enemies: Array) -> float:
	var score := 0.0
	if obscurance_weight > 0.0:
		var blockage_count := 0
		for other_enemy in enemies:
			if not is_instance_valid(other_enemy) or not _unit_is_alive(other_enemy):
				continue
			if _unit_instance_id(other_enemy) == _unit_instance_id(enemy):
				continue
			if _unit_role(other_enemy) not in ["tank", "fighter"]:
				continue
			if _unit_position(other_enemy).distance_to(_unit_position(unit)) >= dist:
				continue
			var line_dist_sq := _point_segment_distance_sq(_unit_position(other_enemy), _unit_position(unit), _unit_position(enemy))
			if line_dist_sq <= CombatData.OBSCURANCE_LINE_RADIUS * CombatData.OBSCURANCE_LINE_RADIUS:
				blockage_count += 1
		score += blockage_count * obscurance_weight

	if flanking_weight > 0.0:
		var alive_enemies: Array = []
		for other_enemy in enemies:
			if is_instance_valid(other_enemy) and _unit_is_alive(other_enemy):
				alive_enemies.append(other_enemy)
		if not alive_enemies.is_empty():
			var center := Vector2.ZERO
			for other_enemy in alive_enemies:
				center += _unit_position(other_enemy)
			center /= float(alive_enemies.size())
			var to_target: Vector2 = _unit_position(enemy) - center
			var to_attacker: Vector2 = _unit_position(unit) - _unit_position(enemy)
			if to_target.length_squared() > CombatData.EPSILON and to_attacker.length_squared() > CombatData.EPSILON:
				var flank_alignment := maxf(0.0, to_target.normalized().dot(to_attacker.normalized()))
				var isolation_scale := minf(1.0, _unit_position(enemy).distance_to(center) * CombatData.FLANKING_TEAM_CENTER_SCALE)
				score -= flank_alignment * isolation_scale * flanking_weight

	return score


func _classify_bucket(unit: Object, enemy: Object, strategy: Dictionary, dist: float, hp_ratio: float, in_range: bool, effective_range: float) -> String:
	if _is_forced_target(unit, enemy):
		return "commit"
	if float(strategy.get("carry_peel_weight", 0.0)) > 0.0 and _unit_current_target(enemy) != null and _unit_instance_id(_unit_current_target(enemy)) == _unit_instance_id(unit):
		return "peel"
	var ally_target: Object = _unit_current_ally_target(unit)
	if ally_target != null and is_instance_valid(ally_target):
		var ally_under_threat := _is_under_threat(ally_target)
		if ally_under_threat and _unit_current_target(enemy) != null and _unit_instance_id(_unit_current_target(enemy)) == _unit_instance_id(ally_target):
			return "peel"
	if float(strategy.get("execute_bonus_weight", 0.0)) > 0.0 and (hp_ratio <= CombatData.TARGET_EXECUTE_HP_RATIO or _unit_hp(enemy) <= _unit_attack_damage(unit)):
		return "burst"
	if bool(strategy.get("prefers_kiting", false)) and not _is_under_threat(unit):
		var min_window := effective_range * CombatData.KITE_TARGET_WINDOW_MIN_FACTOR
		var max_window := effective_range * CombatData.KITE_TARGET_WINDOW_MAX_FACTOR
		if dist >= min_window and dist <= max_window:
			return "kite"
	return "objective"


func _reason_string(unit: Object, enemy: Object, strategy: Dictionary, dist: float, hp_ratio: float, in_range: bool, bucket: String, is_current: bool) -> String:
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
	if _unit_hp(enemy) <= _unit_attack_damage(unit):
		parts.append("killable")
	if bool(strategy.get("prefers_kiting", false)) and in_range:
		parts.append("kite window")
	var enemy_target: Object = _unit_current_target(enemy)
	if enemy_target != null and is_instance_valid(enemy_target) and _unit_instance_id(enemy_target) == _unit_instance_id(unit):
		parts.append("threatening me")
	if _unit_role(enemy) == "tank":
		parts.append("tank")
	if parts.is_empty():
		return "target"
	return ", ".join(parts)


func _calculate_density(target: Object, enemies: Array, radius: float = CombatData.AOE_DENSITY_RADIUS) -> int:
	var count := 0
	var radius_sq := radius * radius
	for enemy in enemies:
		if not is_instance_valid(enemy) or not _unit_is_alive(enemy):
			continue
		if _unit_instance_id(enemy) == _unit_instance_id(target):
			continue
		if _unit_position(target).distance_squared_to(_unit_position(enemy)) <= radius_sq:
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


func _is_forced_target(unit: Object, enemy: Object) -> bool:
	return _unit_forced_target_id(unit) == _unit_instance_id(enemy)


func _is_under_threat(unit: Object) -> bool:
	return float(_unit_incoming_target_count(unit)) >= CombatData.SUPPORT_PEEL_THREAT_THRESHOLD or _unit_perceived_threat(unit) >= CombatData.SUPPORT_PEEL_THREAT_THRESHOLD


func _bucket_rank(strategy: Dictionary, bucket: String) -> int:
	var order: Array = strategy.get("bucket_order", ["objective", "burst", "peel", "kite", "commit"])
	var index := order.find(bucket)
	return index if index >= 0 else order.size()


func _bucket_margin_for(strategy: Dictionary) -> float:
	return float(strategy.get("bucket_margin", CombatData.TARGET_BUCKET_MARGIN))


func _is_better_target_candidate(unit: Object, candidate: Object, current: Object, strategy: Dictionary) -> bool:
	if current == null:
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


func _sort_target_candidates(unit: Object, candidates: Array, strategy: Dictionary) -> void:
	for i in range(candidates.size()):
		var best_index := i
		for j in range(i + 1, candidates.size()):
			if _is_better_target_candidate(unit, candidates[j], candidates[best_index], strategy):
				best_index = j
		if best_index != i:
			var tmp: Dictionary = candidates[i]
			candidates[i] = candidates[best_index]
			candidates[best_index] = tmp


func _candidate_sort_key(candidate: Object, strategy: Dictionary) -> Array:
	var bucket_rank := _bucket_rank(strategy, String(candidate.bucket))
	var adjusted_score := float(candidate.score) + (float(bucket_rank) * _bucket_margin_for(strategy))
	var target: Object = candidate.target
	var instance_id := -1 if target == null else _unit_instance_id(target)
	return [
		adjusted_score,
		float(candidate.score),
		bucket_rank,
		float(candidate.distance),
		instance_id,
	]


func _is_better_ally_candidate(candidate: Object, current: Object) -> bool:
	if current == null:
		return true
	var candidate_hp_ratio := float(candidate.hp_ratio)
	var current_hp_ratio := float(current.hp_ratio)
	if not is_equal_approx(candidate_hp_ratio, current_hp_ratio):
		return candidate_hp_ratio < current_hp_ratio
	var candidate_score := float(candidate.score)
	var current_score := float(current.score)
	if not is_equal_approx(candidate_score, current_score):
		return candidate_score < current_score
	var candidate_dist := float(candidate.distance)
	var current_dist := float(current.distance)
	if not is_equal_approx(candidate_dist, current_dist):
		return candidate_dist < current_dist
	return _unit_instance_id(candidate.target) < _unit_instance_id(current.target)


func _ally_reason_string(ally: Object) -> String:
	var reason := "supportive"
	if _unit_perceived_threat(ally) > 0.0:
		reason += " (threat: %.1f)" % _unit_perceived_threat(ally)
	return reason


func _get_forced_target_candidate(unit: Object, enemies: Array, allies: Array) -> Object:
	var forced_id := _unit_forced_target_id(unit)
	if forced_id < 0:
		return null
	for enemy in enemies:
		if not is_instance_valid(enemy) or not _unit_is_alive(enemy):
			continue
		if _unit_instance_id(enemy) != forced_id:
			continue
		return _score_candidate(unit, enemy, get_strategy_for(unit), allies, enemies)
	return null


func _unit_state(unit: Object) -> Object:
	if unit != null and unit.get_script() == CombatUnitStateScript:
		return unit
	return null


func _unit_role(unit: Object) -> String:
	var state := _unit_state(unit)
	if state != null:
		return state.role
	return String(unit.get("role"))


func _unit_is_alive(unit: Object) -> bool:
	var state := _unit_state(unit)
	if state != null:
		return state.is_alive()
	if unit != null and unit.has_method("is_alive"):
		return bool(unit.call("is_alive"))
	return false


func _unit_max_hp(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.max_hp
	return float(unit.get("max_hp"))


func _unit_hp(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.hp
	return float(unit.get("hp"))


func _unit_attack_range(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.attack_range
	return float(unit.get("attack_range"))


func _unit_projectile_speed(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.projectile_speed
	return float(unit.get("projectile_speed"))


func _unit_combat_ranged_threshold(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.combat_ranged_threshold
	return float(unit.get("combat_ranged_threshold"))


func _unit_combat_contact_buffer(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.combat_contact_buffer
	return float(unit.get("combat_contact_buffer"))


func _unit_target_switch_lock_timer(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.target_switch_lock_timer
	return float(unit.get("target_switch_lock_timer"))


func _unit_in_range(unit: Object) -> bool:
	var state := _unit_state(unit)
	if state != null:
		return state.in_range
	return bool(unit.get("in_range"))


func _unit_attack_speed(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.attack_speed
	return float(unit.get("attack_speed"))


func _unit_attack_cooldown_remaining(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.attack_cooldown_remaining
	return float(unit.get("attack_cooldown_remaining"))


func _unit_current_target(unit: Object) -> Object:
	var state := _unit_state(unit)
	if state != null:
		return state.current_target
	return unit.get("current_target")


func _unit_current_ally_target(unit: Object) -> Object:
	var state := _unit_state(unit)
	if state != null:
		return state.current_ally_target
	return unit.get("current_ally_target")


func _unit_instance_id(unit: Object) -> int:
	var state := _unit_state(unit)
	if state != null:
		return state.instance_id
	return int(unit.get("instance_id"))


func _unit_position(unit: Object) -> Vector2:
	var state := _unit_state(unit)
	if state != null:
		return state.global_position
	return unit.global_position


func _unit_perceived_threat(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.perceived_threat
	return float(unit.get("perceived_threat"))


func _unit_incoming_target_count(unit: Object) -> int:
	var state := _unit_state(unit)
	if state != null:
		return state.incoming_target_count
	return int(unit.get("incoming_target_count"))


func _unit_forced_target_id(unit: Object) -> int:
	var state := _unit_state(unit)
	if state != null:
		return state.forced_target_id
	if unit != null and unit.has_method("get_forced_target_id"):
		return int(unit.call("get_forced_target_id"))
	return -1


func _unit_target_id(unit: Object) -> int:
	var state := _unit_state(unit)
	if state != null:
		return state.target_id
	return int(unit.get("target_id"))


func _unit_attack_damage(unit: Object) -> float:
	var state := _unit_state(unit)
	if state != null:
		return state.attack_damage
	return float(unit.get("attack_damage"))


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
