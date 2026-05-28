#include "sim_coordinator_tick.hpp"

#include "../teamfight_simulation_core.hpp"

#include "sim_combat.hpp"
#include "sim_profile_counters.hpp"
#include "sim_unit_tick.hpp"

namespace {
sim::unit_tick::UnitTickProfileCounters build_unit_tick_profile(sim::profile::Counters &counters, bool profile_sim) {
	sim::unit_tick::UnitTickProfileCounters tick_profile{};
	tick_profile.profile_sim = profile_sim;
	if (!profile_sim) {
		return tick_profile;
	}
	tick_profile.uu_dead_respawn = &counters.uu_dead_respawn;
	tick_profile.uu_cooldowns_cc = &counters.uu_cooldowns_cc;
	tick_profile.uu_separation = &counters.uu_separation;
	tick_profile.uu_threat_and_assist = &counters.uu_threat_and_assist;
	tick_profile.uu_regen_on_tick = &counters.uu_regen_on_tick;
	tick_profile.uu_casting = &counters.uu_casting;
	tick_profile.uu_targeting = &counters.uu_targeting;
	tick_profile.uu_combat = &counters.uu_combat;
	tick_profile.uu_movement = &counters.uu_movement;
	tick_profile.ucc_attack_cd = &counters.ucc_attack_cd;
	tick_profile.ucc_ability_cd = &counters.ucc_ability_cd;
	tick_profile.ucc_retarget = &counters.ucc_retarget;
	tick_profile.ucc_target_switch = &counters.ucc_target_switch;
	tick_profile.ucc_stun = &counters.ucc_stun;
	tick_profile.ucc_slow = &counters.ucc_slow;
	tick_profile.ucc_root = &counters.ucc_root;
	tick_profile.ucc_silence = &counters.ucc_silence;
	tick_profile.ucc_disarm = &counters.ucc_disarm;
	tick_profile.ucc_stealth = &counters.ucc_stealth;
	tick_profile.ucc_shield = &counters.ucc_shield;
	tick_profile.ucc_reflect = &counters.ucc_reflect;
	tick_profile.ucc_taunt = &counters.ucc_taunt;
	tick_profile.ucc_forced_target = &counters.ucc_forced_target;
	tick_profile.ur_effects = &counters.ur_effects;
	tick_profile.ur_channel = &counters.ur_channel;
	tick_profile.ur_periodic = &counters.ur_periodic;
	tick_profile.uc_distance_calc = &counters.uc_distance_calc;
	tick_profile.uc_ability = &counters.uc_ability;
	tick_profile.uc_auto_attack = &counters.uc_auto_attack;
	tick_profile.um_kiting = &counters.um_kiting;
	tick_profile.um_kiting_spatial = &counters.um_kiting_spatial;
	tick_profile.um_kiting_brute = &counters.um_kiting_brute;
	tick_profile.um_toward = &counters.um_toward;
	return tick_profile;
}
} // namespace

void TeamfightSimulationCore::_update_unit(UnitState &unit, bool profile_sim) {
	sim::SimWorld w = _sim_world();

	sim::unit_tick::UnitTickProfileCounters tick_profile = build_unit_tick_profile(_sim_profile_counters, profile_sim);

	sim::unit_tick::UnitTickMatchState match{};
	match.sudden_death_ticks = _sudden_death_ticks;
	match.player_kills = &_player_kills;
	match.enemy_kills = &_enemy_kills;

	sim::unit_tick::update_unit(
			w,
			unit,
			_sim_host_callbacks,
			_channel_host_hooks(),
			_combat_host_hooks(),
			_unit_tick_host_hooks(),
			match,
			tick_profile,
			_projectiles);
}

void TeamfightSimulationCore::_update_projectiles() {
	sim::SimWorld w = _sim_world();
	sim::combat::ProjectileBuffers buffers{ _projectiles, _active_projectiles, _scratch_projectiles };
	sim::combat::ProjectileMatchState match{ _sudden_death_ticks, _player_kills, _enemy_kills };
	sim::combat::update_projectiles(w, _sim_host_callbacks, _combat_host_hooks(), buffers, match);
}
