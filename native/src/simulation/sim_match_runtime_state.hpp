#ifndef SIM_MATCH_RUNTIME_STATE_HPP
#define SIM_MATCH_RUNTIME_STATE_HPP

#include "sim_match_loop.hpp"
#include "sim_profile_counters.hpp"
#include "sim_world.hpp"

#include <godot_cpp/variant/string_name.hpp>

#include <array>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class TeamfightSimulationCore;

namespace sim {
namespace match {

struct MatchRuntimeState {
	std::vector<UnitState> &units;
	std::vector<UnitStateCold> &unit_cold;
	std::unordered_map<int64_t, int64_t> &unit_index_map;
	std::vector<TargetingFrameEntry> &targeting_frame;
	TickContext &tick_ctx;
	std::vector<int64_t> &alive_player_indices;
	std::vector<int64_t> &alive_enemy_indices;
	std::array<std::vector<int64_t>, SPATIAL_GRID_DIM * SPATIAL_GRID_DIM> &spatial_buckets;
	std::vector<uint32_t> &spatial_stamp;
	uint32_t &spatial_generation;

	double &time;
	double &tick_rate;
	int64_t &tick;
	int64_t &sudden_death_ticks;
	int64_t &player_kills;
	int64_t &enemy_kills;
	StringName &winner_team;
	std::vector<ViewerFxEvent> &viewer_fx_events;

	profile::Counters &profile_counters;
	profile::RuntimeFlags &profile_runtime;

	SimWorld sim_world() const;
	MatchLoopState match_loop_state() const;
};

MatchRuntimeState runtime_from(TeamfightSimulationCore &core);

} // namespace match
} // namespace sim

#endif // SIM_MATCH_RUNTIME_STATE_HPP
