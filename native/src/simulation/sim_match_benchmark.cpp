#include "sim_match_benchmark.hpp"

#include "../teamfight_simulation_core.hpp"

#include "sim_constants.hpp"
#include "sim_match_loop.hpp"
#include "sim_match_roster.hpp"
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

namespace {

inline const StringName &sn_player() {
	static const StringName s("player");
	return s;
}
inline const StringName &sn_enemy() {
	static const StringName s("enemy");
	return s;
}

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
	core._ensure_catalog_loaded();
	if (bench_phases) {
		ns_catalog_ensure = sim_profile_elapsed_ns(t_catalog0);
	}
	auto t_preamble0 = std::chrono::steady_clock::now();
	Array champion_keys = core._catalog.effective_champion_by_archetype.keys();
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
		core._reset_runtime_state();
		core._seed = seed;
		core._tick_rate = DEFAULT_TICK_RATE;
		core._record_events = false;
		core._debug_combat_trace = false;
		core._trace_buffer.clear();
		core._rng.seed_int64(core._seed);
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
			sim::SimWorld w = core._sim_world();
			std::pair<UnitState, UnitStateCold> built =
					sim::unit_builder::build_unit(core._unit_builder_host(), spawn_spec, sn_player(), next_instance_id);
			const int64_t unit_index = sim::match_roster::register_built_unit(
					w, core._match_roster_state(), std::move(built), sn_player(), next_instance_id);
			if (unit_index < 0) {
				continue;
			}
			core._player_comp.append(core._unit_cold[static_cast<size_t>(unit_index)].archetype_id);
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
			sim::SimWorld w = core._sim_world();
			std::pair<UnitState, UnitStateCold> built =
					sim::unit_builder::build_unit(core._unit_builder_host(), spawn_spec, sn_enemy(), next_instance_id);
			const int64_t unit_index = sim::match_roster::register_built_unit(
					w, core._match_roster_state(), std::move(built), sn_enemy(), next_instance_id);
			if (unit_index < 0) {
				continue;
			}
			core._enemy_comp.append(core._unit_cold[static_cast<size_t>(unit_index)].archetype_id);
			next_instance_id += 1;
		}
		core._build_role_strategy_cache();
		core._prepare_tick_context();
		if (bench_phases) {
			ns_match_setup_total += sim_profile_elapsed_ns(t_match0);
		}
		auto t_sim0 = std::chrono::steady_clock::now();
		sim::match::simulate(core._match_loop_state(), core._match_loop_host());
		if (bench_phases) {
			ns_simulate_total += sim_profile_elapsed_ns(t_sim0);
		}
		core._reset_runtime_state();
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

Dictionary run_generated_matches_stats_partial(TeamfightSimulationCore &core, int64_t base_seed, int64_t batch_count, int64_t team_size, bool include_match_log, double tick_rate) {
	core._ensure_catalog_loaded();
	Dictionary result;
	if (batch_count <= 0) {
		result["stats_partial"] = Dictionary();
		result["matchup_data"] = Dictionary();
		return result;
	}

	Array champion_keys = core._catalog.effective_champion_by_archetype.keys();
	const int64_t champion_count = champion_keys.size();
	if (champion_count <= 0) {
		UtilityFunctions::push_error("TeamfightSimulationCore.run_generated_matches_stats_partial() requires champion catalog.");
		result["stats_partial"] = Dictionary();
		result["matchup_data"] = Dictionary();
		return result;
	}

	const int64_t units_per_team = Math::max(int64_t(1), team_size);
	std::vector<StringName> archetypes;
	archetypes.reserve(static_cast<size_t>(champion_count));
	for (int64_t index = 0; index < champion_count; ++index) {
		archetypes.push_back(StringName(String(champion_keys[index])));
	}

	auto make_stat_entry = []() -> Dictionary {
		Dictionary entry;
		entry["w"] = int64_t(0);
		entry["l"] = int64_t(0);
		entry["d"] = int64_t(0);
		entry["dmg_d"] = 0.0;
		entry["dmg_r"] = 0.0;
		entry["dmg_m"] = 0.0;
		entry["heal"] = 0.0;
		entry["heal_auto"] = 0.0;
		entry["heal_ability"] = 0.0;
		entry["heal_ultimate"] = 0.0;
		entry["heal_passive"] = 0.0;
		entry["shield"] = 0.0;
		entry["shield_auto"] = 0.0;
		entry["shield_ability"] = 0.0;
		entry["shield_ultimate"] = 0.0;
		entry["shield_passive"] = 0.0;
		entry["stuns"] = int64_t(0);
		entry["kills"] = int64_t(0);
		entry["deaths"] = int64_t(0);
		entry["assists"] = int64_t(0);
		entry["d_auto"] = 0.0;
		entry["d_ab"] = 0.0;
		entry["d_ult"] = 0.0;
		entry["d_passive"] = 0.0;
		entry["minion_dmg_d"] = 0.0;
		entry["minion_dmg_r"] = 0.0;
		return entry;
	};
	auto make_role_entry = [&make_stat_entry]() -> Dictionary {
		Dictionary entry = make_stat_entry();
		entry.erase("kills");
		entry.erase("deaths");
		entry.erase("assists");
		return entry;
	};
	auto accumulate_common = [](Dictionary &entry, const UnitStateCold &c) {
		entry["dmg_d"] = double(entry["dmg_d"]) + c.damage_dealt;
		entry["dmg_r"] = double(entry["dmg_r"]) + c.damage_received;
		entry["dmg_m"] = double(entry["dmg_m"]) + c.damage_mitigated;
		entry["heal"] = double(entry["heal"]) + c.healing_done;
		entry["heal_auto"] = double(entry["heal_auto"]) + c.healing_done_auto;
		entry["heal_ability"] = double(entry["heal_ability"]) + c.healing_done_ability;
		entry["heal_ultimate"] = double(entry["heal_ultimate"]) + c.healing_done_ultimate;
		entry["heal_passive"] = double(entry["heal_passive"]) + c.healing_done_passive;
		entry["shield"] = double(entry["shield"]) + c.shielding_done;
		entry["shield_auto"] = double(entry["shield_auto"]) + c.shielding_done_auto;
		entry["shield_ability"] = double(entry["shield_ability"]) + c.shielding_done_ability;
		entry["shield_ultimate"] = double(entry["shield_ultimate"]) + c.shielding_done_ultimate;
		entry["shield_passive"] = double(entry["shield_passive"]) + c.shielding_done_passive;
		entry["stuns"] = int64_t(entry["stuns"]) + c.stuns;
		entry["d_auto"] = double(entry["d_auto"]) + c.damage_dealt_auto;
		entry["d_ab"] = double(entry["d_ab"]) + c.damage_dealt_ability;
		entry["d_ult"] = double(entry["d_ult"]) + c.damage_dealt_ultimate;
		entry["d_passive"] = double(entry["d_passive"]) + c.damage_dealt_passive;
	};
	auto add_record = [&accumulate_common](Dictionary &entry, const UnitStateCold &c, bool won, bool draw, bool include_kda) {
		if (draw) {
			entry["d"] = int64_t(entry["d"]) + 1;
		} else if (won) {
			entry["w"] = int64_t(entry["w"]) + 1;
		} else {
			entry["l"] = int64_t(entry["l"]) + 1;
		}
		accumulate_common(entry, c);
		if (include_kda) {
			entry["kills"] = int64_t(entry["kills"]) + c.kills;
			entry["deaths"] = int64_t(entry["deaths"]) + c.deaths;
			entry["assists"] = int64_t(entry["assists"]) + c.assists;
		}
	};
	auto sorted_combo_label = [](const Array &comp) -> String {
		std::vector<String> names;
		names.reserve(static_cast<size_t>(comp.size()));
		for (int64_t i = 0; i < comp.size(); ++i) {
			names.push_back(String(comp[i]));
		}
		std::sort(names.begin(), names.end());
		String label;
		for (size_t i = 0; i < names.size(); ++i) {
			if (i > 0) {
				label += " + ";
			}
			label += names[i];
		}
		return label;
	};
	auto record_matchup = [](Dictionary &matchup_data, const StringName &champion_id, const String &key, bool won) {
		const String champion = String(champion_id);
		Dictionary champion_data = Dictionary(matchup_data.get(champion, Dictionary()));
		Dictionary data = Dictionary(champion_data.get(key, Dictionary()));
		if (data.is_empty()) {
			data["wins"] = int64_t(0);
			data["losses"] = int64_t(0);
			data["winrate"] = 0.0;
		}
		if (won) {
			data["wins"] = int64_t(data["wins"]) + 1;
		} else {
			data["losses"] = int64_t(data["losses"]) + 1;
		}
		const int64_t wins = int64_t(data["wins"]);
		const int64_t losses = int64_t(data["losses"]);
		const int64_t total = wins + losses;
		data["winrate"] = total > 0 ? double(wins) / double(total) : 0.0;
		champion_data[key] = data;
		matchup_data[champion] = champion_data;
	};
	auto record_matchup_result = [&record_matchup](Dictionary &matchup_data, const Array &winners, const Array &losers) {
		for (int64_t wi = 0; wi < winners.size(); ++wi) {
			StringName winner = StringName(winners[wi]);
			for (int64_t li = 0; li < losers.size(); ++li) {
				StringName loser = StringName(losers[li]);
				record_matchup(matchup_data, winner, "vs_" + String(loser), true);
			}
			for (int64_t ai = 0; ai < winners.size(); ++ai) {
				StringName ally = StringName(winners[ai]);
				if (winner != ally) {
					record_matchup(matchup_data, winner, "with_" + String(ally), true);
				}
			}
		}
		for (int64_t li = 0; li < losers.size(); ++li) {
			StringName loser = StringName(losers[li]);
			for (int64_t wi = 0; wi < winners.size(); ++wi) {
				StringName winner = StringName(winners[wi]);
				record_matchup(matchup_data, loser, "vs_" + String(winner), false);
			}
			for (int64_t ai = 0; ai < losers.size(); ++ai) {
				StringName ally = StringName(losers[ai]);
				if (loser != ally) {
					record_matchup(matchup_data, loser, "with_" + String(ally), false);
				}
			}
		}
	};
	auto append_generated_unit = [&core](Dictionary &spawn_spec, const StringName &team, const StringName &archetype, int64_t &next_instance_id, Array &comp) {
		spawn_spec.clear();
		spawn_spec["archetype_id"] = archetype;
		sim::SimWorld w = core._sim_world();
		std::pair<UnitState, UnitStateCold> built = sim::unit_builder::build_unit(core._unit_builder_host(), spawn_spec, team, next_instance_id);
		const int64_t unit_index = sim::match_roster::register_built_unit(w, core._match_roster_state(), std::move(built), team, next_instance_id);
		if (unit_index < 0) {
			return;
		}
		comp.append(core._unit_cold[static_cast<size_t>(unit_index)].archetype_id);
		next_instance_id += 1;
	};
	auto pick_index = [](const Ref<RandomNumberGenerator> &rng, int64_t upper_inclusive) -> int64_t {
		return int64_t(rng->randi_range(0, int32_t(upper_inclusive)));
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
		core._reset_runtime_state();
		core._seed = seed;
		core._tick_rate = tick_rate;
		core._record_events = false;
		core._debug_combat_trace = false;
		core._trace_buffer.clear();
		core._rng.seed_int64(core._seed);

		Ref<RandomNumberGenerator> draft_rng;
		draft_rng.instantiate();
		draft_rng->set_seed(uint64_t(seed));
		int64_t next_instance_id = 1;
		if (champion_count < units_per_team * 2) {
			for (int64_t slot = 0; slot < units_per_team; ++slot) {
				const int64_t archetype_index = pick_index(draft_rng, champion_count - 1);
				append_generated_unit(spawn_spec, sn_player(), archetypes[static_cast<size_t>(archetype_index)], next_instance_id, core._player_comp);
			}
			for (int64_t slot = 0; slot < units_per_team; ++slot) {
				const int64_t archetype_index = pick_index(draft_rng, champion_count - 1);
				append_generated_unit(spawn_spec, sn_enemy(), archetypes[static_cast<size_t>(archetype_index)], next_instance_id, core._enemy_comp);
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
				append_generated_unit(spawn_spec, sn_player(), archetypes[static_cast<size_t>(indices[static_cast<size_t>(slot)])], next_instance_id, core._player_comp);
			}
			for (int64_t slot = 0; slot < units_per_team; ++slot) {
				const int64_t source_index = slot + units_per_team;
				append_generated_unit(spawn_spec, sn_enemy(), archetypes[static_cast<size_t>(indices[static_cast<size_t>(source_index)])], next_instance_id, core._enemy_comp);
			}
		}
		core._build_role_strategy_cache();
		core._prepare_tick_context();
		sim::match::simulate(core._match_loop_state(), core._match_loop_host());

		// Aggregate minion stats to summoners
		std::unordered_map<int64_t, double> summoner_minion_damage_dealt;
		std::unordered_map<int64_t, double> summoner_minion_damage_received;
		for (const UnitState &unit : core._units) {
			if (unit.summoner_instance_id != 0) {
				const UnitStateCold &c = core._uc(unit);
				summoner_minion_damage_dealt[unit.summoner_instance_id] += c.damage_dealt;
				summoner_minion_damage_received[unit.summoner_instance_id] += c.damage_received;
			}
		}

		// Define lambdas that capture the summoner maps (must be after maps are created)
		auto accumulate_common_with_minions = [&summoner_minion_damage_dealt, &summoner_minion_damage_received](Dictionary &entry, const UnitStateCold &c, int64_t instance_id) {
			entry["dmg_d"] = double(entry["dmg_d"]) + c.damage_dealt;
			entry["dmg_r"] = double(entry["dmg_r"]) + c.damage_received;
			entry["dmg_m"] = double(entry["dmg_m"]) + c.damage_mitigated;
			entry["heal"] = double(entry["heal"]) + c.healing_done;
			entry["heal_auto"] = double(entry["heal_auto"]) + c.healing_done_auto;
			entry["heal_ability"] = double(entry["heal_ability"]) + c.healing_done_ability;
			entry["heal_ultimate"] = double(entry["heal_ultimate"]) + c.healing_done_ultimate;
			entry["heal_passive"] = double(entry["heal_passive"]) + c.healing_done_passive;
			entry["shield"] = double(entry["shield"]) + c.shielding_done;
			entry["shield_auto"] = double(entry["shield_auto"]) + c.shielding_done_auto;
			entry["shield_ability"] = double(entry["shield_ability"]) + c.shielding_done_ability;
			entry["shield_ultimate"] = double(entry["shield_ultimate"]) + c.shielding_done_ultimate;
			entry["shield_passive"] = double(entry["shield_passive"]) + c.shielding_done_passive;
			entry["stuns"] = int64_t(entry["stuns"]) + c.stuns;
			entry["d_auto"] = double(entry["d_auto"]) + c.damage_dealt_auto;
			entry["d_ab"] = double(entry["d_ab"]) + c.damage_dealt_ability;
			entry["d_ult"] = double(entry["d_ult"]) + c.damage_dealt_ultimate;
			entry["d_passive"] = double(entry["d_passive"]) + c.damage_dealt_passive;
			// Add minion damage for this summoner
			auto it_dealt = summoner_minion_damage_dealt.find(instance_id);
			auto it_received = summoner_minion_damage_received.find(instance_id);
			entry["minion_dmg_d"] = double(entry["minion_dmg_d"]) + (it_dealt != summoner_minion_damage_dealt.end() ? it_dealt->second : 0.0);
			entry["minion_dmg_r"] = double(entry["minion_dmg_r"]) + (it_received != summoner_minion_damage_received.end() ? it_received->second : 0.0);
		};
		auto add_record_with_minions = [&accumulate_common_with_minions](Dictionary &entry, const UnitStateCold &c, int64_t instance_id, bool won, bool draw, bool include_kda) {
			if (draw) {
				entry["d"] = int64_t(entry["d"]) + 1;
			} else if (won) {
				entry["w"] = int64_t(entry["w"]) + 1;
			} else {
				entry["l"] = int64_t(entry["l"]) + 1;
			}
			accumulate_common_with_minions(entry, c, instance_id);
			if (include_kda) {
				entry["kills"] = int64_t(entry["kills"]) + c.kills;
				entry["deaths"] = int64_t(entry["deaths"]) + c.deaths;
				entry["assists"] = int64_t(entry["assists"]) + c.assists;
			}
		};

		const bool player_won = core._winner_team == sn_player();
		const bool enemy_won = core._winner_team == sn_enemy();
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
			row["seed"] = core._seed;
			row["winner"] = String(core._winner_team);
			row["sudden_death_ticks"] = int64_t(core._sudden_death_ticks);
			row["duration"] = core._time;
			match_logs.append(row);
		}

		Dictionary heroes = Dictionary(bucket["heroes"]);
		Dictionary roles = Dictionary(bucket["roles"]);
		for (const UnitState &unit : core._units) {
			const UnitStateCold &c = core._uc(unit);
			// Skip minions (spawned during combat, not drafted champions)
			if (c.role_id == StringName("minion")) {
				continue;
			}
			const String hero = String(c.archetype_id);
			Dictionary hero_entry = Dictionary(heroes.get(hero, Dictionary()));
			if (hero_entry.is_empty()) {
				hero_entry = make_stat_entry();
			}
			const bool unit_won = core._winner_team != StringName() && unit.team == core._winner_team;
			add_record_with_minions(hero_entry, c, unit.instance_id, unit_won, draw, true);
			heroes[hero] = hero_entry;

			const String role = String(Dictionary(c.stats).get("role", String("unknown")));
			Dictionary role_entry = Dictionary(roles.get(role, Dictionary()));
			if (role_entry.is_empty()) {
				role_entry = make_role_entry();
			}
			add_record_with_minions(role_entry, c, unit.instance_id, unit_won, draw, false);
			roles[role] = role_entry;
		}
		bucket["heroes"] = heroes;
		bucket["roles"] = roles;

		if (team_size > 1) {
			Dictionary combos = Dictionary(bucket["combos"]);
			const String player_label = sorted_combo_label(core._player_comp);
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

			const String enemy_label = sorted_combo_label(core._enemy_comp);
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
			record_matchup_result(matchup_data, core._player_comp, core._enemy_comp);
		} else if (enemy_won) {
			record_matchup_result(matchup_data, core._enemy_comp, core._player_comp);
		}

		core._reset_runtime_state();
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
