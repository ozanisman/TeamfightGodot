#include "sim_draft_ai_config.hpp"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <climits>

namespace sim {
namespace draft_ai {

namespace {

bool value_is_numeric(const Variant &v) {
	const Variant::Type t = v.get_type();
	return t == Variant::FLOAT || t == Variant::INT;
}

double dget(const Dictionary &d, const String &key, double fallback, const String &override_path) {
	if (d.has(key)) {
		const Variant v = d[key];
		if (!value_is_numeric(v)) {
			UtilityFunctions::push_warning(vformat("sim_draft_ai_config: key '%s' is not numeric in %s, using default %f", key, override_path, fallback));
			return fallback;
		}
		const double result = double(v);
		if (!UtilityFunctions::is_finite(result)) {
			UtilityFunctions::push_warning(vformat("sim_draft_ai_config: key '%s' is not finite in %s, using default %f", key, override_path, fallback));
			return fallback;
		}
		return result;
	}
	return fallback;
}

int iget(const Dictionary &d, const String &key, int fallback, const String &override_path) {
	if (d.has(key)) {
		const Variant v = d[key];
		if (!value_is_numeric(v)) {
			UtilityFunctions::push_warning(vformat("sim_draft_ai_config: key '%s' is not numeric in %s, using default %d", key, override_path, fallback));
			return fallback;
		}
		if (v.get_type() == Variant::FLOAT) {
			const double result = double(v);
			if (!UtilityFunctions::is_finite(result) || result < INT_MIN || result > INT_MAX) {
				UtilityFunctions::push_warning(vformat("sim_draft_ai_config: key '%s' is out of int range or not finite in %s, using default %d", key, override_path, fallback));
				return fallback;
			}
		}
		return int(v);
	}
	return fallback;
}

void apply_pick_weights(const Dictionary &d, PickPhaseWeights &w, const String &override_path) {
	w.base_power = dget(d, "base_power", w.base_power, override_path);
	w.ally_synergy = dget(d, "ally_synergy", w.ally_synergy, override_path);
	w.enemy_counter_value = dget(d, "enemy_counter_value", w.enemy_counter_value, override_path);
	w.counter_risk = dget(d, "counter_risk", w.counter_risk, override_path);
	w.role_fit = dget(d, "role_fit", w.role_fit, override_path);
	w.comp_fit = dget(d, "comp_fit", w.comp_fit, override_path);
}

void apply_ban_weights(const Dictionary &d, BanPhaseWeights &w, const String &override_path) {
	w.denial_value = dget(d, "denial_value", w.denial_value, override_path);
	w.enemy_synergy = dget(d, "enemy_synergy", w.enemy_synergy, override_path);
	w.counters_my_team = dget(d, "counters_my_team", w.counters_my_team, override_path);
	w.fills_enemy_role_need = dget(d, "fills_enemy_role_need", w.fills_enemy_role_need, override_path);
	w.enemy_comp_fit = dget(d, "enemy_comp_fit", w.enemy_comp_fit, override_path);
	w.early_fallback_multiplier = dget(d, "early_fallback_multiplier", w.early_fallback_multiplier, override_path);
}

void apply_role_fit_steps(const Dictionary &d, RoleFitSteps &s, const String &override_path) {
	s.not_present = dget(d, "not_present", s.not_present, override_path);
	s.once = dget(d, "once", s.once, override_path);
	s.twice = dget(d, "twice", s.twice, override_path);
	s.three_plus = dget(d, "three_plus", s.three_plus, override_path);
}

void apply_fills_role_steps(const Dictionary &d, FillsRoleSteps &s, const String &override_path) {
	s.not_present = dget(d, "not_present", s.not_present, override_path);
	s.once = dget(d, "once", s.once, override_path);
	s.two_plus = dget(d, "two_plus", s.two_plus, override_path);
}

void apply_evaluator_weights(const Dictionary &d, EvaluatorWeights &w, const String &override_path) {
	w.base_power_weight = dget(d, "base_power_weight", w.base_power_weight, override_path);
	w.ally_synergy_weight = dget(d, "ally_synergy_weight", w.ally_synergy_weight, override_path);
	w.enemy_counter_value_weight = dget(d, "enemy_counter_value_weight", w.enemy_counter_value_weight, override_path);
	w.counter_risk_weight = dget(d, "counter_risk_weight", w.counter_risk_weight, override_path);
	w.enemy_pick_value_weight = dget(d, "enemy_pick_value_weight", w.enemy_pick_value_weight, override_path);
	w.enemy_synergy_weight = dget(d, "enemy_synergy_weight", w.enemy_synergy_weight, override_path);
	w.counters_my_team_weight = dget(d, "counters_my_team_weight", w.counters_my_team_weight, override_path);
	w.own_pick_value_penalty_weight = dget(d, "own_pick_value_penalty_weight", w.own_pick_value_penalty_weight, override_path);
}

} // namespace

Config Config::load_with_optional_override(const String &override_path) {
	Config config;
	if (override_path.is_empty()) {
		return config;
	}

	Ref<FileAccess> file = FileAccess::open(override_path, FileAccess::ModeFlags::READ);
	if (file.is_null()) {
		// Override file is optional; silently keep defaults.
		return config;
	}

	Variant parsed = JSON::parse_string(file->get_as_text());
	if (parsed.get_type() != Variant::DICTIONARY) {
		UtilityFunctions::push_warning(vformat("sim_draft_ai_config: failed to parse override JSON at %s, using defaults", override_path));
		return config;
	}

	Dictionary root = parsed;

	if (root.has("pick")) {
		Dictionary pick = root["pick"];
		if (pick.has("default")) {
			apply_pick_weights(pick["default"], config.pick_default, override_path);
		}
		if (pick.has("mid")) {
			apply_pick_weights(pick["mid"], config.pick_default, override_path);
		}
		if (pick.has("early")) {
			apply_pick_weights(pick["early"], config.pick_early, override_path);
		}
		if (pick.has("late")) {
			apply_pick_weights(pick["late"], config.pick_late, override_path);
		}
		if (pick.has("role_fit_steps")) {
			apply_role_fit_steps(pick["role_fit_steps"], config.role_fit_steps, override_path);
		}
		if (pick.has("ranges")) {
			Dictionary ranges = pick["ranges"];
			config.pick_ranges.early_start = iget(ranges, "early_start", config.pick_ranges.early_start, override_path);
			config.pick_ranges.early_end = iget(ranges, "early_end", config.pick_ranges.early_end, override_path);
			config.pick_ranges.mid_start = iget(ranges, "mid_start", config.pick_ranges.mid_start, override_path);
			config.pick_ranges.mid_end = iget(ranges, "mid_end", config.pick_ranges.mid_end, override_path);
			config.pick_ranges.late_start = iget(ranges, "late_start", config.pick_ranges.late_start, override_path);
			config.pick_ranges.late_end = iget(ranges, "late_end", config.pick_ranges.late_end, override_path);
		}
	}

	if (root.has("ban")) {
		Dictionary ban = root["ban"];
		if (ban.has("default")) {
			apply_ban_weights(ban["default"], config.ban_default, override_path);
		}
		if (ban.has("phase1")) {
			apply_ban_weights(ban["phase1"], config.ban_phase1, override_path);
		}
		if (ban.has("phase2")) {
			apply_ban_weights(ban["phase2"], config.ban_phase2, override_path);
		}
		if (ban.has("fills_role_steps")) {
			apply_fills_role_steps(ban["fills_role_steps"], config.fills_role_steps, override_path);
		}
		if (ban.has("ranges")) {
			Dictionary ranges = ban["ranges"];
			config.ban_ranges.phase1_start = iget(ranges, "phase1_start", config.ban_ranges.phase1_start, override_path);
			config.ban_ranges.phase1_end = iget(ranges, "phase1_end", config.ban_ranges.phase1_end, override_path);
			config.ban_ranges.phase2_start = iget(ranges, "phase2_start", config.ban_ranges.phase2_start, override_path);
			config.ban_ranges.phase2_end = iget(ranges, "phase2_end", config.ban_ranges.phase2_end, override_path);
		}
	}

	if (root.has("evaluator_weights")) {
		apply_evaluator_weights(root["evaluator_weights"], config.evaluator_weights, override_path);
	}

	if (root.has("lookahead")) {
		Dictionary lookahead = root["lookahead"];
		config.lookahead.pick_response_weight = dget(lookahead, "response_weight", config.lookahead.pick_response_weight, override_path);
		config.lookahead.ban_denied_enemy_pick_weight = dget(lookahead, "denied_enemy_pick_weight", config.lookahead.ban_denied_enemy_pick_weight, override_path);
		config.lookahead.lookahead_candidates = iget(lookahead, "candidates", config.lookahead.lookahead_candidates, override_path);
		config.lookahead.denial_negligible_epsilon = dget(lookahead, "denial_negligible_epsilon", config.lookahead.denial_negligible_epsilon, override_path);
	}

	if (root.has("certification")) {
		Dictionary cert = root["certification"];
		if (cert.has("stats_snapshot_id")) {
			config.certification.stats_snapshot_id = String(cert["stats_snapshot_id"]);
		}
	}

	return config;
}

} // namespace draft_ai
} // namespace sim
