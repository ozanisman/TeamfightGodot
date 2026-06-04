#include "sim_unit_builder.hpp"

#include "sim_constants.hpp"
#include "sim_stats.hpp"
#include "sim_targeting.hpp"
#include "sim_targeting_strategies.hpp"
#include "stat_definitions.hpp"

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <vector>

namespace sim {
namespace unit_builder {
namespace {

constexpr size_t EFFECT_BUCKET_ON_TICK = 3;

inline const StringName &sn_player() {
	static const StringName s("player");
	return s;
}
inline const StringName &sn_fighter() {
	static const StringName s("fighter");
	return s;
}
inline const StringName &sn_tank() {
	static const StringName s("tank");
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
inline const StringName &sn_on_attack() {
	static const StringName s("on_attack");
	return s;
}
inline const StringName &sn_on_defense() {
	static const StringName s("on_defense");
	return s;
}
inline const StringName &sn_on_ally_defense() {
	static const StringName s("on_ally_defense");
	return s;
}
inline const StringName &sn_on_tick() {
	static const StringName s("on_tick");
	return s;
}
inline const StringName &sn_post_attack() {
	static const StringName s("post_attack");
	return s;
}
inline const StringName &sn_post_take_damage() {
	static const StringName s("post_take_damage");
	return s;
}
inline const StringName &sn_on_ability() {
	static const StringName s("on_ability");
	return s;
}
inline const StringName &sn_on_ultimate() {
	static const StringName s("on_ultimate");
	return s;
}
inline const StringName &sn_post_heal() {
	static const StringName s("post_heal");
	return s;
}
inline const StringName &sn_on_takedown() {
	static const StringName s("on_takedown");
	return s;
}

int passive_bucket_index(const StringName &kind) {
	if (kind == sn_on_attack()) {
		return 0;
	}
	if (kind == sn_on_defense()) {
		return 1;
	}
	if (kind == sn_on_ally_defense()) {
		return 2;
	}
	if (kind == sn_on_tick()) {
		return 3;
	}
	if (kind == sn_post_attack()) {
		return 4;
	}
	if (kind == sn_post_take_damage()) {
		return 5;
	}
	if (kind == sn_on_ability()) {
		return 6;
	}
	if (kind == sn_on_ultimate()) {
		return 7;
	}
	if (kind == sn_post_heal()) {
		return 8;
	}
	if (kind == sn_on_takedown()) {
		return 9;
	}
	UtilityFunctions::push_error(vformat(
			"[DEBUG] passive_bucket_index: unrecognized trigger kind '%s', falling through to bucket 5 (post_take_damage)",
			String(kind)));
	return 5;
}

std::vector<EffectRecord> compile_effect_array(const UnitBuilderHost &host, const Array &effects) {
	std::vector<EffectRecord> compiled;
	compiled.reserve(static_cast<size_t>(effects.size()));
	for (int64_t index = 0; index < effects.size(); ++index) {
		compiled.push_back(host.compile_effect(host.user_data, Dictionary(effects[index])));
	}
	return compiled;
}

EffectRecord compile_effect(const UnitBuilderHost &host, const Dictionary &effect) {
	return host.compile_effect(host.user_data, effect);
}

} // namespace

std::pair<UnitState, UnitStateCold> build_unit(
		const UnitBuilderHost &host,
		const Dictionary &spawn_spec,
		const StringName &team,
		int64_t instance_id) {
	UnitState unit;
	UnitStateCold cold;
	const catalog::CatalogState &catalog = *host.catalog;
	StringName unit_id = StringName(String(spawn_spec.get("unit_id", "")));

	Dictionary minion_data = Dictionary(catalog.minion_catalog.get(String(unit_id), Dictionary()));
	Dictionary champion;

	if (!minion_data.is_empty()) {
		champion = minion_data;
	} else {
		champion = host.effective_champion_for(host.user_data, unit_id);
		if (champion.is_empty()) {
			UtilityFunctions::push_error(vformat("Unknown champion archetype: %s", String(unit_id)));
			return { unit, cold };
		}
	}

	Dictionary stats = Dictionary(champion["stats"]).duplicate(true);
	StringName role_name = StringName(stats.get("role", StringName()));
	Dictionary role_config = Dictionary(catalog.role_configs.get(role_name, Dictionary()));

	bool is_minion = (role_name == StringName("minion"));

	Array passive_ids = Array(champion.get("passive_ids", Array()));
	for (int64_t index = 0; index < passive_ids.size(); ++index) {
		StringName passive_id = StringName(String(passive_ids[index]));
		Dictionary entry = Dictionary(catalog.passive_registry.get(passive_id, Dictionary()));
		if (entry.is_empty()) {
			continue;
		}
		Array effect_kinds;
		effect_kinds.append(StringName("on_attack"));
		effect_kinds.append(StringName("on_defense"));
		effect_kinds.append(StringName("on_ally_defense"));
		effect_kinds.append(StringName("on_tick"));
		effect_kinds.append(StringName("post_attack"));
		effect_kinds.append(StringName("post_take_damage"));
		effect_kinds.append(StringName("on_ability"));
		effect_kinds.append(StringName("on_ultimate"));
		effect_kinds.append(StringName("post_heal"));
		effect_kinds.append(StringName("on_takedown"));
		if (entry.has("on_ally_defense")) {
			double radius = double(entry.get("radius", 0.0));
			if (radius > cold.on_ally_defense_radius) {
				cold.on_ally_defense_radius = radius;
			}
		}
		if (entry.has("radius")) {
			double radius = double(entry.get("radius", 0.0));
			if (radius > 0.0) {
				UnitStateCold::PassiveAoeInfo aoe_info;
				aoe_info.passive_id = passive_id;
				aoe_info.radius = radius;
				cold.passive_aoe_info.push_back(aoe_info);
			}
		}
		for (int64_t kind_index = 0; kind_index < effect_kinds.size(); ++kind_index) {
			Variant kind_value = effect_kinds[kind_index];
			Array effects = Array(entry.get(kind_value, Array()));
			std::vector<EffectRecord> compiled_effects = compile_effect_array(host, effects);
			std::vector<EffectRecord> &bucket = cold.passive_effects[passive_bucket_index(StringName(String(kind_value)))];
			bucket.insert(bucket.end(), compiled_effects.begin(), compiled_effects.end());
		}
	}
	Variant role_tick = role_config.get("passive_on_tick", Variant());
	if (role_tick.get_type() != Variant::NIL) {
		cold.passive_effects[3].push_back(compile_effect(host, Dictionary(role_tick)));
	}
	Variant role_take_damage = role_config.get("passive_post_take_damage", Variant());
	if (role_take_damage.get_type() != Variant::NIL) {
		cold.passive_effects[5].push_back(compile_effect(host, Dictionary(role_take_damage)));
	}
	cold.on_tick_effect_accumulators.resize(cold.passive_effects[EFFECT_BUCKET_ON_TICK].size(), 0.0);

	double max_hp = double(stats.get("max_hp", 0.0));
	double mana_cost = double(stats.get("mana_cost", 0.0));
	double x = 0.0;
	double y = 0.0;

	if (!is_minion) {
		int64_t spawn_slot = host.assign_spawn_slot(host.user_data, team);
		Vector2 spawn_pos = host.get_random_spawn_position(host.user_data, team, false);

		static const std::vector<double> spawn_points = { 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0 };
		if (spawn_slot >= 0 && spawn_slot < int(spawn_points.size())) {
			double x_base = (team == sn_player()) ? PLAYER_SPAWN_X_BASE : ENEMY_SPAWN_X_BASE;

			double attack_range = double(stats.get("attack_range", 0.0));
			if (attack_range <= 1.0) {
				if (team == sn_player()) {
					x_base += 0.5;
				} else {
					x_base -= 0.5;
				}
			}

			x = x_base;
			y = spawn_points[static_cast<size_t>(spawn_slot)];
			cold.respawn_slot_index = spawn_slot;
		} else {
			x = spawn_pos.x;
			y = spawn_pos.y;
		}
	} else {
		x = double(spawn_spec.get("x", 0.0));
		y = double(spawn_spec.get("y", 0.0));
		cold.respawn_slot_index = -1;
	}

	unit.instance_id = instance_id;
	cold.unit_id = unit_id;
	unit.team = team;
	cold.role_id = role_name;

	StringName effective_role_name = role_name;
	if (is_minion) {
		double attack_range = double(stats.get("attack_range", 0.0));
		effective_role_name = (attack_range <= 0.5) ? sn_fighter() : sn_marksman();
	}

	unit.role_slot = targeting::role_slot_for_name(effective_role_name);
	unit.is_tank_role = effective_role_name == sn_tank();
	unit.is_fighter_role = effective_role_name == sn_fighter();
	unit.is_assassin_role = effective_role_name == sn_assassin();
	unit.is_marksman_role = effective_role_name == sn_marksman();
	unit.is_mage_role = effective_role_name == sn_mage();
	unit.is_support_role = effective_role_name == sn_support();
	cold.stats = stats;
#define X(name, def, min_val, max_val) \
	unit.combat.name = double(stats.get(#name, def));
	STAT_LIST
#undef X

	Variant ability_effect = champion.get("ability", Variant());
	Variant ultimate_effect = champion.get("ultimate", Variant());
	unit.has_ability_effect = ability_effect.get_type() == Variant::DICTIONARY;
	unit.has_ultimate_effect = ultimate_effect.get_type() == Variant::DICTIONARY;
	unit.ability_requires_target_in_range = bool(Dictionary(ability_effect).get("requires_target_in_range", true));
	unit.ultimate_requires_target_in_range = bool(Dictionary(ultimate_effect).get("requires_target_in_range", true));
	if (unit.has_ability_effect) {
		cold.ability_effect = compile_effect(host, Dictionary(ability_effect));
	}
	if (unit.has_ultimate_effect) {
		cold.ultimate_effect = compile_effect(host, Dictionary(ultimate_effect));
	}

	cold.spawn_pos_x = x;
	cold.spawn_pos_y = y;
	unit.pos_x = x;
	unit.pos_y = y;
	cold.respawn_slot_index = -1;
	unit.hp = max_hp;
	unit.shield = 0.0;
	unit.mana = 0.0;
	unit.attack_cooldown = 0.0;
	unit.attack_period = 0.0;
	unit.ability_cooldown = get_effective_ability_cd(unit);
	unit.casting_remaining = 0.0;
	cold.casting_kind = StringName();
	cold.casting_effect = EffectRecord();
	unit.casting_target_id = 0;
	unit.casting_ally_target_id = 0;
	unit.cast_resolved_this_tick = false;
	unit.target_id = 0;
	unit.target_index = -1;
	unit.current_ally_target_id = 0;
	unit.retarget_timer = 0.0;
	unit.target_switch_lock_timer = 0.0;
	unit.last_kite_timer = 0.0;
	unit.current_target_score = 0.0;
	unit.stun_remaining = 0.0;
	unit.hard_cc_seconds = 0.0;
	unit.root_remaining = 0.0;
	unit.silence_remaining = 0.0;
	unit.silence_ability_remaining = 0.0;
	unit.silence_ultimate_remaining = 0.0;
	unit.silence_blocks_abilities = false;
	unit.silence_blocks_ultimates = false;
	unit.disarm_remaining = 0.0;
	unit.stealth_remaining = 0.0;
	unit.stealth_break_on_attack = false;
	unit.stealth_break_on_ability = false;
	unit.stealth_break_on_damage_taken = false;
	unit.respawn_timer = 0.0;
	unit.respawned_this_tick = false;
	unit.alive = true;
	unit.incoming_target_count = 0;
	unit.perceived_threat = 0.0;
	unit.attack_count = 0;
#define X(name, def, min_val, max_val) \
	unit.stat_additive_##name = 0.0; \
	unit.stat_multiplicative_##name = 1.0;
	STAT_LIST
#undef X
	cold.damage_dealt = 0.0;
	cold.damage_dealt_auto = 0.0;
	cold.damage_dealt_ability = 0.0;
	cold.damage_dealt_ultimate = 0.0;
	cold.damage_dealt_passive = 0.0;
	cold.damage_received = 0.0;
	cold.damage_mitigated = 0.0;
	cold.healing_done = 0.0;
	cold.healing_done_auto = 0.0;
	cold.healing_done_ability = 0.0;
	cold.healing_done_ultimate = 0.0;
	cold.healing_done_passive = 0.0;
	cold.shielding_done = 0.0;
	cold.shielding_done_auto = 0.0;
	cold.shielding_done_ability = 0.0;
	cold.shielding_done_ultimate = 0.0;
	cold.shielding_done_passive = 0.0;
	cold.auto_attacks = 0;
	cold.abilities = 0;
	cold.ultimates = 0;
	cold.stuns = 0;
	cold.kills = 0;
	cold.deaths = 0;
	cold.assists = 0;
	unit.taunt_target_id = 0;
	unit.taunt_remaining = 0.0;
	unit.forced_target_id = 0;
	unit.forced_target_remaining = 0.0;
	cold.forced_target_kind = StringName();
	cold.damage_sources.clear();
	cold.recent_benefactors.clear();
	cold.last_hit_time = 0.0;
	std::fill(cold.on_tick_effect_accumulators.begin(), cold.on_tick_effect_accumulators.end(), 0.0);
	unit.regen_accumulator = 0.0;
	host.finalize_reflect_passives(host.user_data, unit, cold);
	return { unit, std::move(cold) };
}

} // namespace unit_builder
} // namespace sim
