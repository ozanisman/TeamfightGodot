#include "batch_match_engine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace teamfight {

void BatchMatchEngine::set_seed(int32_t seed) {
	seed_ = seed;
}

void BatchMatchEngine::set_units(const std::vector<UnitState> &units) {
	units_ = units;
	projectiles_.clear();
	effects_.clear();
	targets_.clear();
	time_ = 0.0f;
	ticks_ = 0;
	finished_ = false;
	termination_.clear();
	winner_.clear();
	player_kills_ = 0;
	enemy_kills_ = 0;

	for (size_t i = 0; i < units_.size(); ++i) {
		UnitState &unit = units_[i];
		unit.instance_id = static_cast<int32_t>(i + 1);
		unit.alive = true;
		unit.hp = std::max(0.0f, unit.max_hp);
		unit.shield = 0.0f;
		unit.target_id = -1;
		unit.in_range = false;
		unit.kills = 0;
		unit.deaths = 0;
		unit.assists = 0;
		unit.damage_dealt = 0.0f;
		unit.damage_dealt_auto = 0.0f;
		unit.damage_dealt_ability = 0.0f;
		unit.damage_dealt_ultimate = 0.0f;
		unit.damage_received = 0.0f;
		unit.damage_mitigated = 0.0f;
		unit.healing_done = 0.0f;
		unit.shielding_done = 0.0f;
		unit.auto_attacks_done = 0;
		unit.abilities_used = 0;
		unit.ultimates_used = 0;
		unit.stuns_dealt = 0;
		unit.mana = 0.0f;
		unit.ability_cooldown_remaining = std::max(0.0f, unit.ability_cd);
		unit.ultimate_cooldown_remaining = std::max(0.0f, unit.ultimate_cd);
		unit.attack_cooldown_remaining = 0.0f;
		unit.stun_remaining = 0.0f;
		unit.current_target_id = -1;
		unit.retarget_timer = 0.0f;
		unit.target_switch_lock_timer = 0.0f;
		unit.last_target_score = 0.0f;
	}
}

void BatchMatchEngine::step(float dt) {
	if (finished_) {
		return;
	}
	if (dt <= 0.0f) {
		dt = 0.0f;
	}
	time_ += dt;
	++ticks_;
	resolve_step(dt);
}

MatchResult BatchMatchEngine::get_match_result() const {
	MatchResult result;
	result.termination = termination_;
	result.winner = winner_;
	result.player_kills = player_kills_;
	result.enemy_kills = enemy_kills_;
	result.time = time_;
	result.ticks = ticks_;
	return result;
}

std::vector<UnitState> BatchMatchEngine::snapshot_units() const {
	return units_;
}

void BatchMatchEngine::clear() {
	units_.clear();
	projectiles_.clear();
	effects_.clear();
	targets_.clear();
	time_ = 0.0f;
	ticks_ = 0;
	finished_ = false;
	termination_.clear();
	winner_.clear();
	player_kills_ = 0;
	enemy_kills_ = 0;
}

void BatchMatchEngine::resolve_step(float dt) {
	int32_t alive_player_count = 0;
	int32_t alive_enemy_count = 0;

	for (const UnitState &unit : units_) {
		if (!unit.alive) {
			continue;
		}
		if (unit.team == "player") {
			++alive_player_count;
		} else if (unit.team == "enemy") {
			++alive_enemy_count;
		}
	}

	if (alive_player_count == 0 || alive_enemy_count == 0) {
		termination_ = "winner";
		if (alive_player_count == 0 && alive_enemy_count == 0) {
			winner_ = "draw";
		} else if (alive_player_count == 0) {
			winner_ = "enemy";
		} else {
			winner_ = "player";
		}
		finished_ = true;
		return;
	}

	for (UnitState &unit : units_) {
		if (unit.alive) {
			unit.attack_cooldown_remaining = std::max(0.0f, unit.attack_cooldown_remaining - dt);
			unit.ability_cooldown_remaining = std::max(0.0f, unit.ability_cooldown_remaining - dt);
			unit.ultimate_cooldown_remaining = std::max(0.0f, unit.ultimate_cooldown_remaining - dt);
			unit.stun_remaining = std::max(0.0f, unit.stun_remaining - dt);
			unit.retarget_timer = std::max(0.0f, unit.retarget_timer - dt);
			unit.target_switch_lock_timer = std::max(0.0f, unit.target_switch_lock_timer - dt);
		}
	}

	for (size_t i = 0; i < units_.size(); ++i) {
		UnitState &unit = units_[i];
		if (!unit.alive) {
			continue;
		}

		const int32_t target_index = select_target_index(static_cast<int32_t>(i));
		if (target_index < 0) {
			continue;
		}

		UnitState &target = units_[static_cast<size_t>(target_index)];
		const float dx = target.position_x - unit.position_x;
		const float dy = target.position_y - unit.position_y;
		const float distance = std::sqrt(dx * dx + dy * dy);
		unit.target_id = target.instance_id;
		unit.current_target_id = target.instance_id;
		unit.in_range = distance <= std::max(0.0f, unit.attack_range);

		if (unit.stun_remaining > 0.0f) {
			continue;
		}

		if (unit.in_range && unit.attack_cooldown_remaining <= 0.0f) {
			perform_attack(unit, target);
		} else {
			move_toward_target(unit, target, dt);
		}
	}

	alive_player_count = 0;
	alive_enemy_count = 0;
	for (const UnitState &unit : units_) {
		if (!unit.alive) {
			continue;
		}
		if (unit.team == "player") {
			++alive_player_count;
		} else if (unit.team == "enemy") {
			++alive_enemy_count;
		}
	}

	if (alive_player_count == 0 || alive_enemy_count == 0) {
		termination_ = "winner";
		if (alive_player_count == 0 && alive_enemy_count == 0) {
			winner_ = "draw";
		} else if (alive_player_count == 0) {
			winner_ = "enemy";
		} else {
			winner_ = "player";
		}
		finished_ = true;
		return;
	}

	if (time_ >= MATCH_DURATION) {
		finish_with_timeout();
	}
}

int32_t BatchMatchEngine::find_nearest_enemy_index(int32_t unit_index) const {
	if (unit_index < 0 || unit_index >= static_cast<int32_t>(units_.size())) {
		return -1;
	}

	const UnitState &unit = units_[static_cast<size_t>(unit_index)];
	int32_t best_index = -1;
	float best_distance = std::numeric_limits<float>::infinity();
	for (size_t i = 0; i < units_.size(); ++i) {
		const UnitState &candidate = units_[i];
		if (!candidate.alive || candidate.team == unit.team) {
			continue;
		}
		const float dx = candidate.position_x - unit.position_x;
		const float dy = candidate.position_y - unit.position_y;
		const float distance = std::sqrt(dx * dx + dy * dy);
		if (distance < best_distance || (std::abs(distance - best_distance) <= 1.0e-6f && (best_index < 0 || candidate.instance_id < units_[static_cast<size_t>(best_index)].instance_id))) {
			best_distance = distance;
			best_index = static_cast<int32_t>(i);
		}
	}
	return best_index;
}

int32_t BatchMatchEngine::select_target_index(int32_t unit_index) const {
	if (unit_index < 0 || unit_index >= static_cast<int32_t>(units_.size())) {
		return -1;
	}

	const UnitState &unit = units_[static_cast<size_t>(unit_index)];
	int32_t best_index = -1;
	float best_score = std::numeric_limits<float>::infinity();
	for (size_t i = 0; i < units_.size(); ++i) {
		const UnitState &enemy = units_[i];
		if (!enemy.alive || enemy.team == unit.team) {
			continue;
		}
		const float score = score_target(unit, enemy);
		if (score < best_score || (std::abs(score - best_score) <= 1.0e-6f && (best_index < 0 || enemy.instance_id < units_[static_cast<size_t>(best_index)].instance_id))) {
			best_score = score;
			best_index = static_cast<int32_t>(i);
		}
	}
	return best_index;
}

float BatchMatchEngine::score_target(const UnitState &unit, const UnitState &enemy) const {
	const float dx = enemy.position_x - unit.position_x;
	const float dy = enemy.position_y - unit.position_y;
	const float dist = std::sqrt(dx * dx + dy * dy);
	const float max_hp = std::max(1.0f, enemy.max_hp);
	const float hp_ratio = enemy.hp / max_hp;
	float score = dist * distance_weight_for_role(unit.role);
	score += hp_ratio * hp_weight_for_role(unit.role) * 10.0f;
	score += tank_penalty_for_role(unit.role) * (enemy.role == "tank" ? 1.0f : 0.0f);
	score -= in_range_bonus_for_role(unit.role) * (dist <= unit.attack_range ? 1.0f : 0.0f);
	if (unit.current_target_id == enemy.instance_id) {
		score -= stickiness_for_role(unit.role);
	}
	if (enemy.target_id == unit.instance_id) {
		score -= 1.0f;
	}
	return score;
}

float BatchMatchEngine::distance_weight_for_role(const std::string &role) const {
	if (role == "tank") {
		return 1.5f;
	}
	if (role == "assassin") {
		return 0.15f;
	}
	if (role == "marksman") {
		return 1.0f;
	}
	if (role == "fighter") {
		return 3.0f;
	}
	if (role == "mage") {
		return 1.2f;
	}
	if (role == "support") {
		return 1.5f;
	}
	return 1.0f;
}

float BatchMatchEngine::hp_weight_for_role(const std::string &role) const {
	if (role == "assassin") {
		return 2.0f;
	}
	if (role == "mage") {
		return 2.5f;
	}
	if (role == "support") {
		return 5.0f;
	}
	return 0.5f;
}

float BatchMatchEngine::stickiness_for_role(const std::string &role) const {
	if (role == "marksman") {
		return 5.0f;
	}
	if (role == "support") {
		return 1.0f;
	}
	return 2.0f;
}

float BatchMatchEngine::in_range_bonus_for_role(const std::string &role) const {
	if (role == "tank") {
		return 1.0f;
	}
	if (role == "assassin") {
		return 0.6f;
	}
	if (role == "marksman") {
		return 0.35f;
	}
	if (role == "fighter") {
		return 0.9f;
	}
	if (role == "mage") {
		return 0.45f;
	}
	if (role == "support") {
		return 0.4f;
	}
	return 0.6f;
}

float BatchMatchEngine::tank_penalty_for_role(const std::string &role) const {
	if (role == "tank") {
		return 0.4f;
	}
	if (role == "assassin") {
		return 7.0f;
	}
	if (role == "marksman") {
		return 2.6f;
	}
	if (role == "fighter") {
		return 1.0f;
	}
	if (role == "mage") {
		return 2.3f;
	}
	if (role == "support") {
		return 1.5f;
	}
	return 0.0f;
}

void BatchMatchEngine::move_toward_target(UnitState &unit, const UnitState &target, float dt) {
	const float dx = target.position_x - unit.position_x;
	const float dy = target.position_y - unit.position_y;
	const float distance = std::sqrt(dx * dx + dy * dy);
	const float speed = std::max(0.0f, unit.move_speed);
	const float travel = std::max(0.0f, speed * dt);
	if (distance <= 1.0e-6f || travel <= 0.0f) {
		return;
	}

	const float actual = std::min(travel, distance);
	const float ratio = actual / distance;
	unit.position_x += dx * ratio;
	unit.position_y += dy * ratio;
}

void BatchMatchEngine::perform_attack(UnitState &attacker, UnitState &target) {
	const float raw_damage = std::max(0.0f, attacker.attack_damage);
	const float mitigated = apply_armor(raw_damage, target.armor);
	attacker.damage_dealt += mitigated;
	attacker.damage_dealt_auto += mitigated;
	attacker.auto_attacks_done += 1;
	attacker.attack_cooldown_remaining = attacker.attack_speed > 0.0f ? (1.0f / attacker.attack_speed) : 1.0f;
	if (attacker.mana_per_attack > 0.0f && attacker.max_mana > 0.0f) {
		attacker.mana = std::min(attacker.max_mana, attacker.mana + attacker.mana_per_attack);
	}
	if (attacker.life_steal > 0.0f) {
		attacker.hp = std::min(attacker.max_hp, attacker.hp + mitigated * attacker.life_steal);
	}

	target.damage_received += mitigated;
	target.damage_mitigated += std::max(0.0f, raw_damage - mitigated);

	float remaining = mitigated;
	if (target.shield > 0.0f) {
		const float shield_damage = std::min(target.shield, remaining);
		target.shield -= shield_damage;
		remaining -= shield_damage;
	}

	if (remaining > 0.0f) {
		target.hp -= remaining;
		if (target.hp <= 0.0f) {
			target.hp = 0.0f;
			target.alive = false;
			++target.deaths;
			if (attacker.team == "player") {
				++player_kills_;
			} else {
				++enemy_kills_;
			}
		}
	}

	if (attacker.max_mana > 0.0f && attacker.mana >= attacker.max_mana && attacker.ultimate_cooldown_remaining <= 0.0f) {
		perform_ability(attacker, target, true);
	} else if (attacker.ability_cd > 0.0f && attacker.ability_cooldown_remaining <= 0.0f) {
		perform_ability(attacker, target, false);
	}
}

void BatchMatchEngine::perform_ability(UnitState &attacker, UnitState &target, bool is_ultimate) {
	const float multiplier = is_ultimate ? 1.5f : 0.75f;
	const float raw_damage = std::max(0.0f, attacker.attack_damage * multiplier);
	const float mitigated = apply_armor(raw_damage, target.armor);
	if (is_ultimate) {
		attacker.ultimates_used += 1;
		attacker.ultimate_cooldown_remaining = attacker.ultimate_cd;
		attacker.mana = 0.0f;
		attacker.damage_dealt_ultimate += mitigated;
	} else {
		attacker.abilities_used += 1;
		attacker.ability_cooldown_remaining = attacker.ability_cd;
		attacker.damage_dealt_ability += mitigated;
	}
	attacker.damage_dealt += mitigated;
	target.damage_received += mitigated;
	target.damage_mitigated += std::max(0.0f, raw_damage - mitigated);
	if (is_ultimate) {
		target.stun_remaining = std::max(target.stun_remaining, 0.5f);
		++attacker.stuns_dealt;
	}
	if (attacker.life_steal > 0.0f) {
		attacker.hp = std::min(attacker.max_hp, attacker.hp + mitigated * attacker.life_steal);
	}
}

void BatchMatchEngine::finish_with_timeout() {
	termination_ = "timeout";
	winner_ = "timeout";
	finished_ = true;
}

float BatchMatchEngine::apply_armor(float damage, float armor) {
	const float mitigation = std::clamp(armor, 0.0f, 75.0f) / 100.0f;
	return damage * (1.0f - mitigation);
}

} // namespace teamfight
