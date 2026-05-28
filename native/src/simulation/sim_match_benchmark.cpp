#include "sim_match_benchmark.hpp"

#include "../teamfight_simulation_core.hpp"

#include "sim_constants.hpp"
#include "sim_match_loop.hpp"
#include "sim_match_roster.hpp"
#include "sim_match_benchmark_host.hpp"
#include "sim_unit_builder.hpp"

#include "../python_random.hpp"

#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

using namespace godot;

#include "sim_effect_kinds.inl.hpp"

namespace {

using namespace sim::effect_kinds;

uint64_t sim_profile_elapsed_ns(std::chrono::steady_clock::time_point t0) {
	return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count());
}

bool bench_phases_env_enabled() {
	const char *v = std::getenv("TEAMFIGHT_BENCH_PHASES");
	if (v == nullptr || v[0] == '\0') {
		return false;
	}
	return !(v[0] == '0' && v[1] == '\0');
}

} // namespace

namespace sim {
namespace match {
namespace benchmark {

void run_generated_matches_simulation_only(TeamfightSimulationCore &core, int64_t base_seed, int64_t batch_count, int64_t team_size) {
	if (batch_count <= 0) {
		return;
	}
	const bool bench_phases = bench_phases_env_enabled();
	uint64_t ns_catalog_ensure = 0;
	uint64_t ns_chunk_preamble = 0;
	uint64_t ns_match_setup_total = 0;
	uint64_t ns_simulate_total = 0;
	auto t_catalog0 = std::chrono::steady_clock::now();
	GeneratedMatchHost::ensure_catalog_loaded(&core);
	if (bench_phases) {
		ns_catalog_ensure = sim_profile_elapsed_ns(t_catalog0);
	}
	auto t_preamble0 = std::chrono::steady_clock::now();
	Array champion_keys = GeneratedMatchHost::catalog(&core).effective_champion_by_archetype.keys();
	const int64_t champion_count = champion_keys.size();
	if (champion_count <= 0) {
		UtilityFunctions::push_error("TeamfightSimulationCore.run_generated_matches_simulation_only() requires champion catalog.");
		return;
	}
	const int64_t units_per_team = Math::max(int64_t(1), team_size);
	std::vector<StringName> archetypes;
	archetypes.reserve(static_cast<size_t>(champion_count));
	for (int64_t index = 0; index < champion_count; ++index) {
		archetypes.push_back(StringName(String(champion_keys[index])));
	}
	Dictionary spawn_spec;
	int64_t next_progress = 0;
	if (bench_phases) {
		ns_chunk_preamble = sim_profile_elapsed_ns(t_preamble0);
	}
	for (int64_t match_index = 0; match_index < batch_count; ++match_index) {
		auto t_match0 = std::chrono::steady_clock::now();
		const int64_t seed = base_seed + match_index;
		GeneratedMatchHost::seed_generated_match(&core, seed, DEFAULT_TICK_RATE);
		CPythonRandom draft_rng;
		draft_rng.seed_int64(seed);
		int64_t next_instance_id = 1;
		
		// Create a mutable copy of archetypes for selection
		std::vector<StringName> available_archetypes = archetypes;
		
		// Generate player team
		for (int64_t slot = 0; slot < units_per_team; ++slot) {
			if (available_archetypes.empty()) {
				break; // No more champions available
			}
			const size_t archetype_index = static_cast<size_t>(draft_rng.genrand_uint32() % uint32_t(available_archetypes.size()));
			StringName selected_archetype = available_archetypes[archetype_index];
			
			// Remove selected archetype from available pool
			available_archetypes.erase(available_archetypes.begin() + archetype_index);
			
			spawn_spec.clear();
			spawn_spec["archetype_id"] = selected_archetype;
			sim::SimWorld w = GeneratedMatchHost::sim_world(&core);
			std::pair<UnitState, UnitStateCold> built =
					sim::unit_builder::build_unit(GeneratedMatchHost::unit_builder_host(&core), spawn_spec, sn_player(), next_instance_id);
			const int64_t unit_index = sim::match_roster::register_built_unit(
					w, GeneratedMatchHost::match_roster_state(&core), std::move(built), sn_player(), next_instance_id);
			if (unit_index < 0) {
				continue;
			}
			GeneratedMatchHost::player_comp(&core).append(GeneratedMatchHost::unit_cold_at(&core, static_cast<size_t>(unit_index)).archetype_id);
			next_instance_id += 1;
		}
		
		// Generate enemy team from remaining champions
		for (int64_t slot = 0; slot < units_per_team; ++slot) {
			if (available_archetypes.empty()) {
				break; // No more champions available
			}
			const size_t archetype_index = static_cast<size_t>(draft_rng.genrand_uint32() % uint32_t(available_archetypes.size()));
			StringName selected_archetype = available_archetypes[archetype_index];
			
			// Remove selected archetype from available pool
			available_archetypes.erase(available_archetypes.begin() + archetype_index);
			
			spawn_spec.clear();
			spawn_spec["archetype_id"] = selected_archetype;
			sim::SimWorld w = GeneratedMatchHost::sim_world(&core);
			std::pair<UnitState, UnitStateCold> built =
					sim::unit_builder::build_unit(GeneratedMatchHost::unit_builder_host(&core), spawn_spec, sn_enemy(), next_instance_id);
			const int64_t unit_index = sim::match_roster::register_built_unit(
					w, GeneratedMatchHost::match_roster_state(&core), std::move(built), sn_enemy(), next_instance_id);
			if (unit_index < 0) {
				continue;
			}
			GeneratedMatchHost::enemy_comp(&core).append(GeneratedMatchHost::unit_cold_at(&core, static_cast<size_t>(unit_index)).archetype_id);
			next_instance_id += 1;
		}
		GeneratedMatchHost::build_role_strategy_cache(&core);
		GeneratedMatchHost::prepare_tick_context(&core);
		if (bench_phases) {
			ns_match_setup_total += sim_profile_elapsed_ns(t_match0);
		}
		auto t_sim0 = std::chrono::steady_clock::now();
		GeneratedMatchHost::simulate_match(&core);
		if (bench_phases) {
			ns_simulate_total += sim_profile_elapsed_ns(t_sim0);
		}
		GeneratedMatchHost::reset_runtime_state(&core);
		next_progress += 1;
		if (next_progress == 1000) {
			core.benchmark_progress_add(1000);
			next_progress = 0;
		}
	}
	if (next_progress != 0) {
		core.benchmark_progress_add(next_progress);
	}
	if (bench_phases) {
		const double inv_bc = batch_count > 0 ? 1.0 / double(batch_count) : 0.0;
		std::fprintf(stderr,
				"{\"bench_phases\":{\"batch_count\":%lld,\"ns_catalog_ensure\":%llu,\"ns_chunk_preamble\":%llu,"
				"\"ns_match_setup_total\":%llu,\"ns_simulate_total\":%llu,"
				"\"avg_ns_per_match_setup\":%.0f,\"avg_ns_per_match_simulate\":%.0f}}\n",
				static_cast<long long>(batch_count),
				static_cast<unsigned long long>(ns_catalog_ensure),
				static_cast<unsigned long long>(ns_chunk_preamble),
				static_cast<unsigned long long>(ns_match_setup_total),
				static_cast<unsigned long long>(ns_simulate_total),
				double(ns_match_setup_total) * inv_bc,
				double(ns_simulate_total) * inv_bc);
		std::fflush(stderr);
	}
}


} // namespace benchmark
} // namespace match
} // namespace sim
