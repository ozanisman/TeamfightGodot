#include "sim_unit_tick.hpp"
#include "sim_unit_tick_internal.hpp"

#include "sim_channel.hpp"
#include "sim_combat.hpp"
#include "sim_constants.hpp"
#include "sim_periodic.hpp"
#include "sim_stats.hpp"

#include <godot_cpp/core/math.hpp>

namespace sim {
namespace unit_tick {

using internal::SimProfileAccScope;
using internal::execute_effect;
using internal::sn_on_tick;
using internal::sn_passive;
using internal::sn_player;

void apply_sudden_death_overtime(SimWorld &world, const UnitTickMatchState &match) {
	if (match.sudden_death_ticks <= 0) {
		return;
	}

	const double damage_rate = OVERTIME_DAMAGE_BASE_RATE;

	for (UnitState &overtime_unit : world.units) {
		if (!overtime_unit.alive) {
			continue;
		}

		const double max_hp = get_effective_max_hp(overtime_unit);
		const double damage = 1 + max_hp * damage_rate;

		const double shield_before = overtime_unit.shield;
		const double absorbed = Math::min(shield_before, damage);
		overtime_unit.shield = Math::max(0.0, shield_before - absorbed);
		const double hp_loss = Math::max(0.0, damage - absorbed);
		overtime_unit.hp = Math::max(0.0, overtime_unit.hp - hp_loss);

		if (overtime_unit.hp <= 0.0 && overtime_unit.alive) {
			overtime_unit.alive = false;
			// Minion deaths do not count for team score
			static const StringName sn_minion("minion");
			if (overtime_unit.team == sn_player() && uc(world, overtime_unit).role_id != sn_minion) {
				if (match.enemy_kills != nullptr) {
					++(*match.enemy_kills);
				}
			} else if (overtime_unit.team == internal::sn_enemy() && uc(world, overtime_unit).role_id != sn_minion) {
				if (match.player_kills != nullptr) {
					++(*match.player_kills);
				}
			}
		}
	}
}

bool regen_and_periodic(
		SimWorld &world,
		UnitState &unit,
		SimHostCallbacks &host,
		const channel::ChannelHostHooks &channel_hooks,
		const UnitTickHostHooks &tick_hooks,
		UnitTickProfileCounters &profile) {
	const bool profile_sim = profile.profile_sim;
	const double tick_rate = world.tick_rate;

	SimProfileAccScope _uu_regen(profile_sim, profile.uu_regen_on_tick);
	UnitStateCold &cold = uc(world, unit);
	const std::vector<EffectRecord> &effects = combat::collect_effects(world, unit, sn_on_tick());

	const bool has_regen_work = !effects.empty() || cold.is_channeling;
	if (has_regen_work) {
		{
			SimProfileAccScope _ur_eff(profile_sim, profile.ur_effects);
			if (cold.on_tick_effect_accumulators.size() < effects.size()) {
				cold.on_tick_effect_accumulators.resize(effects.size(), 0.0);
			}
			for (size_t effect_index = 0; effect_index < effects.size(); ++effect_index) {
				const EffectRecord &effect = effects[effect_index];
				double &accumulator = cold.on_tick_effect_accumulators[effect_index];
				accumulator += tick_rate;
				if (accumulator >= effect.on_tick_interval) {
					accumulator -= effect.on_tick_interval;
					EffectContext context = combat::build_context(unit, nullptr, nullptr, 0.0, sn_passive());
					execute_effect(host, effect, context);
				}
			}
		}

		if (cold.is_channeling) {
			SimProfileAccScope _ur_chn(profile_sim, profile.ur_channel);
			channel::process_channel_tick(world, host, channel_hooks, unit, tick_rate);
		}
	}

	if (!cold.periodic_effects.empty()) {
		SimProfileAccScope _ur_per(profile_sim, profile.ur_periodic);
		periodic::tick_periodic_effects(world, host, unit, tick_rate);
	}

	return unit.stun_remaining > 0.0;
}

} // namespace unit_tick
} // namespace sim
