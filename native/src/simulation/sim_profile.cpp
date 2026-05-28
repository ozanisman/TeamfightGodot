#include "sim_profile.hpp"

#include "sim_profile_counters.hpp"

#include "../teamfight_simulation_core.hpp"

#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace sim::profile {

void reset(TeamfightSimulationCore &core) {
	core._sim_profile_counters = Counters{};
}

void emit_json_stderr(const TeamfightSimulationCore &core) {
	Dictionary profile;
	profile["ns_projectiles"] = core._sim_profile_counters.ns_projectiles;
	profile["ns_prepare_tick_ctx"] = core._sim_profile_counters.ns_prepare_tick_ctx;
	profile["ns_refresh_pressure_pre"] = core._sim_profile_counters.ns_refresh_pressure_pre;
	profile["ns_update_units"] = core._sim_profile_counters.ns_update_units;
	profile["ns_refresh_pressure_post"] = core._sim_profile_counters.ns_refresh_pressure_post;
	profile["tick_count"] = core._sim_profile_counters.tick_count;
	profile["uu_dead_respawn"] = core._sim_profile_counters.uu_dead_respawn;
	profile["uu_cooldowns_cc"] = core._sim_profile_counters.uu_cooldowns_cc;
	profile["uu_separation"] = core._sim_profile_counters.uu_separation;
	profile["uu_threat_and_assist"] = core._sim_profile_counters.uu_threat_and_assist;
	profile["uu_regen_on_tick"] = core._sim_profile_counters.uu_regen_on_tick;
	profile["uu_casting"] = core._sim_profile_counters.uu_casting;
	profile["uu_targeting"] = core._sim_profile_counters.uu_targeting;
	profile["uu_combat"] = core._sim_profile_counters.uu_combat;
	profile["uu_movement"] = core._sim_profile_counters.uu_movement;
	profile["se_base"] = core._sim_profile_counters.se_base;
	profile["se_calls"] = core._sim_profile_counters.se_calls;
	profile["uc_attack_cooldown"] = core._sim_profile_counters.uc_attack_cooldown;
	profile["uc_distance_calc"] = core._sim_profile_counters.uc_distance_calc;
	profile["uc_hit_validation"] = core._sim_profile_counters.uc_hit_validation;
	profile["uc_damage_apply"] = core._sim_profile_counters.uc_damage_apply;
	profile["uc_auto_attack"] = core._sim_profile_counters.uc_auto_attack;
	profile["uc_ability"] = core._sim_profile_counters.uc_ability;
	profile["ucc_attack_cd"] = core._sim_profile_counters.ucc_attack_cd;
	profile["ucc_ability_cd"] = core._sim_profile_counters.ucc_ability_cd;
	profile["ucc_retarget"] = core._sim_profile_counters.ucc_retarget;
	profile["ucc_target_switch"] = core._sim_profile_counters.ucc_target_switch;
	profile["ucc_stun"] = core._sim_profile_counters.ucc_stun;
	profile["ucc_slow"] = core._sim_profile_counters.ucc_slow;
	profile["ucc_root"] = core._sim_profile_counters.ucc_root;
	profile["ucc_silence"] = core._sim_profile_counters.ucc_silence;
	profile["ucc_disarm"] = core._sim_profile_counters.ucc_disarm;
	profile["ucc_stealth"] = core._sim_profile_counters.ucc_stealth;
	profile["ucc_shield"] = core._sim_profile_counters.ucc_shield;
	profile["ucc_reflect"] = core._sim_profile_counters.ucc_reflect;
	profile["ucc_taunt"] = core._sim_profile_counters.ucc_taunt;
	profile["ucc_forced_target"] = core._sim_profile_counters.ucc_forced_target;
	profile["ctx_team_centers"] = core._sim_profile_counters.ctx_team_centers;
	profile["ctx_role_classification"] = core._sim_profile_counters.ctx_role_classification;
	profile["ctx_targeting_sync"] = core._sim_profile_counters.ctx_targeting_sync;
	profile["ctx_spatial_grid"] = core._sim_profile_counters.ctx_spatial_grid;
	profile["ctx_density"] = core._sim_profile_counters.ctx_density;
	profile["um_kiting"] = core._sim_profile_counters.um_kiting;
	profile["um_kiting_spatial"] = core._sim_profile_counters.um_kiting_spatial;
	profile["um_kiting_brute"] = core._sim_profile_counters.um_kiting_brute;
	profile["um_toward"] = core._sim_profile_counters.um_toward;
	profile["um_boundary"] = core._sim_profile_counters.um_boundary;
	profile["um_nudge"] = core._sim_profile_counters.um_nudge;
	profile["ur_hp_mana"] = core._sim_profile_counters.ur_hp_mana;
	profile["ur_effects"] = core._sim_profile_counters.ur_effects;
	profile["ur_periodic"] = core._sim_profile_counters.ur_periodic;
	profile["ur_channel"] = core._sim_profile_counters.ur_channel;
	if (core._sim_profile_runtime.targeting_active) {
		profile["tgt_retarget_keeps"] = core._sim_profile_counters.tgt_retarget_keeps;
		profile["tgt_enemy_scans"] = core._sim_profile_counters.tgt_enemy_scans;
		profile["tgt_candidates_scored"] = core._sim_profile_counters.tgt_candidates_scored;
		profile["tgt_candidates_prefix_pruned"] = core._sim_profile_counters.tgt_candidates_prefix_pruned;
		profile["tgt_ally_scans"] = core._sim_profile_counters.tgt_ally_scans;
		profile["tgt_frame_syncs"] = core._sim_profile_counters.tgt_frame_syncs;
		profile["tgt_ties_adjusted"] = core._sim_profile_counters.tgt_ties_adjusted;
		profile["tgt_ties_raw"] = core._sim_profile_counters.tgt_ties_raw;
		profile["tgt_ties_distance"] = core._sim_profile_counters.tgt_ties_distance;
		profile["tgt_ties_instance"] = core._sim_profile_counters.tgt_ties_instance;
	}
	
	String json = JSON::stringify(profile);
	UtilityFunctions::print(json);
}

} // namespace sim::profile
