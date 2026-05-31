#include "../teamfight_simulation_core.hpp"

#include "sim_channel.hpp"
#include "sim_combat.hpp"
#include "sim_coordinator_host.hpp"
#include "sim_effects_host.hpp"
#include "sim_match_loop.hpp"
#include "sim_unit_builder.hpp"
#include "sim_unit_tick.hpp"

using namespace sim;

sim::combat::CombatHostHooks TeamfightSimulationCore::_combat_host_hooks() const {
	sim::combat::CombatHostHooks hooks;
	hooks.user_data = const_cast<TeamfightSimulationCore *>(this);
	hooks.select_ally_target = &sim_host_select_ally_target;
	return hooks;
}

sim::unit_tick::UnitTickHostHooks TeamfightSimulationCore::_unit_tick_host_hooks() const {
	sim::unit_tick::UnitTickHostHooks hooks;
	hooks.user_data = const_cast<TeamfightSimulationCore *>(this);
	hooks.respawn_unit = &sim_host_unit_tick_respawn;
	hooks.strategy_for_unit = &sim_host_unit_tick_strategy;
	hooks.select_enemy_target = &sim_host_unit_tick_select_enemy_target;
	hooks.select_ally_target = &sim_host_unit_tick_select_ally_target;
	hooks.prune_assist_window = &sim_host_unit_tick_prune_assist_window;
	return hooks;
}

sim::channel::ChannelHostHooks TeamfightSimulationCore::_channel_host_hooks() const {
	sim::channel::ChannelHostHooks hooks;
	hooks.user_data = const_cast<TeamfightSimulationCore *>(this);
	hooks.debug_combat_trace = &sim_host_debug_combat_trace;
	return hooks;
}

void TeamfightSimulationCore::_refresh_match_context() {
	sim::unit_builder::UnitBuilderHost &host = _match_ctx.unit_builder_host;
	host.user_data = this;
	host.catalog = &_catalog;
	host.compile_effect = &sim_host_compile_effect;
	host.effective_champion_for = &sim_host_effective_champion_for;
	host.finalize_reflect_passives = &sim_host_finalize_reflect_passives;
	host.assign_spawn_slot = &sim_host_assign_spawn_slot;
	host.get_random_spawn_position = &sim_host_get_random_spawn_position;

	sim::match::MatchLoopHost &loop = _match_ctx.loop_host;
	loop.user_data = this;
	loop.update_projectiles = &sim_host_match_update_projectiles;
	loop.prepare_tick_context = &sim_host_match_prepare_tick_context;
	loop.update_unit = &sim_host_match_update_unit;
	loop.match_roster_state = &sim_host_match_roster_state;
	loop.unit_builder_host = &sim_host_match_unit_builder_host;
	loop.profile_reset = &sim_host_match_profile_reset;
	loop.profile_emit_json_stderr = &sim_host_match_profile_emit_json_stderr;
	loop.profile_env_enabled = &sim_host_match_profile_env_enabled;
	loop.targeting_profile_enabled = &sim_host_match_targeting_profile_enabled;
	loop.log_sudden_death_draw = &sim_host_match_log_sudden_death_draw;
}

void TeamfightSimulationCore::_bind_effect_exec_bindings() {
	_effect_exec_bindings.units = &_units;
	_effect_exec_bindings.unit_cold = &_unit_cold;
	_effect_exec_bindings.unit_index_map = &_unit_index_map;
	_effect_exec_bindings.targeting_frame = &_targeting_frame;
	_effect_exec_bindings.tick_ctx = &_tick_ctx;
	_effect_exec_bindings.alive_player_indices = &_alive_player_indices;
	_effect_exec_bindings.alive_enemy_indices = &_alive_enemy_indices;
	_effect_exec_bindings.time = &_time;
	_effect_exec_bindings.tick_rate = &_tick_rate;
	_effect_exec_bindings.spatial_buckets = &_spatial_buckets;
	_effect_exec_bindings.spatial_stamp = &_spatial_stamp;
	_effect_exec_bindings.spatial_generation = &_spatial_generation;
	_effect_exec_bindings.spatial_fill_cache = &_spatial_fill_cache;
	_effect_exec_bindings.match_host.pending_spawns = &_pending_spawns;
	_effect_exec_bindings.match_host.projectiles = &_projectiles;
	_effect_exec_bindings.match_host.max_instance_id = &_max_instance_id;
	_effect_exec_bindings.match_host.catalog = &_catalog;
	_effect_exec_bindings.exec_callbacks = &_sim_exec_callbacks;
}

void TeamfightSimulationCore::_bind_sim_host() {
	_bind_effect_exec_bindings();
	_sim_host_callbacks.user_data = this;
	_sim_host_callbacks.effect_exec = &_effect_exec_bindings;
	_sim_host_callbacks.execute_effect = &sim::effects::host_execute_effect;
	_sim_host_callbacks.handle_death = &sim_host_handle_death;
	_sim_host_callbacks.sync_targeting_frame_unit = &sim_host_sync_targeting_frame_unit;
	_sim_host_callbacks.sync_targeting_frame_index = &sim_host_sync_targeting_frame_index;
	_sim_host_callbacks.emit_trace = &sim_host_emit_trace;
	_sim_host_callbacks.viewer_record_damage_fx = &sim_host_viewer_record_damage_fx;
	_sim_host_callbacks.viewer_record_heal_fx = &sim_host_viewer_record_heal_fx;
	_sim_host_callbacks.viewer_record_shield_fx = &sim_host_viewer_record_shield_fx;
	_sim_host_callbacks.viewer_record_hot_status_fx = &sim_host_viewer_record_hot_status_fx;
	_sim_host_callbacks.randf = &sim_host_randf;
	_viewer_hooks.user_data = this;
	_viewer_hooks.record_damage_fx = &sim_host_viewer_record_damage_fx;
	_viewer_hooks.record_heal_fx = &sim_host_viewer_record_heal_fx;
	_viewer_hooks.record_shield_fx = &sim_host_viewer_record_shield_fx;
	_viewer_hooks.record_hot_status_fx = &sim_host_viewer_record_hot_status_fx;
	_viewer_hooks.record_aoe_shape_fx = &sim_host_viewer_record_aoe_shape_fx;
	_viewer_hooks.record_passive_aoe_fx = &sim_host_viewer_record_passive_aoe_fx;
	_viewer_hooks.record_unit_death_fx = &sim_host_viewer_record_unit_death_fx;
	_viewer_hooks.sync_targeting_frame_unit = &sim_host_sync_targeting_frame_unit;
	_sim_host_callbacks.viewer_hooks = &_viewer_hooks;
	_bind_sim_exec_hooks();
}

void TeamfightSimulationCore::_bind_sim_exec_hooks() {
	_sim_exec_callbacks.debug_combat_trace = &sim_host_debug_combat_trace;
}
