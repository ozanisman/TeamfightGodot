#ifndef SIM_UNIT_TICK_HPP
#define SIM_UNIT_TICK_HPP

#include "sim_channel.hpp"
#include "sim_combat.hpp"
#include "sim_world.hpp"

#include <cstdint>
#include <vector>

namespace sim {
namespace unit_tick {

struct UnitTickProfileCounters {
	bool profile_sim = false;

	uint64_t *uu_dead_respawn = nullptr;
	uint64_t *uu_cooldowns_cc = nullptr;
	uint64_t *uu_separation = nullptr;
	uint64_t *uu_threat_and_assist = nullptr;
	uint64_t *uu_regen_on_tick = nullptr;
	uint64_t *uu_casting = nullptr;
	uint64_t *uu_targeting = nullptr;
	uint64_t *uu_combat = nullptr;
	uint64_t *uu_movement = nullptr;

	uint64_t *ucc_attack_cd = nullptr;
	uint64_t *ucc_ability_cd = nullptr;
	uint64_t *ucc_retarget = nullptr;
	uint64_t *ucc_target_switch = nullptr;
	uint64_t *ucc_stun = nullptr;
	uint64_t *ucc_slow = nullptr;
	uint64_t *ucc_root = nullptr;
	uint64_t *ucc_silence = nullptr;
	uint64_t *ucc_disarm = nullptr;
	uint64_t *ucc_stealth = nullptr;
	uint64_t *ucc_shield = nullptr;
	uint64_t *ucc_reflect = nullptr;
	uint64_t *ucc_taunt = nullptr;
	uint64_t *ucc_forced_target = nullptr;

	uint64_t *ur_effects = nullptr;
	uint64_t *ur_channel = nullptr;
	uint64_t *ur_periodic = nullptr;
	int64_t *regen_no_work_fast_path = nullptr;
	int64_t *regen_on_tick_empty = nullptr;
	int64_t *regen_periodic_empty = nullptr;

	uint64_t *uc_distance_calc = nullptr;
	uint64_t *uc_ability = nullptr;
	uint64_t *uc_auto_attack = nullptr;

	uint64_t *um_kiting = nullptr;
	uint64_t *um_kiting_spatial = nullptr;
	uint64_t *um_kiting_brute = nullptr;
	uint64_t *um_toward = nullptr;
};

struct UnitTickMatchState {
	int64_t sudden_death_ticks = 0;
	int64_t *player_kills = nullptr;
	int64_t *enemy_kills = nullptr;
};

struct UnitTickHostHooks {
	void *user_data = nullptr;

	void (*respawn_unit)(void *user_data, UnitState &unit) = nullptr;
	const UnitStrategy &(*strategy_for_unit)(void *user_data, const UnitState &unit) = nullptr;
	UnitState *(*select_enemy_target)(void *user_data, UnitState &unit, bool profile_sim) = nullptr;
	UnitState *(*select_ally_target)(void *user_data, UnitState &unit) = nullptr;
	void (*prune_assist_window)(void *user_data, UnitState &unit) = nullptr;
};

struct TargetingResult {
	UnitState *target_ally = nullptr;
	UnitState *target = nullptr;
	bool stop = false;
};

void cooldowns_and_cc(
		SimWorld &world,
		UnitState &unit,
		const UnitTickHostHooks &tick_hooks,
		UnitTickProfileCounters &profile);

void apply_sudden_death_overtime(SimWorld &world, const UnitTickMatchState &match);

void separation(SimWorld &world, UnitState &unit, UnitTickProfileCounters &profile);

void threat_and_assist(
		SimWorld &world,
		UnitState &unit,
		const UnitStrategy &strategy,
		SimHostCallbacks &host,
		const UnitTickHostHooks &tick_hooks,
		UnitTickProfileCounters &profile);

bool regen_and_periodic(
		SimWorld &world,
		UnitState &unit,
		SimHostCallbacks &host,
		const channel::ChannelHostHooks &channel_hooks,
		const UnitTickHostHooks &tick_hooks,
		UnitTickProfileCounters &profile);

bool casting(
		SimWorld &world,
		UnitState &unit,
		SimHostCallbacks &host,
		const combat::CombatHostHooks &combat_hooks,
		UnitTickProfileCounters &profile);

TargetingResult targeting(
		SimWorld &world,
		UnitState &unit,
		SimHostCallbacks &host,
		const UnitTickHostHooks &tick_hooks,
		UnitTickProfileCounters &profile);

bool combat_actions(
		SimWorld &world,
		UnitState &unit,
		UnitState &target,
		SimHostCallbacks &host,
		const combat::CombatHostHooks &combat_hooks,
		std::vector<ProjectileState> &projectiles,
		UnitTickProfileCounters &profile);

bool movement(
		SimWorld &world,
		UnitState &unit,
		UnitState &target,
		const UnitStrategy &strategy,
		SimHostCallbacks &host,
		UnitTickProfileCounters &profile);

void update_unit(
		SimWorld &world,
		UnitState &unit,
		SimHostCallbacks &host,
		const channel::ChannelHostHooks &channel_hooks,
		const combat::CombatHostHooks &combat_hooks,
		const UnitTickHostHooks &tick_hooks,
		const UnitTickMatchState &match,
		UnitTickProfileCounters &profile,
		std::vector<ProjectileState> &projectiles);

} // namespace unit_tick
} // namespace sim

#endif // SIM_UNIT_TICK_HPP
