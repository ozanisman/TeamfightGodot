#include "sim_draft_ai_recommender.hpp"

#include <algorithm>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <map>

namespace sim {
namespace draft_ai {

DraftRecommender::DraftRecommender(const DraftEvaluator *evaluator, const DraftStatsDatabase *database)
	: _evaluator(evaluator), _database(database) {
}

std::vector<DraftPickScoreBreakdown> DraftRecommender::recommend_picks(
	const std::vector<StringName> &available,
	const std::vector<StringName> &allies,
	const std::vector<StringName> &enemies,
	int max_results,
	int draft_step,
	DraftStrategy strategy
) const {
	std::vector<DraftPickScoreBreakdown> results;

	// Determine phase and weights
	double base_power_weight = 0.35;
	double ally_synergy_weight = 0.25;
	double enemy_counter_value_weight = 0.25;
	double counter_risk_weight = -0.15;
	double role_fit_weight = 0.10;
	double comp_fit_weight = 0.08;
	String phase_label = "default";

	if (draft_step >= 6 && draft_step <= 8) {
		// Early pick: steps 6-8
		base_power_weight = 0.45;
		ally_synergy_weight = 0.20;
		enemy_counter_value_weight = 0.15;
		counter_risk_weight = -0.20;
		role_fit_weight = 0.05;
		comp_fit_weight = 0.04;
		phase_label = "early_pick";
	} else if (draft_step >= 9 && draft_step <= 11) {
		// Mid pick: steps 9-11
		base_power_weight = 0.35;
		ally_synergy_weight = 0.25;
		enemy_counter_value_weight = 0.25;
		counter_risk_weight = -0.15;
		role_fit_weight = 0.10;
		comp_fit_weight = 0.08;
		phase_label = "mid_pick";
	} else if (draft_step >= 16 && draft_step <= 19) {
		// Late pick: steps 16-19
		base_power_weight = 0.25;
		ally_synergy_weight = 0.30;
		enemy_counter_value_weight = 0.35;
		counter_risk_weight = -0.10;
		role_fit_weight = 0.15;
		comp_fit_weight = 0.10;
		phase_label = "late_pick";
	}

	// Count allied roles
	std::map<StringName, int> allied_role_counts;
	for (const StringName &ally : allies) {
		StringName role = _database ? _database->role_for(ally) : StringName();
		if (!role.is_empty()) {
			allied_role_counts[role]++;
		}
	}

	// Evaluate all available candidates
	for (const StringName &candidate : available) {
		DraftPickScoreBreakdown breakdown = _evaluator->evaluate_candidate_pick(candidate, allies, enemies);
		
		// Calculate role_fit
		StringName candidate_role = _database ? _database->role_for(candidate) : StringName();
		breakdown.candidate_role = candidate_role;
		
		double role_fit = 0.0;
		if (!candidate_role.is_empty()) {
			auto it = allied_role_counts.find(candidate_role);
			if (it == allied_role_counts.end()) {
				// Role not present - positive bonus
				role_fit = 0.050;
			} else if (it->second == 1) {
				// Role appears once - small positive
				role_fit = 0.020;
			} else if (it->second == 2) {
				// Role appears twice - small negative
				role_fit = -0.030;
			} else {
				// Role appears 3+ times - larger negative
				role_fit = -0.060;
			}
		}
		breakdown.role_fit = role_fit;
		
		// Calculate comp_fit using partial composition lookup
		std::vector<StringName> candidate_comp_roles;
		for (const StringName &ally : allies) {
			StringName role = _database ? _database->role_for(ally) : StringName();
			if (!role.is_empty()) {
				candidate_comp_roles.push_back(role);
			}
		}
		if (!candidate_role.is_empty()) {
			candidate_comp_roles.push_back(candidate_role);
		}
		
		int comp_match_count = 0;
		DraftStatValue comp_value = _database ? _database->partial_role_combination_value_for(candidate_comp_roles, comp_match_count) : DraftStatValue();
		breakdown.comp_fit = comp_value.centered_value;
		breakdown.comp_fingerprint = _database ? DraftStatsDatabase::build_role_fingerprint(candidate_comp_roles) : String();
		breakdown.comp_samples = comp_value.samples;
		breakdown.comp_match_count = comp_match_count;
		
		// Apply phase-aware weights
		breakdown.total_score = 
			base_power_weight * breakdown.base_power +
			ally_synergy_weight * breakdown.ally_synergy +
			enemy_counter_value_weight * breakdown.enemy_counter_value +
			counter_risk_weight * breakdown.counter_risk +
			role_fit_weight * role_fit +
			comp_fit_weight * breakdown.comp_fit;

		// Set weight fields for debug output
		breakdown.base_power_weight = base_power_weight;
		breakdown.ally_synergy_weight = ally_synergy_weight;
		breakdown.enemy_counter_value_weight = enemy_counter_value_weight;
		breakdown.counter_risk_weight = counter_risk_weight;
		breakdown.role_fit_weight = role_fit_weight;
		breakdown.comp_fit_weight = comp_fit_weight;
		breakdown.draft_step = draft_step;
		breakdown.phase_label = phase_label;
		
		results.push_back(breakdown);
	}

	// Sort by total_score descending, then by champion name for deterministic tie-breaking
	std::sort(results.begin(), results.end(), [](const DraftPickScoreBreakdown &a, const DraftPickScoreBreakdown &b) {
		if (a.total_score != b.total_score) {
			return a.total_score > b.total_score;
		}
		return a.candidate < b.candidate;
	});

	// Apply lookahead if requested
	if (strategy == DraftStrategy::NATIVE_LOOKAHEAD || strategy == DraftStrategy::NATIVE_LOOKAHEAD_PICK) {
		const double response_weight = 0.35;
		const int lookahead_candidates = 8;
		
		// Use helper functions to determine if next pick is enemy response
		const String current_side = side_for_step(draft_step);
		const int next_pick_step = next_pick_step_after(draft_step);
		const String next_pick_side = (next_pick_step >= 0) ? side_for_step(next_pick_step) : "N/A";
		const bool next_pick_is_enemy = (next_pick_step >= 0) && (next_pick_side != "N/A") && (next_pick_side != current_side);
		
		// Only apply lookahead if next pick is an enemy response
		if (next_pick_is_enemy) {
			// Keep top 8 candidates for lookahead
			size_t top_n = std::min(static_cast<size_t>(lookahead_candidates), results.size());
			std::vector<DraftPickScoreBreakdown> lookahead_results;
			lookahead_results.reserve(top_n);
			
			for (size_t i = 0; i < top_n; ++i) {
				DraftPickScoreBreakdown candidate = results[i];
				
				// Store immediate score and turn information
				candidate.immediate_score = candidate.total_score;
				candidate.current_side = current_side;
				candidate.next_pick_step = next_pick_step;
				candidate.next_pick_side = next_pick_side;
				candidate.next_pick_is_enemy = next_pick_is_enemy;
				
				// Simulate: if we pick this candidate, what's enemy's best response?
				std::vector<StringName> new_allies = allies;
				new_allies.push_back(candidate.candidate);
				
				// Build new available set (exclude current candidate)
				std::vector<StringName> new_available;
				for (const StringName &avail : available) {
					if (avail != candidate.candidate) {
						new_available.push_back(avail);
					}
				}
				
				// Get enemy's best pick score from their perspective
				// Enemy's allies = our enemies, enemy's enemies = our new allies
				std::vector<DraftPickScoreBreakdown> enemy_responses = recommend_picks(
					new_available,
					enemies,  // Enemy's allies
					new_allies,  // Enemy's enemies (us with new pick)
					1,  // Only need top 1
					next_pick_step,  // Use the actual next pick step
					DraftStrategy::NATIVE_FULL  // Use baseline for response
				);
				
				if (!enemy_responses.empty()) {
					candidate.opponent_response_candidate = enemy_responses[0].candidate;
					candidate.opponent_response_score = enemy_responses[0].total_score;
					candidate.lookahead_adjustment = -response_weight * candidate.opponent_response_score;
				} else {
					candidate.opponent_response_candidate = StringName();
					candidate.opponent_response_score = 0.0;
					candidate.lookahead_adjustment = 0.0;
				}
				
				// Update total score with lookahead adjustment
				candidate.total_score = candidate.immediate_score + candidate.lookahead_adjustment;
				
				lookahead_results.push_back(candidate);
			}
			
			// Sort lookahead results by adjusted score
			std::sort(lookahead_results.begin(), lookahead_results.end(), [](const DraftPickScoreBreakdown &a, const DraftPickScoreBreakdown &b) {
				if (a.total_score != b.total_score) {
					return a.total_score > b.total_score;
				}
				return a.candidate < b.candidate;
			});
			
			// Replace results with lookahead results
			results = lookahead_results;
		}
	}

	// Return top max_results if specified
	if (max_results > 0 && static_cast<size_t>(max_results) < results.size()) {
		results.resize(static_cast<size_t>(max_results));
	}

	return results;
}

std::vector<DraftBanScoreBreakdown> DraftRecommender::recommend_bans(
	const std::vector<StringName> &available,
	const std::vector<StringName> &allies,
	const std::vector<StringName> &enemies,
	int max_results,
	int draft_step,
	const String &acting_side,
	const Dictionary &weight_overrides,
	DraftStrategy strategy
) const {
	// Normalize acting_side to lowercase "blue" or "red"
	static auto normalize_draft_side = [](const String &side) -> String {
		const String lower = side.to_lower();
		if (lower == "b" || lower == "blue") {
			return "blue";
		}
		if (lower == "r" || lower == "red") {
			return "red";
		}
		return lower;
	};
	
	const String normalized_side = normalize_draft_side(acting_side);
	
	std::vector<DraftBanScoreBreakdown> results;

	// Determine phase and weights
	double denial_value_weight = 0.60;
	double enemy_synergy_weight = 0.25;
	double counters_my_team_weight = 0.25;
	double fills_enemy_role_need_weight = 0.10;
	double enemy_comp_fit_weight = 0.08;
	double early_fallback_multiplier = 0.30;
	String phase_label = "default";

	if (draft_step >= 0 && draft_step <= 5) {
		// Phase 1 ban: steps 0-5
		denial_value_weight = 0.75;
		enemy_synergy_weight = 0.15;
		counters_my_team_weight = 0.20;
		fills_enemy_role_need_weight = 0.05;
		enemy_comp_fit_weight = 0.04;
		phase_label = "phase1_ban";
	} else if (draft_step >= 12 && draft_step <= 15) {
		// Phase 2 ban: steps 12-15
		denial_value_weight = 0.50;
		enemy_synergy_weight = 0.30;
		counters_my_team_weight = 0.35;
		fills_enemy_role_need_weight = 0.15;
		enemy_comp_fit_weight = 0.10;
		phase_label = "phase2_ban";
	}

	// Apply weight overrides if provided (multipliers applied to phase-specific weights)
	if (weight_overrides.size() > 0) {
		if (weight_overrides.has(Variant("denial_value_multiplier"))) {
			denial_value_weight *= double(weight_overrides[Variant("denial_value_multiplier")]);
		}
		if (weight_overrides.has(Variant("enemy_synergy_multiplier"))) {
			enemy_synergy_weight *= double(weight_overrides[Variant("enemy_synergy_multiplier")]);
		}
		if (weight_overrides.has(Variant("counters_my_team_multiplier"))) {
			counters_my_team_weight *= double(weight_overrides[Variant("counters_my_team_multiplier")]);
		}
		if (weight_overrides.has(Variant("fills_enemy_role_need_multiplier"))) {
			fills_enemy_role_need_weight *= double(weight_overrides[Variant("fills_enemy_role_need_multiplier")]);
		}
		if (weight_overrides.has(Variant("enemy_comp_fit_multiplier"))) {
			enemy_comp_fit_weight *= double(weight_overrides[Variant("enemy_comp_fit_multiplier")]);
		}
		if (weight_overrides.has(Variant("early_fallback_multiplier"))) {
			early_fallback_multiplier = double(weight_overrides[Variant("early_fallback_multiplier")]);
		}
	}

	// Determine who gets first pick after this ban phase
	bool enemy_gets_first_pick_after_ban_phase = false;
	if (phase_label == "phase1_ban") {
		// After phase 1 bans, Blue picks first at step 6
		// If we are Red, enemy (Blue) gets first pick
		enemy_gets_first_pick_after_ban_phase = (normalized_side == "red");
	} else if (phase_label == "phase2_ban") {
		// After phase 2 bans, Red picks first at step 16
		// If we are Blue, enemy (Red) gets first pick
		enemy_gets_first_pick_after_ban_phase = (normalized_side == "blue");
	}

	// Count enemy roles
	std::map<StringName, int> enemy_role_counts;
	for (const StringName &enemy : enemies) {
		StringName role = _database ? _database->role_for(enemy) : StringName();
		if (!role.is_empty()) {
			enemy_role_counts[role]++;
		}
	}

	// Evaluate all available candidates
	for (const StringName &candidate : available) {
		DraftBanScoreBreakdown breakdown = _evaluator->evaluate_candidate_ban(candidate, allies, enemies);
		
		// Calculate fills_enemy_role_need
		StringName candidate_role = _database ? _database->role_for(candidate) : StringName();
		breakdown.candidate_role = candidate_role;
		
		double fills_enemy_role_need = 0.0;
		if (!candidate_role.is_empty()) {
			auto it = enemy_role_counts.find(candidate_role);
			if (it == enemy_role_counts.end()) {
				// Role not present on enemy team - positive bonus
				fills_enemy_role_need = 0.050;
			} else if (it->second == 1) {
				// Role appears once - small positive
				fills_enemy_role_need = 0.020;
			} else {
				// Role appears 2+ times - small negative
				fills_enemy_role_need = -0.020;
			}
		}
		breakdown.fills_enemy_role_need = fills_enemy_role_need;
		
		// Calculate enemy_comp_fit using partial composition lookup
		std::vector<StringName> enemy_candidate_roles;
		for (const StringName &enemy : enemies) {
			StringName role = _database ? _database->role_for(enemy) : StringName();
			if (!role.is_empty()) {
				enemy_candidate_roles.push_back(role);
			}
		}
		if (!candidate_role.is_empty()) {
			enemy_candidate_roles.push_back(candidate_role);
		}
		
		int enemy_comp_match_count = 0;
		DraftStatValue enemy_comp_value = _database ? _database->partial_role_combination_value_for(enemy_candidate_roles, enemy_comp_match_count) : DraftStatValue();
		breakdown.enemy_comp_fit = enemy_comp_value.centered_value;
		breakdown.enemy_comp_fingerprint = _database ? DraftStatsDatabase::build_role_fingerprint(enemy_candidate_roles) : String();
		breakdown.enemy_comp_samples = enemy_comp_value.samples;
		breakdown.enemy_comp_match_count = enemy_comp_match_count;
		
		// Apply phase-aware weights with differential denial
		double denial_component = denial_value_weight * breakdown.denial_value;
		double early_ban_fallback_component = 0.0;
		
		// Early ban fallback when denial is negligible
		if (std::abs(breakdown.denial_value) < 0.001 && (phase_label == "phase1_ban" || phase_label == "phase2_ban")) {
			if (enemy_gets_first_pick_after_ban_phase) {
				// Enemy gets first pick after this ban phase - use modest enemy-value fallback
				early_ban_fallback_component = early_fallback_multiplier * std::max(0.0, breakdown.enemy_pick_value);
			} else {
				// We get first pick after this ban phase - avoid banning our own top picks
				// Use very small fallback or zero
				early_ban_fallback_component = 0.00;  // Prefer zero first
			}
		}
		
		breakdown.total_score = 
			denial_component +
			early_ban_fallback_component +
			enemy_synergy_weight * breakdown.enemy_synergy +
			counters_my_team_weight * breakdown.counters_my_team +
			fills_enemy_role_need_weight * fills_enemy_role_need +
			enemy_comp_fit_weight * breakdown.enemy_comp_fit;

		// Set weight fields for debug output
		breakdown.denial_value_weight = denial_value_weight;
		breakdown.enemy_synergy_weight = enemy_synergy_weight;
		breakdown.counters_my_team_weight = counters_my_team_weight;
		breakdown.fills_enemy_role_need_weight = fills_enemy_role_need_weight;
		breakdown.enemy_comp_fit_weight = enemy_comp_fit_weight;
		breakdown.draft_step = draft_step;
		breakdown.phase_label = phase_label;
		breakdown.acting_side = normalized_side;
		breakdown.enemy_gets_first_pick_after_ban_phase = enemy_gets_first_pick_after_ban_phase;
		breakdown.early_ban_fallback_component = early_ban_fallback_component;
		
		results.push_back(breakdown);
	}

	// Sort by total_score descending, then by champion name for deterministic tie-breaking
	std::sort(results.begin(), results.end(), [](const DraftBanScoreBreakdown &a, const DraftBanScoreBreakdown &b) {
		if (a.total_score != b.total_score) {
			return a.total_score > b.total_score;
		}
		return a.candidate < b.candidate;
	});

	// Apply lookahead if requested
	if (strategy == DraftStrategy::NATIVE_LOOKAHEAD || strategy == DraftStrategy::NATIVE_LOOKAHEAD_BAN) {
		const double denied_enemy_pick_weight = 0.50;
		const int lookahead_candidates = 8;
		
		// Use helper functions to find enemy's next pick step
		const String current_side = side_for_step(draft_step);
		const String enemy_side = (current_side == "B") ? "R" : "B";
		const int enemy_next_pick_step = next_pick_step_for_side_after(draft_step, enemy_side);
		
		// Only apply lookahead if enemy has a future pick
		if (enemy_next_pick_step >= 0) {
			// Keep top 8 candidates for lookahead
			size_t top_n = std::min(static_cast<size_t>(lookahead_candidates), results.size());
			std::vector<DraftBanScoreBreakdown> lookahead_results;
			lookahead_results.reserve(top_n);
			
			for (size_t i = 0; i < top_n; ++i) {
				DraftBanScoreBreakdown candidate = results[i];
				
				// Store immediate score and turn information
				candidate.immediate_score = candidate.total_score;
				candidate.current_side = current_side;
				candidate.enemy_next_pick_step = enemy_next_pick_step;
				
				// Get enemy's best pick score before ban (from enemy's perspective)
				std::vector<DraftPickScoreBreakdown> enemy_picks_before = recommend_picks(
					available,
					enemies,  // Enemy's allies
					allies,  // Enemy's enemies
					1,  // Only need top 1
					enemy_next_pick_step,  // Use enemy's next pick step
					DraftStrategy::NATIVE_FULL  // Use baseline
				);
				
				if (!enemy_picks_before.empty()) {
					candidate.enemy_best_pick_before = enemy_picks_before[0].candidate;
					candidate.enemy_best_pick_score_before = enemy_picks_before[0].total_score;
				} else {
					candidate.enemy_best_pick_before = StringName();
					candidate.enemy_best_pick_score_before = 0.0;
				}
				
				// Simulate: if we ban this candidate, what's enemy's best pick after?
				std::vector<StringName> new_available;
				for (const StringName &avail : available) {
					if (avail != candidate.candidate) {
						new_available.push_back(avail);
					}
				}
				
				std::vector<DraftPickScoreBreakdown> enemy_picks_after = recommend_picks(
					new_available,
					enemies,  // Enemy's allies
					allies,  // Enemy's enemies
					1,  // Only need top 1
					enemy_next_pick_step,  // Use enemy's next pick step
					DraftStrategy::NATIVE_FULL  // Use baseline
				);
				
				if (!enemy_picks_after.empty()) {
					candidate.enemy_best_pick_after = enemy_picks_after[0].candidate;
					candidate.enemy_best_pick_score_after = enemy_picks_after[0].total_score;
				} else {
					candidate.enemy_best_pick_after = StringName();
					candidate.enemy_best_pick_score_after = 0.0;
				}
				
				// Calculate denied value (how much we reduced enemy's best pick)
				candidate.denied_enemy_pick_value = std::max(0.0, candidate.enemy_best_pick_score_before - candidate.enemy_best_pick_score_after);
				candidate.lookahead_adjustment = denied_enemy_pick_weight * candidate.denied_enemy_pick_value;
				
				// Update total score with lookahead adjustment
				candidate.total_score = candidate.immediate_score + candidate.lookahead_adjustment;
				
				lookahead_results.push_back(candidate);
			}
			
			// Sort lookahead results by adjusted score
			std::sort(lookahead_results.begin(), lookahead_results.end(), [](const DraftBanScoreBreakdown &a, const DraftBanScoreBreakdown &b) {
				if (a.total_score != b.total_score) {
					return a.total_score > b.total_score;
				}
				return a.candidate < b.candidate;
			});
			
			// Replace results with lookahead results
			results = lookahead_results;
		}
	}

	// Return top max_results if specified
	if (max_results > 0 && static_cast<size_t>(max_results) < results.size()) {
		results.resize(static_cast<size_t>(max_results));
	}

	return results;
}

// Lookahead turn logic helpers
bool DraftRecommender::is_pick_step(int step) {
	// Fixed draft order: picks at steps 6-11 and 16-19
	return (step >= 6 && step <= 11) || (step >= 16 && step <= 19);
}

bool DraftRecommender::is_ban_step(int step) {
	// Fixed draft order: bans at steps 0-5 and 12-15
	return (step >= 0 && step <= 5) || (step >= 12 && step <= 15);
}

String DraftRecommender::side_for_step(int step) {
	// Fixed draft order based on the standard snake draft
	// 0 B_BAN, 1 R_BAN, 2 B_BAN, 3 R_BAN, 4 B_BAN, 5 R_BAN
	// 6 B_PICK, 7 R_PICK, 8 R_PICK, 9 B_PICK, 10 B_PICK, 11 R_PICK
	// 12 R_BAN, 13 B_BAN, 14 R_BAN, 15 B_BAN
	// 16 R_PICK, 17 B_PICK, 18 B_PICK, 19 R_PICK
	
	switch (step) {
		case 0: return "B";
		case 1: return "R";
		case 2: return "B";
		case 3: return "R";
		case 4: return "B";
		case 5: return "R";
		case 6: return "B";
		case 7: return "R";
		case 8: return "R";
		case 9: return "B";
		case 10: return "B";
		case 11: return "R";
		case 12: return "R";
		case 13: return "B";
		case 14: return "R";
		case 15: return "B";
		case 16: return "R";
		case 17: return "B";
		case 18: return "B";
		case 19: return "R";
		default: return "";
	}
}

int DraftRecommender::next_pick_step_after(int step) {
	// Find the next pick step after the given step
	for (int s = step + 1; s <= 19; ++s) {
		if (is_pick_step(s)) {
			return s;
		}
	}
	return -1; // No more picks
}

int DraftRecommender::next_pick_step_for_side_after(int step, const String &side) {
	// Find the next pick step where the given side acts
	for (int s = step + 1; s <= 19; ++s) {
		if (is_pick_step(s) && side_for_step(s) == side) {
			return s;
		}
	}
	return -1; // No more picks for this side
}

void DraftRecommender::generate_lookahead_turn_diagnostic() {
	// Write to file for reliable output
	String report_path = "lookahead_turn_logic_report.txt";
	Ref<FileAccess> file = FileAccess::open(report_path, FileAccess::WRITE);
	if (!file.is_valid()) {
		UtilityFunctions::push_error(vformat("Failed to open diagnostic report file: %s", report_path));
		return;
	}
	
	file->store_line("=== LOOKAHEAD TURN LOGIC DIAGNOSTIC ===");
	file->store_line("step\taction\tcurrent_side\tnext_pick_step\tnext_pick_side\tnext_pick_is_enemy\tenemy_next_pick_step\tally_next_pick_step");
	
	for (int step = 0; step <= 19; ++step) {
		String action = is_ban_step(step) ? "BAN" : "PICK";
		String current_side = side_for_step(step);
		int next_pick_step = next_pick_step_after(step);
		String next_pick_side = (next_pick_step >= 0) ? side_for_step(next_pick_step) : "";
		bool next_pick_is_enemy = (next_pick_step >= 0) && (next_pick_side != current_side);
		
		String enemy_side = (current_side == "B") ? "R" : "B";
		int enemy_next_pick_step = next_pick_step_for_side_after(step, enemy_side);
		int ally_next_pick_step = next_pick_step_for_side_after(step, current_side);
		
		String line = vformat("%d\t%s\t%s\t%d\t%s\t%s\t%d\t%d",
			step,
			action,
			current_side,
			next_pick_step,
			next_pick_side,
			next_pick_is_enemy ? "true" : "false",
			enemy_next_pick_step,
			ally_next_pick_step
		);
		file->store_line(line);
	}
	
	file->store_line("=== DIAGNOSTIC COMPLETE ===");
	file->close();
	
	UtilityFunctions::print("Lookahead turn diagnostic written to lookahead_turn_logic_report.txt");
}

} // namespace draft_ai
} // namespace sim
