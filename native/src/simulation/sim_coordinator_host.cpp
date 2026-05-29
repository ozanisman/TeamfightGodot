#include "sim_coordinator_host.hpp"

#include "../teamfight_simulation_core.hpp"
#include "sim_match.hpp"
#include "sim_match_roster.hpp"
#include "sim_profile.hpp"
#include "sim_viewer.hpp"

#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace sim {

void sim_host_handle_death(void *user_data, UnitState &killer, UnitState &target) {
	auto *core = static_cast<TeamfightSimulationCore *>(user_data);
	sim::SimWorld w = core->_sim_world();
	sim::match_roster::MatchRosterState roster = core->_match_roster_state();
	sim::match::MatchScoreState score = core->_match_score_state();
	score.roster = &roster;
	sim::match::SpawnSlotState slots = core->_spawn_slot_state();
	sim::match::handle_death(
			w,
			core->_sim_host_callbacks,
			&core->_viewer_hooks,
			score,
			slots,
			killer,
			target);
}

void sim_host_sync_targeting_frame_unit(void *user_data, const UnitState &unit) {
	static_cast<TeamfightSimulationCore *>(user_data)->_sync_targeting_frame_unit(unit);
}

void sim_host_sync_targeting_frame_index(void *user_data, int64_t index, const UnitState &unit) {
	static_cast<TeamfightSimulationCore *>(user_data)->_sync_targeting_frame_index(index, unit);
}

void sim_host_emit_trace(void *user_data, const StringName &kind, int64_t src_id, int64_t tgt_id, double val) {
	static_cast<TeamfightSimulationCore *>(user_data)->_emit_trace(kind, src_id, tgt_id, val);
}

viewer::ViewerFxBuffer &CoordinatorHostAccess::viewer_fx(TeamfightSimulationCore *core) {
	return core->_viewer_fx;
}

SimWorld CoordinatorHostAccess::sim_world(TeamfightSimulationCore *core) {
	return core->_sim_world();
}

std::vector<UnitState> &CoordinatorHostAccess::units(TeamfightSimulationCore *core) {
	return core->_units;
}

std::vector<UnitStateCold> &CoordinatorHostAccess::unit_cold(TeamfightSimulationCore *core) {
	return core->_unit_cold;
}

UnitStateCold &CoordinatorHostAccess::unit_cold_at(TeamfightSimulationCore *core, size_t index) {
	return core->_unit_cold[index];
}

UnitStateCold &CoordinatorHostAccess::uc(TeamfightSimulationCore *core, UnitState &unit) {
	return core->_uc(unit);
}

const UnitStateCold &CoordinatorHostAccess::uc(TeamfightSimulationCore *core, const UnitState &unit) {
	return core->_uc(unit);
}

void sim_host_viewer_record_damage_fx(
		void *user_data,
		const UnitState &source,
		const UnitState &target,
		double total_damage,
		const StringName &action_kind,
		const StringName &damage_type) {
	auto *core = static_cast<TeamfightSimulationCore *>(user_data);
	sim::viewer::record_damage_fx(CoordinatorHostAccess::viewer_fx(core), source, target, total_damage, action_kind, damage_type);
}

void sim_host_viewer_record_hot_status_fx(void *user_data, const UnitState &target, double duration, const StringName &effect_type) {
	auto *core = static_cast<TeamfightSimulationCore *>(user_data);
	sim::viewer::record_hot_status_fx(CoordinatorHostAccess::viewer_fx(core), target, duration, effect_type);
}

void sim_host_viewer_record_heal_fx(void *user_data, const UnitState &target, double amount) {
	auto *core = static_cast<TeamfightSimulationCore *>(user_data);
	sim::viewer::record_heal_fx(CoordinatorHostAccess::viewer_fx(core), target, amount);
}

void sim_host_viewer_record_shield_fx(void *user_data, const UnitState &target, double amount) {
	auto *core = static_cast<TeamfightSimulationCore *>(user_data);
	sim::viewer::record_shield_fx(CoordinatorHostAccess::viewer_fx(core), target, amount);
}

void sim_host_viewer_record_aoe_shape_fx(
		void *user_data,
		const UnitState &source,
		const UnitState *target,
		const AoShapeParams &params,
		const StringName &kind) {
	auto *core = static_cast<TeamfightSimulationCore *>(user_data);
	sim::SimWorld w = CoordinatorHostAccess::sim_world(core);
	sim::viewer::record_aoe_shape_fx(CoordinatorHostAccess::viewer_fx(core), w, source, target, params, kind);
}

void sim_host_viewer_record_passive_aoe_fx(
		void *user_data,
		const UnitState &unit,
		double radius,
		const StringName &passive_id) {
	auto *core = static_cast<TeamfightSimulationCore *>(user_data);
	sim::viewer::record_passive_aoe_fx(CoordinatorHostAccess::viewer_fx(core), unit, radius, passive_id);
}

Dictionary sim_host_effective_champion_for(void *user_data, const StringName &archetype_id) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_effective_champion_for(archetype_id);
}

EffectRecord sim_host_compile_effect(void *user_data, const Dictionary &effect) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_compile_effect(effect);
}

void sim_host_finalize_reflect_passives(void *user_data, UnitState &unit, UnitStateCold &cold) {
	static_cast<TeamfightSimulationCore *>(user_data)->_finalize_reflect_passives(unit, cold);
}

int64_t sim_host_assign_spawn_slot(void *user_data, const StringName &team) {
	auto *core = static_cast<TeamfightSimulationCore *>(user_data);
	sim::match::SpawnSlotState slots = core->_spawn_slot_state();
	return sim::match::assign_spawn_slot(slots, team);
}

Vector2 sim_host_get_random_spawn_position(void *user_data, const StringName &team, bool is_respawn) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_get_random_spawn_position(team, is_respawn);
}

Dictionary sim_host_coerce_spawn_spec(void *user_data, const Variant &value) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_coerce_match_input(value);
}

uint32_t sim_host_rand_uint32(void *user_data) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_rng.genrand_uint32();
}

double sim_host_randf(void *user_data) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_randf();
}

StringName sim_host_archetype_for_unit(void *user_data, const UnitState &unit) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_uc(unit).archetype_id;
}

void sim_host_print_line(void *user_data, const String &line) {
	(void)user_data;
	UtilityFunctions::print(line);
}

void sim_host_print_score_breakdown(
		void *user_data,
		const ScoreBreakdown &breakdown,
		const StringName &attacker_archetype,
		const StringName &enemy_archetype) {
	static_cast<TeamfightSimulationCore *>(user_data)->_print_score_breakdown(breakdown, attacker_archetype, enemy_archetype);
}

UnitState *sim_host_select_ally_target(void *user_data, UnitState &unit) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_select_ally_target(unit);
}

void sim_host_match_update_projectiles(void *user_data, bool /*profile_sim*/) {
	static_cast<TeamfightSimulationCore *>(user_data)->_update_projectiles();
}

void sim_host_match_prepare_tick_context(void *user_data, bool /*profile_sim*/) {
	static_cast<TeamfightSimulationCore *>(user_data)->_prepare_tick_context();
}

void sim_host_match_update_unit(void *user_data, UnitState &unit, bool profile_sim) {
	static_cast<TeamfightSimulationCore *>(user_data)->_update_unit(unit, profile_sim);
}

match_roster::MatchRosterState sim_host_match_roster_state(void *user_data) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_match_roster_state();
}

unit_builder::UnitBuilderHost sim_host_match_unit_builder_host(void *user_data) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_unit_builder_host();
}

void sim_host_match_profile_reset(void *user_data) {
	static_cast<TeamfightSimulationCore *>(user_data)->_sim_profile_reset();
}

void sim_host_match_profile_emit_json_stderr(void *user_data) {
	static_cast<TeamfightSimulationCore *>(user_data)->_sim_profile_emit_json_stderr();
}

bool sim_host_match_profile_env_enabled(void *user_data) {
	(void)user_data;
	return TeamfightSimulationCore::_sim_profile_env_enabled();
}

void sim_host_unit_tick_respawn(void *user_data, UnitState &unit) {
	auto *core = static_cast<TeamfightSimulationCore *>(user_data);
	sim::SimWorld w = core->_sim_world();
	sim::match_roster::MatchRosterState roster = core->_match_roster_state();
	sim::match::MatchScoreState score = core->_match_score_state();
	score.roster = &roster;
	sim::match::SpawnSlotState slots = core->_spawn_slot_state();
	sim::match::respawn_unit(
			w,
			core->_sim_host_callbacks,
			&core->_viewer_hooks,
			score,
			slots,
			unit);
}

const UnitStrategy &sim_host_unit_tick_strategy(void *user_data, const UnitState &unit) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_strategy_for_unit(unit);
}

UnitState *sim_host_unit_tick_select_enemy_target(void *user_data, UnitState &unit, bool profile_sim) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_select_enemy_target(unit, profile_sim);
}

UnitState *sim_host_unit_tick_select_ally_target(void *user_data, UnitState &unit) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_select_ally_target(unit);
}

void sim_host_unit_tick_prune_assist_window(void *user_data, UnitState &unit) {
	static_cast<TeamfightSimulationCore *>(user_data)->_prune_assist_window(unit);
}

bool sim_host_debug_combat_trace(void *user_data) {
	return static_cast<TeamfightSimulationCore *>(user_data)->_debug_combat_trace;
}

bool sim_host_match_targeting_profile_enabled(void *user_data) {
	(void)user_data;
	return TeamfightSimulationCore::targeting_profile_env_enabled();
}

void sim_host_match_log_sudden_death_draw(void *user_data) {
	static_cast<TeamfightSimulationCore *>(user_data)->_log_sudden_death_draw();
}

} // namespace sim
