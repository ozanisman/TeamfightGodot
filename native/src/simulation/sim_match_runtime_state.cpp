#include "sim_match_runtime_state.hpp"

#include "sim_coordinator_host.hpp"

#include "../teamfight_simulation_core.hpp"

namespace sim {
namespace match {

SimWorld MatchRuntimeState::sim_world() const {
	return SimWorld(
			units,
			unit_cold,
			unit_index_map,
			targeting_frame,
			tick_ctx,
			alive_player_indices,
			alive_enemy_indices,
			time,
			tick_rate,
			&spatial_buckets,
			&spatial_stamp,
			&spatial_generation);
}

MatchLoopState MatchRuntimeState::match_loop_state() const {
	MatchLoopState state{
		units,
		unit_cold,
		unit_index_map,
		targeting_frame,
		tick_ctx,
		alive_player_indices,
		alive_enemy_indices,
		spatial_buckets,
		spatial_stamp,
		spatial_generation,
		tick,
		time,
		tick_rate,
		sudden_death_ticks,
		player_kills,
		enemy_kills,
		winner_team,
		viewer_fx_events,
	};
	state.profile.ns_projectiles = &profile_counters.ns_projectiles;
	state.profile.ns_prepare_tick_ctx = &profile_counters.ns_prepare_tick_ctx;
	state.profile.ns_update_units = &profile_counters.ns_update_units;
	state.profile.tick_count = &profile_counters.tick_count;
	state.profile_active = &profile_runtime.sim_active;
	state.profile_targeting_active = &profile_runtime.targeting_active;
	return state;
}

MatchRuntimeState runtime_from(TeamfightSimulationCore &core) {
	return MatchRuntimeState{
		CoordinatorHostAccess::units(&core),
		CoordinatorHostAccess::unit_cold(&core),
		core._unit_index_map,
		core._targeting_frame,
		core._tick_ctx,
		core._alive_player_indices,
		core._alive_enemy_indices,
		core._spatial_buckets,
		core._spatial_stamp,
		core._spatial_generation,
		core._time,
		core._tick_rate,
		core._tick,
		core._sudden_death_ticks,
		core._player_kills,
		core._enemy_kills,
		core._winner_team,
		core._viewer_fx.events,
		core._sim_profile_counters,
		core._sim_profile_runtime,
	};
}

} // namespace match
} // namespace sim
