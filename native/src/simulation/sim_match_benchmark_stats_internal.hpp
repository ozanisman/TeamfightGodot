#ifndef SIM_MATCH_BENCHMARK_STATS_INTERNAL_HPP
#define SIM_MATCH_BENCHMARK_STATS_INTERNAL_HPP

#include "simulation_types.hpp"

#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <unordered_map>
#include <vector>

namespace sim {
namespace match {
namespace benchmark {
namespace stats_internal {

godot::Dictionary make_stat_entry();
godot::Dictionary make_role_entry();
void accumulate_common(godot::Dictionary &entry, const UnitStateCold &c);
void add_record(godot::Dictionary &entry, const UnitStateCold &c, bool won, bool draw, bool include_kda);
void accumulate_common_with_minions(
		godot::Dictionary &entry,
		const UnitStateCold &c,
		int64_t instance_id,
		const std::unordered_map<int64_t, double> &summoner_minion_damage_dealt,
		const std::unordered_map<int64_t, double> &summoner_minion_damage_received,
		const std::unordered_map<int64_t, double> &summoner_minion_damage_mitigated);
void add_record_with_minions(
		godot::Dictionary &entry,
		const UnitStateCold &c,
		int64_t instance_id,
		bool won,
		bool draw,
		bool include_kda,
		const std::unordered_map<int64_t, double> &summoner_minion_damage_dealt,
		const std::unordered_map<int64_t, double> &summoner_minion_damage_received,
		const std::unordered_map<int64_t, double> &summoner_minion_damage_mitigated);
godot::String sorted_combo_label(const godot::Array &comp);
void record_matchup(godot::Dictionary &matchup_data, const godot::StringName &champion_id, const godot::String &key, bool won);
void record_matchup_result(godot::Dictionary &matchup_data, const godot::Array &winners, const godot::Array &losers);
int64_t pick_index(const godot::Ref<godot::RandomNumberGenerator> &rng, int64_t upper_inclusive);
std::vector<godot::StringName> load_archetypes(const godot::Array &champion_keys);

} // namespace stats_internal
} // namespace benchmark
} // namespace match
} // namespace sim

#endif
