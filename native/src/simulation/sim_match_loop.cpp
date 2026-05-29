#include "sim_match_loop.hpp"

#include <godot_cpp/core/math.hpp>

#include <chrono>

namespace sim {
namespace match {

namespace {

uint64_t profile_elapsed_ns(std::chrono::steady_clock::time_point t0) {
	using namespace std::chrono;
	return duration_cast<nanoseconds>(steady_clock::now() - t0).count();
}

} // namespace

SimWorld make_world(MatchLoopState &state) {
	return SimWorld(
			state.units,
			state.unit_cold,
			state.unit_index_map,
			state.targeting_frame,
			state.tick_ctx,
			state.alive_player_indices,
			state.alive_enemy_indices,
			state.time,
			state.tick_rate,
			&state.spatial_buckets,
			&state.spatial_stamp,
			&state.spatial_generation,
			&state.spatial_fill_cache);
}

StringName determine_winner(int64_t player_kills, int64_t enemy_kills) {
	if (player_kills > enemy_kills) {
		return StringName("player");
	}
	if (enemy_kills > player_kills) {
		return StringName("enemy");
	}
	return StringName("draw");
}

void step_tick(MatchLoopState &state, MatchLoopHost &host, bool profile_sim) {
	state.tick += 1;
	state.time = double(state.tick) * state.tick_rate;
	state.viewer_fx_events.clear();
	if (profile_sim && state.profile.tick_count != nullptr) {
		*state.profile.tick_count += 1;
	}
	for (UnitState &unit : state.units) {
		unit.respawned_this_tick = false;
		unit.cast_resolved_this_tick = false;
		unit.incoming_target_count = 0;
	}
	if (profile_sim) {
		auto t0 = std::chrono::steady_clock::now();
		host.update_projectiles(host.user_data, profile_sim);
		if (state.profile.ns_projectiles != nullptr) {
			*state.profile.ns_projectiles += profile_elapsed_ns(t0);
		}
	} else {
		host.update_projectiles(host.user_data, profile_sim);
	}
	SimWorld world = make_world(state);
	match_roster::process_pending_spawns(
			world,
			host.match_roster_state(host.user_data),
			host.unit_builder_host(host.user_data));
	invalidate_spatial_bucket_fill(world);
	if (profile_sim) {
		auto t0 = std::chrono::steady_clock::now();
		host.prepare_tick_context(host.user_data, profile_sim);
		if (state.profile.ns_prepare_tick_ctx != nullptr) {
			*state.profile.ns_prepare_tick_ctx += profile_elapsed_ns(t0);
		}
	} else {
		host.prepare_tick_context(host.user_data, profile_sim);
	}
	if (profile_sim) {
		auto t0 = std::chrono::steady_clock::now();
		for (UnitState &unit : state.units) {
			host.update_unit(host.user_data, unit, true);
		}
		if (state.profile.ns_update_units != nullptr) {
			*state.profile.ns_update_units += profile_elapsed_ns(t0);
		}
	} else {
		for (UnitState &unit : state.units) {
			host.update_unit(host.user_data, unit, false);
		}
	}
}

void simulate(MatchLoopState &state, MatchLoopHost &host) {
	const bool profile = host.profile_env_enabled != nullptr && host.profile_env_enabled(host.user_data);
	if (state.profile_active != nullptr) {
		*state.profile_active = profile;
	}
	if (state.profile_targeting_active != nullptr && host.targeting_profile_enabled != nullptr) {
		*state.profile_targeting_active = profile && host.targeting_profile_enabled(host.user_data);
	}
	if (profile && host.profile_reset != nullptr) {
		host.profile_reset(host.user_data);
	}
	const double effective_tick_rate = Math::max(state.tick_rate, EPSILON);
	const int64_t max_ticks = int64_t(Math::ceil(MATCH_DURATION / effective_tick_rate));
	for (int64_t tick_index = 0; tick_index < max_ticks; ++tick_index) {
		step_tick(state, host, profile);
	}

	state.sudden_death_ticks = 0;

	if (state.player_kills == state.enemy_kills) {
		while (state.player_kills == state.enemy_kills && state.sudden_death_ticks < SUDDEN_DEATH_MAX_TICKS) {
			step_tick(state, host, profile);
			state.sudden_death_ticks++;
		}

		if (state.sudden_death_ticks >= SUDDEN_DEATH_MAX_TICKS && host.log_sudden_death_draw != nullptr) {
			host.log_sudden_death_draw(host.user_data);
		}
	}

	state.winner_team = determine_winner(state.player_kills, state.enemy_kills);

	if (profile && host.profile_emit_json_stderr != nullptr) {
		host.profile_emit_json_stderr(host.user_data);
	}
	if (state.profile_active != nullptr) {
		*state.profile_active = false;
	}
	if (state.profile_targeting_active != nullptr) {
		*state.profile_targeting_active = false;
	}
}

} // namespace match
} // namespace sim
