#ifndef SIM_PROFILE_COUNTERS_HPP
#define SIM_PROFILE_COUNTERS_HPP

#include <cstdint>

namespace sim::profile {

struct RuntimeFlags {
	bool sim_active = false;
	bool targeting_active = false;
};

struct Counters {
	uint64_t ns_projectiles = 0;
	uint64_t ns_prepare_tick_ctx = 0;
	uint64_t ns_refresh_pressure_pre = 0;
	uint64_t ns_update_units = 0;
	uint64_t ns_refresh_pressure_post = 0;
	int64_t tick_count = 0;
	uint64_t uu_dead_respawn = 0;
	uint64_t uu_cooldowns_cc = 0;
	uint64_t uu_separation = 0;
	uint64_t uu_threat_and_assist = 0;
	uint64_t uu_regen_on_tick = 0;
	uint64_t uu_casting = 0;
	uint64_t uu_targeting = 0;
	uint64_t uu_combat = 0;
	uint64_t uu_movement = 0;
	uint64_t se_base = 0;
	int64_t se_calls = 0;
	uint64_t uc_attack_cooldown = 0;
	uint64_t uc_distance_calc = 0;
	uint64_t uc_hit_validation = 0;
	uint64_t uc_damage_apply = 0;
	uint64_t uc_auto_attack = 0;
	uint64_t uc_ability = 0;
	uint64_t ucc_attack_cd = 0;
	uint64_t ucc_ability_cd = 0;
	uint64_t ucc_retarget = 0;
	uint64_t ucc_target_switch = 0;
	uint64_t ucc_stun = 0;
	uint64_t ucc_slow = 0;
	uint64_t ucc_root = 0;
	uint64_t ucc_silence = 0;
	uint64_t ucc_disarm = 0;
	uint64_t ucc_stealth = 0;
	uint64_t ucc_shield = 0;
	uint64_t ucc_reflect = 0;
	uint64_t ucc_taunt = 0;
	uint64_t ucc_forced_target = 0;
	uint64_t ctx_team_centers = 0;
	uint64_t ctx_role_classification = 0;
	uint64_t ctx_targeting_sync = 0;
	uint64_t ctx_spatial_grid = 0;
	uint64_t ctx_density = 0;
	uint64_t um_kiting = 0;
	uint64_t um_kiting_spatial = 0;
	uint64_t um_kiting_brute = 0;
	uint64_t um_toward = 0;
	uint64_t um_boundary = 0;
	uint64_t um_nudge = 0;
	uint64_t ur_hp_mana = 0;
	uint64_t ur_effects = 0;
	uint64_t ur_periodic = 0;
	uint64_t ur_channel = 0;
	int64_t tgt_retarget_keeps = 0;
	int64_t tgt_enemy_scans = 0;
	int64_t tgt_candidates_scored = 0;
	int64_t tgt_candidates_prefix_pruned = 0;
	int64_t tgt_ally_scans = 0;
	int64_t tgt_frame_syncs = 0;
	int64_t tgt_ties_adjusted = 0;
	int64_t tgt_ties_raw = 0;
	int64_t tgt_ties_distance = 0;
	int64_t tgt_ties_instance = 0;
};

} // namespace sim::profile

#endif // SIM_PROFILE_COUNTERS_HPP
