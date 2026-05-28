#ifndef SIM_MATCH_HPP
#define SIM_MATCH_HPP

#include "simulation_types.hpp"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <cstdint>
#include <vector>

namespace sim {
namespace match {

struct MatchSnapshot {
	int64_t seed = 0;
	StringName winner_team;
	double time = 0.0;
	int64_t sudden_death_ticks = 0;
	int64_t player_kills = 0;
	int64_t enemy_kills = 0;
	Array player_comp;
	Array enemy_comp;
};

Dictionary build_summary(
		const MatchSnapshot &match,
		const std::vector<UnitState> &units,
		const std::vector<UnitStateCold> &unit_cold,
		Dictionary &summary_cache,
		Array &summary_unit_stats);

Dictionary build_stats_summary(
		const MatchSnapshot &match,
		const std::vector<UnitState> &units,
		const std::vector<UnitStateCold> &unit_cold);

} // namespace match
} // namespace sim

#endif // SIM_MATCH_HPP
