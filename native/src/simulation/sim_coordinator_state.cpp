#include "../teamfight_simulation_core.hpp"

#include "sim_constants.hpp"
#include "sim_coordinator_host.hpp"
#include "sim_match_runtime_state.hpp"
#include "sim_stats_modifiers.hpp"

using namespace sim;

sim::unit_builder::UnitBuilderHost TeamfightSimulationCore::_unit_builder_host() const {
	return _match_ctx.unit_builder_host;
}

sim::match_roster::MatchRosterState TeamfightSimulationCore::_match_roster_state() {
	return {
		_max_instance_id,
		_alive_player_indices_set,
		_alive_enemy_indices_set,
		_pending_spawns,
	};
}

sim::match::MatchScoreState TeamfightSimulationCore::_match_score_state() {
	sim::match::MatchScoreState score;
	score.player_kills = &_player_kills;
	score.enemy_kills = &_enemy_kills;
	score.tick_ctx = &_tick_ctx;
	score.time = _time;
	return score;
}

sim::match::SpawnSlotState TeamfightSimulationCore::_spawn_slot_state() {
	return {
		_player_spawn_slots_used,
		_enemy_spawn_slots_used,
		&sim_host_rand_uint32,
		&sim_host_get_random_spawn_position,
		this,
	};
}

sim::match::MatchLoopHost TeamfightSimulationCore::_match_loop_host() const {
	return _match_ctx.loop_host;
}

void TeamfightSimulationCore::_reset_runtime_state() {
	for (sim::UnitState &unit : _units) {
		sim::stats_modifiers::clear_all_stat_modifiers(unit);
	}

	_units.clear();
	_unit_cold.clear();
	_projectiles.clear();
	_projectiles.reserve(128);
	_scratch_projectiles.clear();
	_active_projectiles.clear();
	_scratch_critical_allies.clear();

	_summary_unit_stats.clear();
	_summary_cache.clear();
	_time = 0.0;
	_tick = 0;
	_sudden_death_ticks = 0;
	_tick_rate = sim::DEFAULT_TICK_RATE;
	_seed = 0;
	_winner_team = StringName("draw");
	_record_events = false;
	_player_comp.clear();
	_enemy_comp.clear();
	_player_kills = 0;
	_enemy_kills = 0;
	_max_instance_id = 0;

	_player_spawn_slots_used.clear();
	_enemy_spawn_slots_used.clear();

	_unit_index_map.clear();
	_alive_player_indices.clear();
	_alive_enemy_indices.clear();
	_alive_player_indices_set.clear();
	_alive_enemy_indices_set.clear();
	_targeting_frame.clear();
	_role_strategy_cache_by_slot.fill(UnitStrategy());
	_default_strategy = UnitStrategy();
	_tick_ctx.density_by_unit_index.clear();
	_tick_ctx.player_backliner_indices.clear();
	_tick_ctx.enemy_backliner_indices.clear();
	_tick_ctx.player_backliner_alive_count = 0;
	_tick_ctx.enemy_backliner_alive_count = 0;
	_tick_ctx.has_player_center = false;
	_tick_ctx.has_enemy_center = false;
	_trace_buffer.clear();
	_debug_combat_trace = false;
	_debug_targeting_scoring = false;
	_viewer_fx.clear();
	_pending_spawns.clear();
	if (!_match_context_hosts_cached) {
		_refresh_match_context();
		_match_context_hosts_cached = true;
	}
}
