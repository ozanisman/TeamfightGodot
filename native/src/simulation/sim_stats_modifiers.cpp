#include "sim_stats_modifiers.hpp"

#include "../stat_definitions.hpp"

namespace sim {
namespace stats_modifiers {

namespace {

bool is_valid_stat_name(const StringName &stat_name) {
#define X(name, def, min_val, max_val) \
	if (stat_name == StringName(#name)) { \
		return true; \
	}
	STAT_LIST
#undef X
	return false;
}

} // namespace

void apply_stat_modifier(UnitState &source, UnitState &target, StringName stat_name, double additive, double multiplicative, double duration, bool is_match_duration) {
	(void)source;
	if (additive == 0.0 && multiplicative == 1.0) {
		return;
	}

	// Skip attack speed modifications if base is 0
	if (stat_name == StringName("attack_speed") && target.combat.attack_speed == 0.0) {
		return;
	}

	// Invalidate cache when stat modifier is applied
	target.stats_dirty = true;

	if (additive < -10000.0) {
		additive = -10000.0;
	}
	if (multiplicative < 0.0) {
		multiplicative = 0.0;
	}
	if (multiplicative > 1000.0) {
		multiplicative = 1000.0;
	}
	if (duration < 0.0) {
		duration = 0.0;
	}

#define X(name, def, min_val, max_val) \
	if (stat_name == StringName(#name)) { \
		target.stat_additive_##name += additive; \
		target.stat_multiplicative_##name *= multiplicative; \
		return; \
	}
	STAT_LIST
#undef X
}

void apply_simple_stat_modifier(UnitState &source, UnitState &target, StringName stat_name, double additive, double multiplicative, double duration, bool is_match_duration, const String &reason) {
	if (additive == 0.0 && multiplicative == 1.0) {
		return;
	}
	if (duration <= 0.0) {
		if (is_match_duration) {
			// Persist zero-duration match modifiers so they survive respawn clears.
			// These accumulate (e.g. Bloodlust gains per takedown), so add to any
			// existing total rather than replacing it.
			String modifier_key = get_stack_key(stat_name, reason);
			Dictionary existing = Dictionary(target.stat_modifiers.get(modifier_key, Dictionary()));
			double total_additive = additive;
			double total_multiplicative = multiplicative;
			if (!existing.is_empty()) {
				total_additive += double(existing.get("additive", 0.0));
				total_multiplicative *= double(existing.get("multiplicative", 1.0));
			}
			apply_stat_modifier(source, target, stat_name, additive, multiplicative, 0.0, false);
			Dictionary entry;
			entry["stat_name"] = stat_name;
			entry["additive"] = total_additive;
			entry["multiplicative"] = total_multiplicative;
			entry["duration"] = 0.0;
			entry["is_match_duration"] = true;
			target.stat_modifiers[modifier_key] = entry;
			return;
		}
		apply_stat_modifier(source, target, stat_name, additive, multiplicative, duration, is_match_duration);
		return;
	}

	String modifier_key = get_stack_key(stat_name, reason);
	Dictionary existing = Dictionary(target.stat_modifiers.get(modifier_key, Dictionary()));
	if (!existing.is_empty()) {
		double previous_additive = double(existing.get("additive", 0.0));
		double previous_multiplicative = double(existing.get("multiplicative", 1.0));
		double inverse_multiplicative = previous_multiplicative != 0.0 ? 1.0 / previous_multiplicative : 1.0;
		apply_stat_modifier(source, target, stat_name, -previous_additive, inverse_multiplicative, 0.0, false);
	}

	apply_stat_modifier(source, target, stat_name, additive, multiplicative, 0.0, false);

	Dictionary entry;
	entry["stat_name"] = stat_name;
	entry["additive"] = additive;
	entry["multiplicative"] = multiplicative;
	entry["duration"] = duration;
	entry["is_match_duration"] = is_match_duration;
	target.stat_modifiers[modifier_key] = entry;
}

void clear_all_stat_modifiers(UnitState &unit) {
	// Store match-duration modifiers before clearing
	Array match_duration_keys;
	Array match_duration_modifiers;
	Array modifier_keys = unit.stat_modifiers.keys();
	for (int i = 0; i < modifier_keys.size(); i++) {
		Variant key_variant = modifier_keys[i];
		if (key_variant.get_type() != Variant::STRING) {
			continue;
		}
		String key = key_variant;
		Dictionary modifier = unit.stat_modifiers[key];
		bool is_match_duration = bool(modifier.get("is_match_duration", false));
		if (is_match_duration) {
			match_duration_keys.append(key);
			match_duration_modifiers.append(modifier);
		}
	}

	// Store match-duration stacks before clearing (e.g. Soul Feast)
	Array match_duration_stack_keys;
	Array match_duration_stacks;
	Array stack_keys = unit.stat_stacks.keys();
	for (int i = 0; i < stack_keys.size(); i++) {
		Variant key_variant = stack_keys[i];
		if (key_variant.get_type() != Variant::STRING) {
			continue;
		}
		String key = key_variant;
		Dictionary stack_entry = unit.stat_stacks[key];
		bool is_match_duration = bool(stack_entry.get("is_match_duration", false));
		if (is_match_duration) {
			match_duration_stack_keys.append(key);
			match_duration_stacks.append(stack_entry);
		}
	}

	// Clear additive/multiplicative for all stats
#define X(name, def, min_val, max_val) \
	unit.stat_additive_##name = 0.0; \
	unit.stat_multiplicative_##name = 1.0;
	STAT_LIST
#undef X

	// Invalidate cache
	unit.stats_dirty = true;

	// Clear stacks (match-duration stacks are restored below)
	unit.stat_stacks.clear();

	// Clear only non-match-duration modifiers
	unit.stat_modifiers.clear();

	// Re-apply match-duration modifiers
	for (int i = 0; i < match_duration_keys.size(); i++) {
		String key = match_duration_keys[i];
		Dictionary modifier = match_duration_modifiers[i];
		StringName stat_name = StringName(String(modifier.get("stat_name", "")));
		double additive = double(modifier.get("additive", 0.0));
		double multiplicative = double(modifier.get("multiplicative", 1.0));
		apply_stat_modifier(unit, unit, stat_name, additive, multiplicative, 0.0, false);
		unit.stat_modifiers[key] = modifier;
	}

	// Re-apply match-duration stacks (e.g. Soul Feast)
	for (int i = 0; i < match_duration_stack_keys.size(); i++) {
		String key = match_duration_stack_keys[i];
		Dictionary stack_entry = match_duration_stacks[i];
		StringName stat_name = StringName(String(key.substr(0, key.find("|"))));
		double applied_additive = double(stack_entry.get("applied_additive", 0.0));
		double applied_multiplicative = double(stack_entry.get("applied_multiplicative", 1.0));
		apply_stat_modifier(unit, unit, stat_name, applied_additive, applied_multiplicative, 0.0, false);
		unit.stat_stacks[key] = stack_entry;
	}
}

void update_stat_modifier_durations(UnitState &unit, double delta) {
	if (unit.stat_modifiers.is_empty()) {
		return;
	}
	Array keys_to_remove;
	Array modifier_keys = unit.stat_modifiers.keys();
	for (int i = 0; i < modifier_keys.size(); i++) {
		Variant key_variant = modifier_keys[i];
		if (key_variant.get_type() != Variant::STRING) {
			continue;
		}
		String key = key_variant;
		Dictionary modifier = unit.stat_modifiers[key];
		bool is_match_duration = bool(modifier.get("is_match_duration", false));
		if (is_match_duration) {
			continue;
		}
		double duration = double(modifier.get("duration", 0.0)) - delta;
		if (duration > 0.0) {
			modifier["duration"] = duration;
			unit.stat_modifiers[key] = modifier;
			continue;
		}
		StringName stat_name = StringName(String(modifier.get("stat_name", "")));
		double additive = double(modifier.get("additive", 0.0));
		double multiplicative = double(modifier.get("multiplicative", 1.0));
		double inverse_multiplicative = multiplicative != 0.0 ? 1.0 / multiplicative : 1.0;
		apply_stat_modifier(unit, unit, stat_name, -additive, inverse_multiplicative, 0.0, false);
		keys_to_remove.append(key);
	}
	for (int i = 0; i < keys_to_remove.size(); i++) {
		unit.stat_modifiers.erase(String(keys_to_remove[i]));
	}
}

void clear_expired_stat_modifiers(UnitState &unit) {
	// Dictionary system handles expiration - this function is now a no-op
	// Modifiers are cleared when their dictionary entries expire in update_stat_modifier_durations
	(void)unit;
}

void apply_stacked_stat_modifier(UnitState &source, UnitState &target, StringName stat_name, double additive, double multiplicative, double duration, bool is_match_duration, int max_stacks, StackBehavior stack_behavior, const String &reason) {
	if (max_stacks <= 0) {
		return;
	}

	String stack_key = get_stack_key(stat_name, reason);
	Dictionary stack_entry = Dictionary(target.stat_stacks.get(stack_key, Dictionary()));
	int current_stacks = int(stack_entry.get("current_stacks", 0));
	double previous_additive = double(stack_entry.get("applied_additive", 0.0));
	double previous_multiplicative = double(stack_entry.get("applied_multiplicative", 1.0));
	double previous_duration = double(stack_entry.get("duration", 0.0));

	if (current_stacks > 0) {
		double inverse_multiplicative = previous_multiplicative != 0.0 ? 1.0 / previous_multiplicative : 1.0;
		apply_stat_modifier(source, target, stat_name, -previous_additive, inverse_multiplicative, 0.0, false);
	}

	if (stack_behavior == StackBehavior::Reset) {
		current_stacks = 0;
		previous_duration = 0.0;
	}

	if (current_stacks < max_stacks) {
		current_stacks++;
	}

	double applied_additive = additive * double(current_stacks);
	double applied_multiplicative = 1.0;
	if (!Math::is_equal_approx(multiplicative, 1.0)) {
		applied_multiplicative = Math::pow(multiplicative, double(current_stacks));
	}
	apply_stat_modifier(source, target, stat_name, applied_additive, applied_multiplicative, 0.0, false);

	double next_duration = duration;
	if (stack_behavior == StackBehavior::Accumulate) {
		next_duration = previous_duration + duration;
	}
	if (next_duration < 0.0) {
		next_duration = 0.0;
	}

	stack_entry["current_stacks"] = current_stacks;
	stack_entry["max_stacks"] = max_stacks;
	stack_entry["duration"] = next_duration;
	stack_entry["additive_per_stack"] = additive;
	stack_entry["multiplicative_per_stack"] = multiplicative;
	stack_entry["applied_additive"] = applied_additive;
	stack_entry["applied_multiplicative"] = applied_multiplicative;
	stack_entry["stack_behavior"] = int(stack_behavior);
	stack_entry["is_match_duration"] = is_match_duration;
	target.stat_stacks[stack_key] = stack_entry;
}

String get_stack_key(StringName stat_name, const String &reason) {
	return String(stat_name) + "|" + reason;
}

int consume_stat_stacks(UnitState &unit, StringName stat_name, const String &reason) {
	if (!is_valid_stat_name(stat_name)) {
		return 0;
	}

	String stack_key = get_stack_key(stat_name, reason);
	Dictionary stack_entry = Dictionary(unit.stat_stacks.get(stack_key, Dictionary()));
	int current_stacks = int(stack_entry.get("current_stacks", 0));

	if (current_stacks > 0) {
		double applied_additive = double(stack_entry.get("applied_additive", 0.0));
		double applied_multiplicative = double(stack_entry.get("applied_multiplicative", 1.0));

		double inverse_multiplicative = applied_multiplicative != 0.0 ? 1.0 / applied_multiplicative : 1.0;
		apply_stat_modifier(unit, unit, stat_name, -applied_additive, inverse_multiplicative, 0.0, false);

		unit.stat_stacks.erase(stack_key);
	}

	return current_stacks;
}

void set_stat_stacks(UnitState &unit, StringName stat_name, const String &reason, int stack_count, double duration, bool to_max, int fallback_max_stacks, double fallback_additive_per_stack, double fallback_multiplicative_per_stack) {
	if (!is_valid_stat_name(stat_name)) {
		return;
	}

	String stack_key = get_stack_key(stat_name, reason);
	Dictionary stack_entry = Dictionary(unit.stat_stacks.get(stack_key, Dictionary()));

	int max_stacks = fallback_max_stacks > 0 ? fallback_max_stacks : 1;
	double additive_per_stack = fallback_additive_per_stack;
	double multiplicative_per_stack = fallback_multiplicative_per_stack;
	double current_duration = duration;
	int stack_behavior = int(StackBehavior::Refresh);
	bool is_match_duration = false;
	bool entry_existed = !stack_entry.is_empty();

	if (entry_existed) {
		double applied_additive = double(stack_entry.get("applied_additive", 0.0));
		double applied_multiplicative = double(stack_entry.get("applied_multiplicative", 1.0));
		double inverse_multiplicative = applied_multiplicative != 0.0 ? 1.0 / applied_multiplicative : 1.0;
		apply_stat_modifier(unit, unit, stat_name, -applied_additive, inverse_multiplicative, 0.0, false);

		max_stacks = int(stack_entry.get("max_stacks", max_stacks));
		additive_per_stack = double(stack_entry.get("additive_per_stack", additive_per_stack));
		multiplicative_per_stack = double(stack_entry.get("multiplicative_per_stack", multiplicative_per_stack));
		if (duration <= 0.0) {
			current_duration = double(stack_entry.get("duration", 0.0));
		}
		stack_behavior = int(stack_entry.get("stack_behavior", int(StackBehavior::Refresh)));
		is_match_duration = bool(stack_entry.get("is_match_duration", false));
	}

	int final_stacks = stack_count;
	if (to_max) {
		final_stacks = max_stacks;
	}
	if (final_stacks < 0) {
		final_stacks = 0;
	}
	if (final_stacks > max_stacks) {
		final_stacks = max_stacks;
	}

	double new_applied_additive = additive_per_stack * double(final_stacks);
	double new_applied_multiplicative = 1.0;
	if (!Math::is_equal_approx(multiplicative_per_stack, 1.0)) {
		new_applied_multiplicative = Math::pow(multiplicative_per_stack, double(final_stacks));
	}

	apply_stat_modifier(unit, unit, stat_name, new_applied_additive, new_applied_multiplicative, 0.0, false);

	stack_entry["current_stacks"] = final_stacks;
	stack_entry["max_stacks"] = max_stacks;
	stack_entry["duration"] = current_duration;
	stack_entry["additive_per_stack"] = additive_per_stack;
	stack_entry["multiplicative_per_stack"] = multiplicative_per_stack;
	stack_entry["applied_additive"] = new_applied_additive;
	stack_entry["applied_multiplicative"] = new_applied_multiplicative;
	stack_entry["stack_behavior"] = stack_behavior;
	stack_entry["is_match_duration"] = is_match_duration;
	unit.stat_stacks[stack_key] = stack_entry;
}

void update_stacks(UnitState &unit, double delta, double current_time) {
	(void)current_time;
	if (unit.stat_stacks.is_empty()) {
		return;
	}
	Array keys_to_remove;
	Array stack_keys = unit.stat_stacks.keys();
	for (int i = 0; i < stack_keys.size(); i++) {
		Variant key_variant = stack_keys[i];
		if (key_variant.get_type() != Variant::STRING) {
			continue;
		}
		String key = key_variant;
		Dictionary stack_dict = unit.stat_stacks[key];
		double duration = double(stack_dict.get("duration", 0.0));
		if (duration <= 0.0) {
			continue;
		}
		duration -= delta;
		if (duration > 0.0) {
			stack_dict["duration"] = duration;
			unit.stat_stacks[key] = stack_dict;
			continue;
		}

		StringName stat_name = StringName(key.get_slice("|", 0));
		double additive = double(stack_dict.get("applied_additive", 0.0));
		double multiplicative = double(stack_dict.get("applied_multiplicative", 1.0));
		double inverse_multiplicative = multiplicative != 0.0 ? 1.0 / multiplicative : 1.0;
		int current_stacks = int(stack_dict.get("current_stacks", 0));
		if (current_stacks > 0) {
			apply_stat_modifier(unit, unit, stat_name, -additive, inverse_multiplicative, 0.0, false);
		}

		keys_to_remove.append(key);
	}
	for (int i = 0; i < keys_to_remove.size(); i++) {
		unit.stat_stacks.erase(String(keys_to_remove[i]));
	}
}

void cleanup_expired_stacks(UnitState &unit, double current_time) {
	(void)unit;
	(void)current_time;
}

} // namespace stats_modifiers
} // namespace sim