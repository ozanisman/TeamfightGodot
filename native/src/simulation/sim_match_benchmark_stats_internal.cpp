#include "sim_match_benchmark_stats_internal.hpp"

#include <algorithm>
#include <unordered_map>

using namespace godot;

namespace sim {
namespace match {
namespace benchmark {
namespace stats_internal {

Dictionary make_stat_entry() {
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
	entry["minion_dmg_m"] = 0.0;
	return entry;
}

Dictionary make_role_entry() {
	Dictionary entry = make_stat_entry();
	entry.erase("kills");
	entry.erase("deaths");
	entry.erase("assists");
	return entry;
}

void accumulate_common(Dictionary &entry, const UnitStateCold &c) {
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
}

void add_record(Dictionary &entry, const UnitStateCold &c, bool won, bool draw, bool include_kda) {
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
}

void accumulate_common_with_minions(
		Dictionary &entry,
		const UnitStateCold &c,
		int64_t instance_id,
		const std::unordered_map<int64_t, double> &summoner_minion_damage_dealt,
		const std::unordered_map<int64_t, double> &summoner_minion_damage_received,
		const std::unordered_map<int64_t, double> &summoner_minion_damage_mitigated) {
	accumulate_common(entry, c);
	auto it_dealt = summoner_minion_damage_dealt.find(instance_id);
	auto it_received = summoner_minion_damage_received.find(instance_id);
	auto it_mitigated = summoner_minion_damage_mitigated.find(instance_id);
	entry["minion_dmg_d"] = double(entry["minion_dmg_d"]) + (it_dealt != summoner_minion_damage_dealt.end() ? it_dealt->second : 0.0);
	entry["minion_dmg_r"] = double(entry["minion_dmg_r"]) + (it_received != summoner_minion_damage_received.end() ? it_received->second : 0.0);
	entry["minion_dmg_m"] = double(entry["minion_dmg_m"]) + (it_mitigated != summoner_minion_damage_mitigated.end() ? it_mitigated->second : 0.0);
}

void add_record_with_minions(
		Dictionary &entry,
		const UnitStateCold &c,
		int64_t instance_id,
		bool won,
		bool draw,
		bool include_kda,
		const std::unordered_map<int64_t, double> &summoner_minion_damage_dealt,
		const std::unordered_map<int64_t, double> &summoner_minion_damage_received,
		const std::unordered_map<int64_t, double> &summoner_minion_damage_mitigated) {
	if (draw) {
		entry["d"] = int64_t(entry["d"]) + 1;
	} else if (won) {
		entry["w"] = int64_t(entry["w"]) + 1;
	} else {
		entry["l"] = int64_t(entry["l"]) + 1;
	}
	accumulate_common_with_minions(entry, c, instance_id, summoner_minion_damage_dealt, summoner_minion_damage_received, summoner_minion_damage_mitigated);
	if (include_kda) {
		entry["kills"] = int64_t(entry["kills"]) + c.kills;
		entry["deaths"] = int64_t(entry["deaths"]) + c.deaths;
		entry["assists"] = int64_t(entry["assists"]) + c.assists;
	}
}

String sorted_combo_label(const Array &comp) {
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
}

void record_matchup(Dictionary &matchup_data, const StringName &champion_id, const String &key, bool won) {
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
}

void record_matchup_result(Dictionary &matchup_data, const Array &winners, const Array &losers) {
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
}

int64_t pick_index(const Ref<RandomNumberGenerator> &rng, int64_t upper_inclusive) {
	return int64_t(rng->randi_range(0, int32_t(upper_inclusive)));
}

std::vector<StringName> load_archetypes(const Array &champion_keys) {
	const int64_t champion_count = champion_keys.size();
	std::vector<StringName> archetypes;
	archetypes.reserve(static_cast<size_t>(champion_count));
	for (int64_t index = 0; index < champion_count; ++index) {
		archetypes.push_back(StringName(String(champion_keys[index])));
	}
	return archetypes;
}

} // namespace stats_internal
} // namespace benchmark
} // namespace match
} // namespace sim
