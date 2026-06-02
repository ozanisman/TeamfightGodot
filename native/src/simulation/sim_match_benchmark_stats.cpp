#include "sim_match_benchmark.hpp"

#include "../teamfight_simulation_core.hpp"

#include "sim_match_roster.hpp"
#include "sim_match_benchmark_host.hpp"
#include "sim_match_benchmark_stats_internal.hpp"
#include "sim_unit_builder.hpp"

#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <unordered_map>

using namespace godot;

#include "sim_effect_kinds.inl.hpp"

namespace {
using namespace sim::effect_kinds;
using namespace sim::match::benchmark::stats_internal;
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
		std::pair<UnitState, UnitStateCold> built = sim::unit_builder::build_unit(GeneratedMatchHost::unit_builder_host(&core), spawn_spec, team, next_instance_id);
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

	for (int64_t match_index = 0; match_index < batch_count; ++match_index) {
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
		GeneratedMatchHost::simulate_match(&core);

		std::unordered_map<int64_t, double> summoner_minion_damage_dealt;
		std::unordered_map<int64_t, double> summoner_minion_damage_received;
		std::unordered_map<int64_t, double> summoner_minion_damage_mitigated;
		for (UnitState &unit : GeneratedMatchHost::units(&core)) {
			if (unit.summoner_instance_id != 0) {
				const UnitStateCold &c = GeneratedMatchHost::uc(&core, unit);
				summoner_minion_damage_dealt[unit.summoner_instance_id] += c.damage_dealt;
				summoner_minion_damage_received[unit.summoner_instance_id] += c.damage_received;
				summoner_minion_damage_mitigated[unit.summoner_instance_id] += c.damage_mitigated;
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
			match_logs.append(row);
		}

		Dictionary heroes = Dictionary(bucket["heroes"]);
		Dictionary roles = Dictionary(bucket["roles"]);
		for (UnitState &unit : GeneratedMatchHost::units(&core)) {
			const UnitStateCold &c = GeneratedMatchHost::uc(&core, unit);
			if (c.role_id == StringName("minion")) {
				continue;
			}
			const String hero = String(c.unit_id);
			Dictionary hero_entry = Dictionary(heroes.get(hero, Dictionary()));
			if (hero_entry.is_empty()) {
				hero_entry = make_stat_entry();
			}
			const bool unit_won = GeneratedMatchHost::winner_team(&core) != StringName() && unit.team == GeneratedMatchHost::winner_team(&core);
			add_record_with_minions(hero_entry, c, unit.instance_id, unit_won, draw, true, summoner_minion_damage_dealt, summoner_minion_damage_received, summoner_minion_damage_mitigated);
			heroes[hero] = hero_entry;

			const String role = String(Dictionary(c.stats).get("role", String("unknown")));
			Dictionary role_entry = Dictionary(roles.get(role, Dictionary()));
			if (role_entry.is_empty()) {
				role_entry = make_role_entry();
			}
			add_record_with_minions(role_entry, c, unit.instance_id, unit_won, draw, false, summoner_minion_damage_dealt, summoner_minion_damage_received, summoner_minion_damage_mitigated);
			roles[role] = role_entry;
		}
		bucket["heroes"] = heroes;
		bucket["roles"] = roles;

		if (team_size > 1) {
			Dictionary combos = Dictionary(bucket["combos"]);
			const String player_label = sorted_combo_label(GeneratedMatchHost::player_comp(&core));
			Dictionary player_combo = Dictionary(combos.get(player_label, Dictionary()));
			if (player_combo.is_empty()) {
				player_combo["w"] = int64_t(0);
				player_combo["n"] = int64_t(0);
			}
			player_combo["n"] = int64_t(player_combo["n"]) + 1;
			if (player_won) {
				player_combo["w"] = int64_t(player_combo["w"]) + 1;
			}
			combos[player_label] = player_combo;

			const String enemy_label = sorted_combo_label(GeneratedMatchHost::enemy_comp(&core));
			Dictionary enemy_combo = Dictionary(combos.get(enemy_label, Dictionary()));
			if (enemy_combo.is_empty()) {
				enemy_combo["w"] = int64_t(0);
				enemy_combo["n"] = int64_t(0);
			}
			enemy_combo["n"] = int64_t(enemy_combo["n"]) + 1;
			if (enemy_won) {
				enemy_combo["w"] = int64_t(enemy_combo["w"]) + 1;
			}
			combos[enemy_label] = enemy_combo;
			bucket["combos"] = combos;
		}

		if (player_won) {
			record_matchup_result(matchup_data, GeneratedMatchHost::player_comp(&core), GeneratedMatchHost::enemy_comp(&core));
		} else if (enemy_won) {
			record_matchup_result(matchup_data, GeneratedMatchHost::enemy_comp(&core), GeneratedMatchHost::player_comp(&core));
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

	Dictionary by_size;
	by_size[team_size] = bucket;
	Dictionary stats_partial;
	stats_partial["by_size"] = by_size;
	stats_partial["match_logs"] = include_match_log ? match_logs : Array();
	result["stats_partial"] = stats_partial;
	result["matchup_data"] = matchup_data;
	return result;
}

} // namespace benchmark
} // namespace match
} // namespace sim
