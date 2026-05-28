#ifndef SIM_COORDINATOR_HOST_HPP
#define SIM_COORDINATOR_HOST_HPP

#include "sim_world.hpp"
#include "simulation_types.hpp"
#include "sim_unit_builder.hpp"
#include "sim_match_roster.hpp"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <vector>

class TeamfightSimulationCore;

namespace sim {

struct CoordinatorHostAccess {
	static viewer::ViewerFxBuffer &viewer_fx(TeamfightSimulationCore *core);
	static SimWorld sim_world(TeamfightSimulationCore *core);
	static std::vector<UnitState> &units(TeamfightSimulationCore *core);
	static std::vector<UnitStateCold> &unit_cold(TeamfightSimulationCore *core);
	static UnitStateCold &unit_cold_at(TeamfightSimulationCore *core, size_t index);
	static UnitStateCold &uc(TeamfightSimulationCore *core, UnitState &unit);
	static const UnitStateCold &uc(TeamfightSimulationCore *core, const UnitState &unit);
};

void sim_host_handle_death(void *user_data, UnitState &killer, UnitState &target);
void sim_host_sync_targeting_frame_unit(void *user_data, const UnitState &unit);
void sim_host_sync_targeting_frame_index(void *user_data, int64_t index, const UnitState &unit);
void sim_host_emit_trace(void *user_data, const StringName &kind, int64_t src_id, int64_t tgt_id, double val);
void sim_host_viewer_record_damage_fx(
		void *user_data,
		const UnitState &source,
		const UnitState &target,
		double total_damage,
		const StringName &action_kind,
		const StringName &damage_type);
void sim_host_viewer_record_hot_status_fx(void *user_data, const UnitState &target, double duration, const StringName &effect_type);
void sim_host_viewer_record_heal_fx(void *user_data, const UnitState &target, double amount);
void sim_host_viewer_record_shield_fx(void *user_data, const UnitState &target, double amount);
void sim_host_viewer_record_aoe_shape_fx(
		void *user_data,
		const UnitState &source,
		const UnitState *target,
		const AoShapeParams &params,
		const StringName &kind);
void sim_host_viewer_record_passive_aoe_fx(
		void *user_data,
		const UnitState &unit,
		double radius,
		const StringName &passive_id);
Dictionary sim_host_effective_champion_for(void *user_data, const StringName &archetype_id);
EffectRecord sim_host_compile_effect(void *user_data, const Dictionary &effect);
void sim_host_finalize_reflect_passives(void *user_data, UnitState &unit, UnitStateCold &cold);
int64_t sim_host_assign_spawn_slot(void *user_data, const StringName &team);
Vector2 sim_host_get_random_spawn_position(void *user_data, const StringName &team, bool is_respawn);
Dictionary sim_host_coerce_spawn_spec(void *user_data, const Variant &value);
uint32_t sim_host_rand_uint32(void *user_data);
double sim_host_randf(void *user_data);
StringName sim_host_archetype_for_unit(void *user_data, const UnitState &unit);
void sim_host_print_line(void *user_data, const String &line);
void sim_host_print_score_breakdown(
		void *user_data,
		const ScoreBreakdown &breakdown,
		const StringName &attacker_archetype,
		const StringName &enemy_archetype);
UnitState *sim_host_select_ally_target(void *user_data, UnitState &unit);
void sim_host_match_update_projectiles(void *user_data, bool profile_sim);
void sim_host_match_prepare_tick_context(void *user_data, bool profile_sim);
void sim_host_match_update_unit(void *user_data, UnitState &unit, bool profile_sim);
match_roster::MatchRosterState sim_host_match_roster_state(void *user_data);
unit_builder::UnitBuilderHost sim_host_match_unit_builder_host(void *user_data);
void sim_host_match_profile_reset(void *user_data);
void sim_host_match_profile_emit_json_stderr(void *user_data);
bool sim_host_match_profile_env_enabled(void *user_data);
bool sim_host_match_targeting_profile_enabled(void *user_data);
void sim_host_match_log_sudden_death_draw(void *user_data);
void sim_host_unit_tick_respawn(void *user_data, UnitState &unit);
const UnitStrategy &sim_host_unit_tick_strategy(void *user_data, const UnitState &unit);
UnitState *sim_host_unit_tick_select_enemy_target(void *user_data, UnitState &unit, bool profile_sim);
UnitState *sim_host_unit_tick_select_ally_target(void *user_data, UnitState &unit);
void sim_host_unit_tick_prune_assist_window(void *user_data, UnitState &unit);
bool sim_host_debug_combat_trace(void *user_data);

} // namespace sim

#endif // SIM_COORDINATOR_HOST_HPP
