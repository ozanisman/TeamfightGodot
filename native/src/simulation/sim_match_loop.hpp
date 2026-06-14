#ifndef SIM_MATCH_LOOP_HPP
#define SIM_MATCH_LOOP_HPP

#include "sim_constants.hpp"
#include "sim_match_roster.hpp"
#include "sim_unit_builder.hpp"
#include "sim_world.hpp"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <array>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sim {
namespace match {

struct MatchLoopProfileCounters {
	uint64_t *ns_projectiles = nullptr;
	uint64_t *ns_prepare_tick_ctx = nullptr;
	uint64_t *ns_update_units = nullptr;
	int64_t *tick_count = nullptr;
};

struct MatchLoopState {
	std::vector<UnitState> &units;
	std::vector<UnitStateCold> &unit_cold;
	std::vector<UnitStateRare> &unit_rare;
	std::unordered_map<int64_t, int64_t> &unit_index_map;
	std::vector<TargetingFrameEntry> &targeting_frame;
	TickContext &tick_ctx;
	std::vector<int64_t> &alive_player_indices;
	std::vector<int64_t> &alive_enemy_indices;
	std::array<std::vector<int64_t>, SPATIAL_GRID_DIM * SPATIAL_GRID_DIM> &spatial_buckets;
	std::vector<uint32_t> &spatial_stamp;
	uint32_t &spatial_generation;
	SpatialBucketFillCache &spatial_fill_cache;

	int64_t &tick;
	double &time;
	double &tick_rate;
	int64_t &sudden_death_ticks;
	int64_t &player_kills;
	int64_t &enemy_kills;
	StringName &winner_team;
	std::vector<ViewerFxEvent> &viewer_fx_events;

	MatchLoopProfileCounters profile;
	bool *profile_active = nullptr;
	bool *profile_targeting_active = nullptr;
};

struct MatchLoopHost {
	void *user_data = nullptr;

	void (*update_projectiles)(void *user_data, bool profile_sim) = nullptr;
	void (*prepare_tick_context)(void *user_data, bool profile_sim) = nullptr;
	void (*update_unit)(void *user_data, UnitState &unit, bool profile_sim) = nullptr;
	match_roster::MatchRosterState (*match_roster_state)(void *user_data) = nullptr;
	unit_builder::UnitBuilderHost (*unit_builder_host)(void *user_data) = nullptr;
	void (*profile_reset)(void *user_data) = nullptr;
	void (*profile_emit_json_stderr)(void *user_data) = nullptr;
	bool (*profile_env_enabled)(void *user_data) = nullptr;
	bool (*targeting_profile_enabled)(void *user_data) = nullptr;
	void (*log_sudden_death_draw)(void *user_data) = nullptr;
};

SimWorld make_world(MatchLoopState &state);

StringName determine_winner(int64_t player_kills, int64_t enemy_kills);

void step_tick(MatchLoopState &state, MatchLoopHost &host, bool profile_sim);

void simulate(MatchLoopState &state, MatchLoopHost &host);

} // namespace match
} // namespace sim

#endif // SIM_MATCH_LOOP_HPP
