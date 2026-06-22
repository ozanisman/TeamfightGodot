#include "sim_channel.hpp"

#include "sim_combat.hpp"
#include "sim_spatial.hpp"
#include "sim_stats.hpp"
#include "sim_targeting.hpp"

#include <godot_cpp/variant/utility_functions.hpp>

#include <limits>

namespace sim {
namespace channel {

namespace {

inline const StringName &sn_ability() {
	static const StringName s("ability");
	return s;
}

inline const StringName &sn_ultimate() {
	static const StringName s("ultimate");
	return s;
}

Dictionary execute_effect(SimHostCallbacks &host, const EffectRecord &effect, EffectContext &context) {
	if (host.execute_effect == nullptr) {
		return Dictionary();
	}
	return host.execute_effect(host, effect, context);
}

int64_t get_channel_tick_count(const UnitStateCold &cold) {
	if (cold.channel_tick_interval <= 0.0) {
		return 0;
	}
	const double total_ticked = cold.channel_tick_accumulator;
	return static_cast<int64_t>(total_ticked / cold.channel_tick_interval);
}

void clear_channel_state(UnitStateCold &cold) {
	cold.is_channeling = false;
	cold.channel_remaining_duration = 0.0;
	cold.channel_tick_interval = 0.0;
	cold.channel_tick_accumulator = 0.0;
	cold.channel_accumulated_damage = 0.0;
	cold.channel_target_instance_id = 0;
	cold.channel_source_instance_id = 0;
	cold.channel_reason = StringName();
	cold.channel_action_kind = StringName();
	cold.channel_allow_movement = false;
	cold.channel_target_mode = StringName();
	cold.channel_sub_effect = EffectRecord();
	cold.channel_post_complete_effect = EffectRecord();
	cold.channel_post_interrupt_effect = EffectRecord();
}

bool should_interrupt_channel(SimWorld &world, UnitState &unit, const UnitStateCold &cold) {
	if (!unit.alive) {
		return true;
	}
	if (unit.stun_remaining > 0.0) {
		return true;
	}
	if (cold.channel_action_kind == sn_ability() && unit.silence_blocks_abilities) {
		return true;
	}
	if (cold.channel_action_kind == sn_ultimate() && unit.silence_blocks_ultimates) {
		return true;
	}
	if (unit.taunt_remaining > 0.0) {
		return true;
	}
	if (!cold.channel_allow_movement && unit.root_remaining <= 0.0) {
		const double raw_cast_range = cold.channel_action_kind == sn_ultimate()
				? unit.ultimate_cast_range
				: unit.ability_cast_range;
		// cast_range < 0: use attack range. == 0: no gate.
		double effective_cast_range;
		if (raw_cast_range < 0.0) {
			effective_cast_range = get_effective_attack_range(unit);
		} else if (raw_cast_range == 0.0) {
			effective_cast_range = -1.0;
		} else {
			effective_cast_range = raw_cast_range;
		}
		if (effective_cast_range >= 0.0) {
			// TODO: This range-interrupt check only scans for enemies. For
			// ally-targeted non-movement channels (e.g. a channel heal on a
			// fixed ally), this would incorrectly interrupt when enemies walk
			// away even if the ally is still in range. No current champion has
			// such a channel, so this is not yet a bug. If an ally-targeted
			// non-movement channel is added, store the EffectCastRangeSpec on
			// UnitStateCold at channel start and branch here on
			// needs_single_ally (scan allies) vs needs_enemy (scan enemies).
			static const StringName player_team("player");
			static const StringName enemy_team("enemy");
			const StringName &enemy_team_name = unit.team == player_team ? enemy_team : player_team;
			const std::vector<int64_t> &enemy_indices = alive_indices_for_team(world, enemy_team_name);
			bool has_target_in_range = false;
			for (int64_t index : enemy_indices) {
				const UnitState &enemy = world.units[static_cast<size_t>(index)];
				const double dist = distance_between_coords(unit.pos_x, unit.pos_y, enemy.pos_x, enemy.pos_y);
				if (dist <= effective_cast_range) {
					has_target_in_range = true;
					break;
				}
			}
			if (!has_target_in_range) {
				return true;
			}
		}
	}
	return false;
}

void complete_channel(SimWorld &world, SimHostCallbacks &host, UnitState &unit, UnitStateCold &cold) {
	cold.is_channeling = false;
	if (cold.channel_post_complete_effect.opcode != 0) {
		UnitState *target = cold.channel_target_instance_id != 0 ? targeting::unit_by_id(world, cold.channel_target_instance_id) : nullptr;
		if (cold.channel_target_mode == StringName("self")) {
			target = &unit;
		}
		EffectContext context = combat::build_context(unit, target, nullptr, 0.0, cold.channel_action_kind);
		context.channel_remaining_duration = cold.channel_remaining_duration;
		context.channel_tick_count = get_channel_tick_count(cold);
		context.channel_accumulated_damage = cold.channel_accumulated_damage;
		context.channel_completed = true;
		execute_effect(host, cold.channel_post_complete_effect, context);
	}
	if (cold.channel_action_kind == sn_ability()) {
		unit.ability_cooldown = get_effective_ability_cd(unit);
	}
	clear_channel_state(cold);
}

void interrupt_channel(SimWorld &world, SimHostCallbacks &host, UnitState &unit, UnitStateCold &cold) {
	cold.is_channeling = false;
	if (cold.channel_post_interrupt_effect.opcode != 0) {
		UnitState *target = cold.channel_target_instance_id != 0 ? targeting::unit_by_id(world, cold.channel_target_instance_id) : nullptr;
		if (cold.channel_target_mode == StringName("self")) {
			target = &unit;
		}
		EffectContext context = combat::build_context(unit, target, nullptr, 0.0, cold.channel_action_kind);
		context.channel_remaining_duration = cold.channel_remaining_duration;
		context.channel_tick_count = get_channel_tick_count(cold);
		context.channel_accumulated_damage = cold.channel_accumulated_damage;
		context.channel_completed = false;
		execute_effect(host, cold.channel_post_interrupt_effect, context);
	}
	if (cold.channel_action_kind == sn_ability()) {
		unit.ability_cooldown = get_effective_ability_cd(unit);
	}
	clear_channel_state(cold);
}

} // namespace

void process_channel_tick(SimWorld &world, SimHostCallbacks &host, const ChannelHostHooks &hooks, UnitState &unit, double delta) {
	UnitStateCold &cold = uc(world, unit);

	if (should_interrupt_channel(world, unit, cold)) {
		interrupt_channel(world, host, unit, cold);
		return;
	}

	cold.channel_tick_accumulator += delta;

	if (cold.channel_tick_accumulator >= cold.channel_tick_interval) {
		cold.channel_tick_accumulator -= cold.channel_tick_interval;

		UnitState *target = nullptr;
		if (cold.channel_target_mode == StringName("self")) {
			target = &unit;
		} else if (cold.channel_target_mode == StringName("fixed")) {
			target = cold.channel_target_instance_id != 0 ? targeting::unit_by_id(world, cold.channel_target_instance_id) : nullptr;
		} else if (cold.channel_target_mode == StringName("dynamic")) {
			static const StringName player_team("player");
			static const StringName enemy_team("enemy");
			const StringName &enemy_team_name = unit.team == player_team ? enemy_team : player_team;
			const std::vector<int64_t> &enemy_indices = alive_indices_for_team(world, enemy_team_name);
			if (!enemy_indices.empty()) {
				UnitState *closest = nullptr;
				double closest_dist = std::numeric_limits<double>::max();
				for (int64_t index : enemy_indices) {
					UnitState &enemy = world.units[static_cast<size_t>(index)];
					const double dist = distance_between_coords(unit.pos_x, unit.pos_y, enemy.pos_x, enemy.pos_y);
					if (dist < closest_dist) {
						closest_dist = dist;
						closest = &enemy;
					}
				}
				target = closest;
			}
		}

		if (target != nullptr && target->alive) {
			EffectContext context = combat::build_context(unit, target, nullptr, 0.0, cold.channel_action_kind);
			context.channel_remaining_duration = cold.channel_remaining_duration;
			context.channel_tick_count = get_channel_tick_count(cold);
			context.channel_accumulated_damage = cold.channel_accumulated_damage;

			const Dictionary result = execute_effect(host, cold.channel_sub_effect, context);
			if (result.has("damage")) {
				cold.channel_accumulated_damage += double(result["damage"]);
			}
		}
	}

	cold.channel_remaining_duration -= delta;
	if (cold.channel_remaining_duration <= 0.0) {
		complete_channel(world, host, unit, cold);
	}
}

} // namespace channel
} // namespace sim
