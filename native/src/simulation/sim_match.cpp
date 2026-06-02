#include "sim_match.hpp"

#include <godot_cpp/variant/string.hpp>

#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace sim {
namespace match {

namespace {

const UnitStateCold &unit_cold_at(
		const std::vector<UnitState> &units,
		const std::vector<UnitStateCold> &unit_cold,
		const UnitState &unit) {
	const size_t index = static_cast<size_t>(&unit - units.data());
	return unit_cold[index];
}

// When TEAMFIGHT_STATS_EXPORT_MINIMAL=1, skip per-unit telemetry dicts in build_stats_summary (CSV export path).
bool stats_export_minimal_telemetry_enabled() {
	const char *v = std::getenv("TEAMFIGHT_STATS_EXPORT_MINIMAL");
	return v != nullptr && std::strcmp(v, "1") == 0;
}

void aggregate_summoner_minion_stats(
		const std::vector<UnitState> &units,
		const std::vector<UnitStateCold> &unit_cold,
		std::unordered_map<int64_t, double> &summoner_minion_damage_dealt,
		std::unordered_map<int64_t, double> &summoner_minion_damage_received) {
	for (const UnitState &unit : units) {
		if (unit.summoner_instance_id != 0) {
			const UnitStateCold &c = unit_cold_at(units, unit_cold, unit);
			summoner_minion_damage_dealt[unit.summoner_instance_id] += c.damage_dealt;
			summoner_minion_damage_received[unit.summoner_instance_id] += c.damage_received;
		}
	}
}

void fill_common_unit_summary_fields(
		Dictionary &unit_summary,
		const UnitState &unit,
		const UnitStateCold &c,
		const StringName &winner_team,
		const std::unordered_map<int64_t, double> &summoner_minion_damage_dealt,
		const std::unordered_map<int64_t, double> &summoner_minion_damage_received) {
	unit_summary["won"] = winner_team != StringName() && unit.team == winner_team;
	unit_summary["damage_dealt"] = c.damage_dealt;
	unit_summary["damage_dealt_auto"] = c.damage_dealt_auto;
	unit_summary["damage_dealt_ability"] = c.damage_dealt_ability;
	unit_summary["damage_dealt_ultimate"] = c.damage_dealt_ultimate;
	unit_summary["damage_dealt_passive"] = c.damage_dealt_passive;
	unit_summary["damage_received"] = c.damage_received;
	unit_summary["damage_mitigated"] = c.damage_mitigated;
	unit_summary["healing_done"] = c.healing_done;
	unit_summary["healing_done_auto"] = c.healing_done_auto;
	unit_summary["healing_done_ability"] = c.healing_done_ability;
	unit_summary["healing_done_ultimate"] = c.healing_done_ultimate;
	unit_summary["healing_done_passive"] = c.healing_done_passive;
	unit_summary["shielding_done"] = c.shielding_done;
	unit_summary["shielding_done_auto"] = c.shielding_done_auto;
	unit_summary["shielding_done_ability"] = c.shielding_done_ability;
	unit_summary["shielding_done_ultimate"] = c.shielding_done_ultimate;
	unit_summary["shielding_done_passive"] = c.shielding_done_passive;
	unit_summary["auto_attacks"] = c.auto_attacks;
	unit_summary["abilities"] = c.abilities;
	unit_summary["ultimates"] = c.ultimates;
	unit_summary["stuns"] = c.stuns;
	unit_summary["kills"] = c.kills;
	unit_summary["deaths"] = c.deaths;
	unit_summary["assists"] = c.assists;

	auto it_dealt = summoner_minion_damage_dealt.find(unit.instance_id);
	auto it_received = summoner_minion_damage_received.find(unit.instance_id);
	if (it_dealt != summoner_minion_damage_dealt.end() || it_received != summoner_minion_damage_received.end()) {
		unit_summary["minion_damage_dealt"] = it_dealt != summoner_minion_damage_dealt.end() ? it_dealt->second : 0.0;
		unit_summary["minion_damage_received"] = it_received != summoner_minion_damage_received.end() ? it_received->second : 0.0;
	} else {
		unit_summary["minion_damage_dealt"] = 0.0;
		unit_summary["minion_damage_received"] = 0.0;
	}
}

} // namespace

Dictionary build_summary(
		const MatchSnapshot &match,
		const std::vector<UnitState> &units,
		const std::vector<UnitStateCold> &unit_cold,
		Dictionary &summary_cache,
		Array &summary_unit_stats) {
	summary_cache.clear();
	summary_cache["seed"] = match.seed;
	summary_cache["winner_team"] = String(match.winner_team);
	summary_cache["duration"] = match.time;
	summary_cache["sudden_death_ticks"] = int64_t(match.sudden_death_ticks);
	summary_cache["player_kills"] = int64_t(match.player_kills);
	summary_cache["enemy_kills"] = int64_t(match.enemy_kills);
	summary_cache["player_comp"] = match.player_comp;
	summary_cache["enemy_comp"] = match.enemy_comp;
	summary_unit_stats.clear();

	std::unordered_map<int64_t, double> summoner_minion_damage_dealt;
	std::unordered_map<int64_t, double> summoner_minion_damage_received;
	aggregate_summoner_minion_stats(units, unit_cold, summoner_minion_damage_dealt, summoner_minion_damage_received);

	for (const UnitState &unit : units) {
		// Skip minions in unit_stats output - their damage is aggregated to summoners
		if (unit.summoner_instance_id != 0) {
			continue;
		}

		const UnitStateCold &c = unit_cold_at(units, unit_cold, unit);
		Dictionary unit_summary;
		unit_summary["instance_id"] = unit.instance_id;
		unit_summary["archetype"] = String(c.unit_id);
		unit_summary["role"] = String(c.role_id);
		unit_summary["team"] = String(unit.team);
		fill_common_unit_summary_fields(
				unit_summary,
				unit,
				c,
				match.winner_team,
				summoner_minion_damage_dealt,
				summoner_minion_damage_received);

		Dictionary telemetry;
		telemetry["schema"] = String("teamfight.telemetry.v1");
		telemetry["hard_cc_seconds"] = unit.hard_cc_seconds;
		unit_summary["telemetry"] = telemetry;
		summary_unit_stats.append(unit_summary);
	}
	summary_cache["unit_stats"] = summary_unit_stats;
	return summary_cache;
}

Dictionary build_stats_summary(
		const MatchSnapshot &match,
		const std::vector<UnitState> &units,
		const std::vector<UnitStateCold> &unit_cold) {
	const bool omit_unit_telemetry = stats_export_minimal_telemetry_enabled();
	Dictionary summary;
	summary["seed"] = match.seed;
	summary["winner_team"] = String(match.winner_team);
	summary["duration"] = match.time;
	summary["sudden_death_ticks"] = int64_t(match.sudden_death_ticks);
	summary["player_comp"] = match.player_comp;
	summary["enemy_comp"] = match.enemy_comp;

	std::unordered_map<int64_t, double> summoner_minion_damage_dealt;
	std::unordered_map<int64_t, double> summoner_minion_damage_received;
	aggregate_summoner_minion_stats(units, unit_cold, summoner_minion_damage_dealt, summoner_minion_damage_received);

	Array unit_stats;
	for (const UnitState &unit : units) {
		// Skip minions in unit_stats output - their damage is aggregated to summoners
		if (unit.summoner_instance_id != 0) {
			continue;
		}

		const UnitStateCold &c = unit_cold_at(units, unit_cold, unit);
		Dictionary unit_summary;
		unit_summary["unit_id"] = String(c.unit_id);
		unit_summary["role"] = String(c.role_id);
		fill_common_unit_summary_fields(
				unit_summary,
				unit,
				c,
				match.winner_team,
				summoner_minion_damage_dealt,
				summoner_minion_damage_received);

		if (!omit_unit_telemetry) {
			Dictionary telemetry;
			telemetry["schema"] = String("teamfight.telemetry.v1");
			telemetry["hard_cc_seconds"] = unit.hard_cc_seconds;
			unit_summary["telemetry"] = telemetry;
		}
		unit_stats.append(unit_summary);
	}
	summary["unit_stats"] = unit_stats;
	return summary;
}

} // namespace match
} // namespace sim
