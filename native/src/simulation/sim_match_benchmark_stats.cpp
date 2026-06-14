#include "sim_match_benchmark.hpp"

#include "../teamfight_simulation_core.hpp"

#include "sim_match_roster.hpp"
#include "sim_match_benchmark_host.hpp"
#include "sim_match_benchmark_stats_internal.hpp"
#include "sim_unit_builder.hpp"

#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <chrono>
#include <unordered_map>

using namespace godot;

#include "sim_effect_kinds.inl.hpp"

namespace {
using namespace sim::effect_kinds;
using namespace sim::match::benchmark::stats_internal;

uint64_t profile_elapsed_ns(const std::chrono::steady_clock::time_point &t0) {
	return static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count());
}
}

namespace sim {
namespace match {
namespace benchmark {

Dictionary run_generated_matches_stats_partial(TeamfightSimulationCore &core, int64_t base_seed, int64_t batch_count, int64_t team_size, bool include_match_log, double tick_rate) {
	GeneratedMatchHost::ensure_catalog_loaded(&core);
	Dictionary result;
	if (batch_count <= 0) {
		result["stats_partial"] = Dictionary();
		result["matchup_data"] = Dictionary();
		return result;
	}

	Array champion_keys = GeneratedMatchHost::catalog(&core).effective_champion_by_archetype.keys();
	const int64_t champion_count = champion_keys.size();
	if (champion_count <= 0) {
		UtilityFunctions::push_error("TeamfightSimulationCore.run_generated_matches_stats_partial() requires champion catalog.");
		result["stats_partial"] = Dictionary();
		result["matchup_data"] = Dictionary();
		return result;
	}

	const int64_t units_per_team = Math::max(int64_t(1), team_size);
	const std::vector<StringName> archetypes = load_archetypes(champion_keys);

	auto append_generated_unit = [&core](
			match_roster::MatchRosterState &roster,
			Dictionary &spawn_spec,
			const StringName &team,
			const StringName &archetype,
			int64_t &next_instance_id,
			Array &comp) {
		spawn_spec.clear();
		spawn_spec["unit_id"] = archetype;
		sim::SimWorld w = GeneratedMatchHost::sim_world(&core);
		sim::unit_builder::BuiltUnit built = sim::unit_builder::build_unit(GeneratedMatchHost::unit_builder_host(&core), spawn_spec, team, next_instance_id);
		const int64_t unit_index = sim::match_roster::register_built_unit(w, roster, std::move(built), team, next_instance_id);
		if (unit_index < 0) {
			return;
		}
		comp.append(GeneratedMatchHost::unit_cold_at(&core, static_cast<size_t>(unit_index)).unit_id);
		next_instance_id += 1;
	};

	Dictionary bucket;
	bucket["p1"] = int64_t(0);
	bucket["p2"] = int64_t(0);
	bucket["draws"] = int64_t(0);
	bucket["total"] = int64_t(0);
	bucket["heroes"] = Dictionary();
	bucket["roles"] = Dictionary();
	bucket["combos"] = Dictionary();
	Array match_logs;
	Dictionary matchup_data;
	Dictionary spawn_spec;
	int64_t next_progress = 0;
	uint64_t setup_ns = 0;
	uint64_t simulate_ns = 0;
	uint64_t stats_aggregation_ns = 0;
	uint64_t matchup_aggregation_ns = 0;
	uint64_t reset_ns = 0;
	uint64_t progress_ns = 0;

	for (int64_t match_index = 0; match_index < batch_count; ++match_index) {
		auto phase_start = std::chrono::steady_clock::now();
		const int64_t seed = base_seed + match_index;
		GeneratedMatchHost::seed_generated_match(&core, seed, tick_rate);
		match_roster::MatchRosterState roster = GeneratedMatchHost::match_roster_state(&core);

		Ref<RandomNumberGenerator> draft_rng;
		draft_rng.instantiate();
		draft_rng->set_seed(uint64_t(seed));
		int64_t next_instance_id = 1;
		if (champion_count < units_per_team * 2) {
			for (int64_t slot = 0; slot < units_per_team; ++slot) {
				const int64_t archetype_index = pick_index(draft_rng, champion_count - 1);
				append_generated_unit(roster, spawn_spec, sn_player(), archetypes[static_cast<size_t>(archetype_index)], next_instance_id, GeneratedMatchHost::player_comp(&core));
			}
			for (int64_t slot = 0; slot < units_per_team; ++slot) {
				const int64_t archetype_index = pick_index(draft_rng, champion_count - 1);
				append_generated_unit(roster, spawn_spec, sn_enemy(), archetypes[static_cast<size_t>(archetype_index)], next_instance_id, GeneratedMatchHost::enemy_comp(&core));
			}
		} else {
			std::vector<int64_t> indices;
			indices.reserve(static_cast<size_t>(champion_count));
			for (int64_t i = 0; i < champion_count; ++i) {
				indices.push_back(i);
			}
			for (int64_t i = champion_count - 1; i > 0; --i) {
				const int64_t j = pick_index(draft_rng, i);
				std::swap(indices[static_cast<size_t>(i)], indices[static_cast<size_t>(j)]);
			}
			for (int64_t slot = 0; slot < units_per_team; ++slot) {
				append_generated_unit(roster, spawn_spec, sn_player(), archetypes[static_cast<size_t>(indices[static_cast<size_t>(slot)])], next_instance_id, GeneratedMatchHost::player_comp(&core));
			}
			for (int64_t slot = 0; slot < units_per_team; ++slot) {
				const int64_t source_index = slot + units_per_team;
				append_generated_unit(roster, spawn_spec, sn_enemy(), archetypes[static_cast<size_t>(indices[static_cast<size_t>(source_index)])], next_instance_id, GeneratedMatchHost::enemy_comp(&core));
			}
		}
		GeneratedMatchHost::build_role_strategy_cache(&core);
		GeneratedMatchHost::prepare_tick_context(&core);
		setup_ns += profile_elapsed_ns(phase_start);

		phase_start = std::chrono::steady_clock::now();
		GeneratedMatchHost::simulate_match(&core);
		simulate_ns += profile_elapsed_ns(phase_start);

		phase_start = std::chrono::steady_clock::now();
		std::unordered_map<int64_t, double> summoner_minion_damage_dealt;
		std::unordered_map<int64_t, double> summoner_minion_damage_received;
		std::unordered_map<int64_t, double> summoner_minion_damage_mitigated;
		for (UnitState &unit : GeneratedMatchHost::units(&core)) {
			if (unit.summoner_instance_id != 0) {
				const UnitStateRare &r = GeneratedMatchHost::ur(&core, unit);
				summoner_minion_damage_dealt[unit.summoner_instance_id] += r.damage_dealt;
				summoner_minion_damage_received[unit.summoner_instance_id] += r.damage_received;
				summoner_minion_damage_mitigated[unit.summoner_instance_id] += r.damage_mitigated;
			}
		}

		const bool player_won = GeneratedMatchHost::winner_team(&core) == sn_player();
		const bool enemy_won = GeneratedMatchHost::winner_team(&core) == sn_enemy();
		const bool draw = !player_won && !enemy_won;
		if (player_won) {
			bucket["p1"] = int64_t(bucket["p1"]) + 1;
		} else if (enemy_won) {
			bucket["p2"] = int64_t(bucket["p2"]) + 1;
		} else {
			bucket["draws"] = int64_t(bucket["draws"]) + 1;
		}
		bucket["total"] = int64_t(bucket["total"]) + 1;
		if (include_match_log) {
			Dictionary row;
			row["team_size"] = team_size;
			row["seed"] = GeneratedMatchHost::seed(&core);
			row["winner"] = String(GeneratedMatchHost::winner_team(&core));
			row["sudden_death_ticks"] = int64_t(GeneratedMatchHost::sudden_death_ticks(&core));
			row["duration"] = GeneratedMatchHost::time(&core);
			row["player_comp"] = GeneratedMatchHost::player_comp(&core).duplicate();
			row["enemy_comp"] = GeneratedMatchHost::enemy_comp(&core).duplicate();
			match_logs.append(row);
		}

		Dictionary heroes = Dictionary(bucket["heroes"]);
		Dictionary roles = Dictionary(bucket["roles"]);
		Array player_roles;
		Array enemy_roles;
		for (UnitState &unit : GeneratedMatchHost::units(&core)) {
			const UnitStateCold &c = GeneratedMatchHost::uc(&core, unit);
			const UnitStateRare &r = GeneratedMatchHost::ur(&core, unit);
			if (c.role_id == StringName("minion")) {
				continue;
			}
			const String hero = String(c.unit_id);
			Dictionary hero_entry = Dictionary(heroes.get(hero, Dictionary()));
			if (hero_entry.is_empty()) {
				hero_entry = make_stat_entry();
			}
			const bool unit_won = GeneratedMatchHost::winner_team(&core) != StringName() && unit.team == GeneratedMatchHost::winner_team(&core);
			add_record_with_minions(hero_entry, r, unit.instance_id, unit_won, draw, true, summoner_minion_damage_dealt, summoner_minion_damage_received, summoner_minion_damage_mitigated);
			heroes[hero] = hero_entry;

			const String role = String(c.role_id);
			Dictionary role_entry = Dictionary(roles.get(role, Dictionary()));
			if (role_entry.is_empty()) {
				role_entry = make_role_entry();
			}
			add_record_with_minions(role_entry, r, unit.instance_id, unit_won, draw, false, summoner_minion_damage_dealt, summoner_minion_damage_received, summoner_minion_damage_mitigated);
			roles[role] = role_entry;

			// Collect roles for matchup tracking
			if (unit.team == sn_player()) {
				player_roles.append(role);
			} else {
				enemy_roles.append(role);
			}
		}
		bucket["heroes"] = heroes;
		bucket["roles"] = roles;

		if (team_size > 1) {
			Dictionary combos = Dictionary(bucket["combos"]);
			const String player_role_fp = sorted_role_label(player_roles);
			const String enemy_role_fp = sorted_role_label(enemy_roles);

			// Record player team fingerprint
			Dictionary player_combo = Dictionary(combos.get(player_role_fp, Dictionary()));
			if (player_combo.is_empty()) {
				player_combo["w"] = int64_t(0);
				player_combo["n"] = int64_t(0);
			}
			player_combo["n"] = int64_t(player_combo["n"]) + 1;
			if (player_won) {
				player_combo["w"] = int64_t(player_combo["w"]) + 1;
			}
			combos[player_role_fp] = player_combo;

			// Record enemy team fingerprint
			Dictionary enemy_combo = Dictionary(combos.get(enemy_role_fp, Dictionary()));
			if (enemy_combo.is_empty()) {
				enemy_combo["w"] = int64_t(0);
				enemy_combo["n"] = int64_t(0);
			}
			enemy_combo["n"] = int64_t(enemy_combo["n"]) + 1;
			if (enemy_won) {
				enemy_combo["w"] = int64_t(enemy_combo["w"]) + 1;
			}
			combos[enemy_role_fp] = enemy_combo;
			bucket["combos"] = combos;
		}
		stats_aggregation_ns += profile_elapsed_ns(phase_start);

		phase_start = std::chrono::steady_clock::now();
		if (player_won) {
			record_matchup_result(matchup_data, GeneratedMatchHost::player_comp(&core), GeneratedMatchHost::enemy_comp(&core));
		} else if (enemy_won) {
			record_matchup_result(matchup_data, GeneratedMatchHost::enemy_comp(&core), GeneratedMatchHost::player_comp(&core));
		}
		matchup_aggregation_ns += profile_elapsed_ns(phase_start);

		next_progress += 1;
		if (next_progress == 1000) {
			phase_start = std::chrono::steady_clock::now();
			core.benchmark_progress_add(1000);
			progress_ns += profile_elapsed_ns(phase_start);
			next_progress = 0;
		}
	}
	if (next_progress != 0) {
		const auto phase_start = std::chrono::steady_clock::now();
		core.benchmark_progress_add(next_progress);
		progress_ns += profile_elapsed_ns(phase_start);
	}

	const auto cleanup_start = std::chrono::steady_clock::now();
	GeneratedMatchHost::reset_runtime_state(&core);
	reset_ns += profile_elapsed_ns(cleanup_start);

	const auto result_build_start = std::chrono::steady_clock::now();
	Dictionary by_size;
	by_size[team_size] = bucket;
	Dictionary stats_partial;
	stats_partial["by_size"] = by_size;
	stats_partial["match_logs"] = include_match_log ? match_logs : Array();
	result["stats_partial"] = stats_partial;
	result["matchup_data"] = matchup_data;
	Dictionary native_profile;
	native_profile["setup_ns"] = int64_t(setup_ns);
	native_profile["simulate_ns"] = int64_t(simulate_ns);
	native_profile["stats_aggregation_ns"] = int64_t(stats_aggregation_ns);
	native_profile["matchup_aggregation_ns"] = int64_t(matchup_aggregation_ns);
	native_profile["reset_ns"] = int64_t(reset_ns);
	native_profile["progress_ns"] = int64_t(progress_ns);
	native_profile["result_build_ns"] = int64_t(profile_elapsed_ns(result_build_start));
	result["native_profile"] = native_profile;
	return result;
}

} // namespace benchmark
} // namespace match
} // namespace sim
