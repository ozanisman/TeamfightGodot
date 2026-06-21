#include "sim_targeting_strategies.hpp"

namespace sim {
namespace targeting {

namespace {

inline const StringName &sn_tank() {
	static const StringName s("tank");
	return s;
}
inline const StringName &sn_fighter() {
	static const StringName s("fighter");
	return s;
}
inline const StringName &sn_assassin() {
	static const StringName s("assassin");
	return s;
}
inline const StringName &sn_marksman() {
	static const StringName s("marksman");
	return s;
}
inline const StringName &sn_mage() {
	static const StringName s("mage");
	return s;
}
inline const StringName &sn_support() {
	static const StringName s("support");
	return s;
}

StrategyRolePriorities get_tank_role_priorities() {
	StrategyRolePriorities p;
	p.enemy_priorities[0] = {"assassin", -5.0};
	p.enemy_priorities[1] = {"fighter", -2.0};
	p.enemy_priorities[2] = {"tank", 0.4};
	p.ally_priorities[0] = {"marksman", -5.0};
	p.ally_priorities[1] = {"mage", -5.0};
	p.ally_priorities[2] = {"support", -3.0};
	return p;
}

StrategyRolePriorities get_assassin_role_priorities() {
	StrategyRolePriorities p;
	p.enemy_priorities[0] = {"marksman", -15.0};
	p.enemy_priorities[1] = {"mage", -15.0};
	p.enemy_priorities[2] = {"support", -10.0};
	p.enemy_priorities[3] = {"fighter", 10.0};
	p.enemy_priorities[4] = {"tank", 20.0};
	return p;
}

StrategyRolePriorities get_fighter_role_priorities() {
	StrategyRolePriorities p;
	p.enemy_priorities[0] = {"marksman", -1.0};
	p.enemy_priorities[1] = {"mage", -1.0};
	p.enemy_priorities[2] = {"tank", 1.0};
	return p;
}

StrategyRolePriorities get_marksman_role_priorities() {
	StrategyRolePriorities p;
	p.enemy_priorities[0] = {"tank", 1};
	return p;
}

StrategyRolePriorities get_mage_role_priorities() {
	StrategyRolePriorities p;
	p.enemy_priorities[0] = {"marksman", -4.0};
	p.enemy_priorities[1] = {"support", -2.0};
	p.enemy_priorities[2] = {"tank", 2};
	return p;
}

StrategyRolePriorities get_support_role_priorities() {
	StrategyRolePriorities p;
	p.enemy_priorities[0] = {"assassin", -8.0};
	p.enemy_priorities[1] = {"fighter", -4.0};
	p.enemy_priorities[2] = {"tank", 1.5};
	p.ally_priorities[0] = {"marksman", -5.0};
	p.ally_priorities[1] = {"mage", -5.0};
	p.ally_priorities[2] = {"fighter", -2.0};
	return p;
}

StrategyConfig get_tank_config() {
	StrategyConfig c;
	c.display_name = "Protector";
	c.role_name = "tank";
	c.distance_weight = 1.5;
	c.hp_weight = 0.0;
	c.ally_distance_weight = 1.0;
	c.ally_hp_weight = 0.0;
	c.ally_threat_weight = 25.0;
	c.in_range_bonus = 1.0;
	c.threat_response_weight = 2.0;
	c.enemy_threat_weight = 2.5;
	c.execute_bonus_weight = 0.0;
	c.spacing_weight = 0.0;
	c.threat_decay_rate = 4.0;
	c.switch_margin = 2.0;
	c.prefers_kiting = false;
	c.uses_ally_targeting = true;
	c.role_priorities = get_tank_role_priorities();
	return c;
}

StrategyConfig get_assassin_config() {
	StrategyConfig c;
	c.display_name = "Diver";
	c.role_name = "assassin";
	c.distance_weight = 0.01;
	c.hp_weight = 2.0;
	c.ally_distance_weight = 1.0;
	c.ally_hp_weight = 0.0;
	c.ally_threat_weight = 0.0;
	c.in_range_bonus = 0.6;
	c.threat_response_weight = 0.8;
	c.enemy_threat_weight = 0.5;
	c.execute_bonus_weight = 10.0;
	c.spacing_weight = 0.0;
	c.threat_decay_rate = 1.0;
	c.switch_margin = 2.0;
	c.prefers_kiting = false;
	c.uses_ally_targeting = false;
	c.role_priorities = get_assassin_role_priorities();
	return c;
}

StrategyConfig get_fighter_config() {
	StrategyConfig c;
	c.display_name = "Brawler";
	c.role_name = "fighter";
	c.distance_weight = 3.0;
	c.hp_weight = 0.5;
	c.ally_distance_weight = 1.0;
	c.ally_hp_weight = 0.0;
	c.ally_threat_weight = 0.0;
	c.in_range_bonus = 0.9;
	c.threat_response_weight = 1.8;
	c.enemy_threat_weight = 2.0;
	c.execute_bonus_weight = 20.0;
	c.spacing_weight = 0.0;
	c.threat_decay_rate = 4.5;
	c.switch_margin = 2.0;
	c.prefers_kiting = false;
	c.uses_ally_targeting = false;
	c.role_priorities = get_fighter_role_priorities();
	return c;
}

StrategyConfig get_marksman_config() {
	StrategyConfig c;
	c.display_name = "Kiter";
	c.role_name = "marksman";
	c.distance_weight = 1.0;
	c.hp_weight = 0.5;
	c.ally_distance_weight = 1.0;
	c.ally_hp_weight = 0.0;
	c.ally_threat_weight = 0.0;
	c.in_range_bonus = 0.35;
	c.threat_response_weight = 61.0;
	c.enemy_threat_weight = 1.0;
	c.execute_bonus_weight = 20.0;
	c.spacing_weight = 0.0; // TODO: revisit focus-fire vs spread (was 2.5)
	c.threat_decay_rate = 1.0;
	c.switch_margin = 2.0;
	c.prefers_kiting = true;
	c.uses_ally_targeting = false;
	c.role_priorities = get_marksman_role_priorities();
	return c;
}

StrategyConfig get_mage_config() {
	StrategyConfig c;
	c.display_name = "Spellcaster";
	c.role_name = "mage";
	c.distance_weight = 1.2;
	c.hp_weight = 2.5;
	c.ally_distance_weight = 1.0;
	c.ally_hp_weight = 0.0;
	c.ally_threat_weight = 0.0;
	c.in_range_bonus = 0.45;
	c.threat_response_weight = 61.1;
	c.enemy_threat_weight = 1.5;
	c.execute_bonus_weight = 20.0;
	c.spacing_weight = 0.0; // TODO: revisit focus-fire vs spread (was 1.5)
	c.threat_decay_rate = 1.0;
	c.switch_margin = 2.0;
	c.prefers_kiting = true;
	c.uses_ally_targeting = false;
	c.role_priorities = get_mage_role_priorities();
	return c;
}

StrategyConfig get_support_config() {
	StrategyConfig c;
	c.display_name = "Enchanter";
	c.role_name = "support";
	c.distance_weight = 1.5;
	c.hp_weight = 0.0;
	c.ally_distance_weight = 0.0;
	c.ally_hp_weight = 10.0;
	c.ally_threat_weight = 25.0;
	c.in_range_bonus = 0.4;
	c.threat_response_weight = 1.7;
	c.enemy_threat_weight = 0.25;
	c.execute_bonus_weight = 0.0;
	c.spacing_weight = 0.0; // TODO: revisit focus-fire vs spread (was 1.0)
	c.threat_decay_rate = 1.0;
	c.switch_margin = 2.0;
	c.prefers_kiting = true;
	c.uses_ally_targeting = true;
	c.role_priorities = get_support_role_priorities();
	return c;
}

void apply_role_priorities(std::array<double, ROLE_SLOT_COUNT> &slots, const StrategyRolePriorities &prio_config) {
	for (const auto &prio : prio_config.enemy_priorities) {
		if (prio.role != StringName()) {
			int64_t slot = role_slot_for_name(prio.role);
			if (slot >= 0) {
				slots[static_cast<size_t>(slot)] = prio.priority;
			}
		}
	}
}

void apply_ally_role_priorities(std::array<double, ROLE_SLOT_COUNT> &slots, const StrategyRolePriorities &prio_config) {
	for (const auto &prio : prio_config.ally_priorities) {
		if (prio.role != StringName()) {
			int64_t slot = role_slot_for_name(prio.role);
			if (slot >= 0) {
				slots[static_cast<size_t>(slot)] = prio.priority;
			}
		}
	}
}

void apply_strategy_config(UnitStrategy &s, const StrategyConfig &config) {
	s.display_name = config.display_name;
	s.distance_weight = config.distance_weight;
	s.hp_weight = config.hp_weight;
	s.ally_distance_weight = config.ally_distance_weight;
	s.ally_hp_weight = config.ally_hp_weight;
	s.ally_threat_weight = config.ally_threat_weight;
	s.in_range_bonus = config.in_range_bonus;
	s.threat_response_weight = config.threat_response_weight;
	s.enemy_threat_weight = config.enemy_threat_weight;
	s.execute_bonus_weight = config.execute_bonus_weight;
	s.spacing_weight = config.spacing_weight;
	s.threat_decay_rate = config.threat_decay_rate;
	s.switch_margin = config.switch_margin;
	s.prefers_kiting = config.prefers_kiting;
	s.uses_ally_targeting = config.uses_ally_targeting;
	apply_role_priorities(s.role_priorities, config.role_priorities);
	apply_ally_role_priorities(s.ally_role_priorities, config.role_priorities);
}

} // namespace

int64_t role_slot_for_name(const StringName &role) {
	if (role == sn_tank()) {
		return ROLE_SLOT_TANK;
	}
	if (role == sn_fighter()) {
		return ROLE_SLOT_FIGHTER;
	}
	if (role == sn_assassin()) {
		return ROLE_SLOT_ASSASSIN;
	}
	if (role == sn_marksman()) {
		return ROLE_SLOT_MARKSMAN;
	}
	if (role == sn_mage()) {
		return ROLE_SLOT_MAGE;
	}
	if (role == sn_support()) {
		return ROLE_SLOT_SUPPORT;
	}
	return -1;
}

void build_role_strategy_cache(std::array<UnitStrategy, ROLE_SLOT_COUNT> &cache_by_slot, UnitStrategy &default_strategy) {
	cache_by_slot.fill(UnitStrategy());
	auto put = [&](const StringName &role, UnitStrategy s) {
		int64_t slot = role_slot_for_name(role);
		if (slot >= 0) {
			cache_by_slot[static_cast<size_t>(slot)] = std::move(s);
		}
	};

	{
		UnitStrategy s;
		apply_strategy_config(s, get_tank_config());
		put(StringName("tank"), std::move(s));
	}
	{
		UnitStrategy s;
		apply_strategy_config(s, get_assassin_config());
		put(StringName("assassin"), std::move(s));
	}
	{
		UnitStrategy s;
		apply_strategy_config(s, get_marksman_config());
		put(StringName("marksman"), std::move(s));
	}
	{
		UnitStrategy s;
		apply_strategy_config(s, get_fighter_config());
		put(StringName("fighter"), std::move(s));
	}
	{
		UnitStrategy s;
		apply_strategy_config(s, get_mage_config());
		put(StringName("mage"), std::move(s));
	}
	{
		UnitStrategy s;
		apply_strategy_config(s, get_support_config());
		put(StringName("support"), std::move(s));
	}

	default_strategy = UnitStrategy();
	default_strategy.display_name = String("Default");
	default_strategy.distance_weight = 1.0;
	default_strategy.hp_weight = 0.0;
	default_strategy.ally_distance_weight = 1.0;
	default_strategy.ally_hp_weight = 0.0;
	default_strategy.ally_threat_weight = 0.0;
	default_strategy.in_range_bonus = 0.6;
	default_strategy.threat_response_weight = 0.0;
	default_strategy.enemy_threat_weight = 1.0;
	default_strategy.execute_bonus_weight = 0.0;
	default_strategy.threat_decay_rate = 2.0;
	default_strategy.switch_margin = 0.75;
}

const UnitStrategy &strategy_for_unit(
		const UnitState &unit,
		const std::array<UnitStrategy, ROLE_SLOT_COUNT> &cache_by_slot,
		const UnitStrategy &default_strategy) {
	int64_t slot = unit.role_slot;
	if (slot >= 0 && slot < ROLE_SLOT_COUNT) {
		return cache_by_slot[static_cast<size_t>(slot)];
	}
	return default_strategy;
}

} // namespace targeting
} // namespace sim
