#ifndef SIM_MATCH_BENCHMARK_HPP
#define SIM_MATCH_BENCHMARK_HPP

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

using godot::Dictionary;

class TeamfightSimulationCore;

namespace sim {
namespace match {
namespace benchmark {

void run_generated_matches_simulation_only(
		TeamfightSimulationCore &core,
		int64_t base_seed,
		int64_t batch_count,
		int64_t team_size);

Dictionary run_generated_matches_stats_partial(
		TeamfightSimulationCore &core,
		int64_t base_seed,
		int64_t batch_count,
		int64_t team_size,
		bool include_match_log,
		double tick_rate);

} // namespace benchmark
} // namespace match
} // namespace sim

#endif // SIM_MATCH_BENCHMARK_HPP
