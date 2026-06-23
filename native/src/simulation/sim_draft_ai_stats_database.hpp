#ifndef SIM_DRAFT_AI_STATS_DATABASE_HPP
#define SIM_DRAFT_AI_STATS_DATABASE_HPP

#include "sim_draft_ai_types.hpp"
#include "sim_catalog.hpp"

#include <godot_cpp/variant/string.hpp>
#include <map>

using namespace godot;

namespace sim {
namespace draft_ai {

class DraftStatsDatabase {
public:
	DraftStatsDatabase() = default;
	~DraftStatsDatabase() = default;

	bool load_from_dir(const String &dir_path);
	bool is_loaded() const;
	String last_error() const;

	DraftStatValue base_winrate_for(const StringName &champion) const;
	DraftStatValue synergy_winrate_for(const StringName &champion, const StringName &ally) const;
	DraftStatValue counter_winrate_for(const StringName &champion, const StringName &enemy) const;
	StringName role_for(const StringName &champion) const;
	DraftStatValue role_combination_value_for(const std::vector<StringName> &roles) const;
	DraftStatValue partial_role_combination_value_for(const std::vector<StringName> &roles, int &out_match_count) const;
	static String build_role_fingerprint(const std::vector<StringName> &roles);

	void set_catalog(const catalog::CatalogState *catalog);

private:
	struct StatRecord {
		int wins = 0;
		int losses = 0;
		int samples = 0;
		double raw_winrate = 0.5;
	};

	using MatchupMap = std::map<String, std::map<String, StatRecord>>;

	std::map<StringName, StatRecord> _base_stats;
	MatchupMap _synergy_stats;
	MatchupMap _counter_stats;
	std::map<StringName, StringName> _champion_roles;
	std::map<String, StatRecord> _role_combination_stats;
	bool _loaded = false;
	String _last_error;
	const catalog::CatalogState *_catalog = nullptr;

	bool _load_combat_stats(const String &path);
	bool _load_matchup_stats(const String &path, MatchupMap &target);
	bool _load_role_combinations(const String &path);
	DraftStatValue _apply_smoothing(const StatRecord &record, const DraftPredictionConfig &config) const;
	DraftStatValue _create_missing_value() const;

	static std::vector<String> _parse_csv_line(const String &line);
	static int64_t _column_index(const std::vector<String> &header, const String &name);
	static String _cell_or_empty(const std::vector<String> &row, int64_t index);
};

} // namespace draft_ai
} // namespace sim

#endif // SIM_DRAFT_AI_STATS_DATABASE_HPP
