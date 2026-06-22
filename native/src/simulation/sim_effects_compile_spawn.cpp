#include "sim_effects_compile_internal.hpp"

#include "sim_effects_compile.hpp"

#include "sim_constants.hpp"
#include "sim_effect_kinds.inl.hpp"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace sim::effects::compile::internal {

using namespace sim::effect_kinds;

bool try_fill_spawn(EffectRecord &compiled, const StringName &kind, ParamTracker &tracker, const Dictionary &params) {
	if (kind == sn_summon_ally()) {
		compiled.scalar0 = double(tracker.get("spawn_radius", 0.0));
		// Parse minions array: each entry has minion_id and count
		Array minions_array = tracker.get("minions", Array());
		for (int64_t i = 0; i < minions_array.size(); ++i) {
			Dictionary minion_entry = minions_array[i];
			StringName minion_id = StringName(String(minion_entry.get("minion_id", "")));
			int64_t count = int64_t(minion_entry.get("count", 1));
			// Store minion_id in string0 and count in int0 for each entry
			// We'll use children array to store multiple minion specs
			// Note: opcode not set - children are data containers, not executable sub-effects
			EffectRecord minion_spec;
			minion_spec.string0 = String(minion_id);
			minion_spec.int0 = count;
			compiled.children.push_back(minion_spec);
		}
		compiled.reason = String(tracker.get("reason", "Summon Ally"));
		return true;
	}
	if (kind == sn_multi_target()) {
		// Multi-target effect parameters
		compiled.int0 = int64_t(tracker.get("target_count", 1));
		String strategy_str = String(tracker.get("selection_strategy", "closest"));
		if (strategy_str == "random") {
			compiled.int1 = TARGET_SELECTION_RANDOM;
		} else if (strategy_str == "lowest_hp") {
			compiled.int1 = TARGET_SELECTION_LOWEST_HP;
		} else if (strategy_str == "highest_hp") {
			compiled.int1 = TARGET_SELECTION_HIGHEST_HP;
		} else if (strategy_str == "closest_to_target") {
			compiled.int1 = TARGET_SELECTION_CLOSEST_TO_TARGET;
		} else if (strategy_str == "lowest_percent_hp") {
			compiled.int1 = TARGET_SELECTION_LOWEST_PERCENT_HP;
		} else if (strategy_str == "highest_percent_hp") {
			compiled.int1 = TARGET_SELECTION_HIGHEST_PERCENT_HP;
		} else if (strategy_str == "closest") {
			compiled.int1 = TARGET_SELECTION_CLOSEST;
		} else {
			UtilityFunctions::push_error(vformat("Invalid selection_strategy '%s' for multi_target effect", strategy_str));
			compiled.int1 = -1;
		}
		compiled.int2 = tracker.get("include_self", false) ? 1 : 0;
		String handling_str = String(tracker.get("excess_handling", "drop"));
		if (handling_str == "stack") {
			compiled.int3 = EXCESS_TARGET_STACK;
		} else if (handling_str == "drop") {
			compiled.int3 = EXCESS_TARGET_DROP;
		} else {
			UtilityFunctions::push_error(vformat("Invalid excess_handling '%s' for multi_target effect", handling_str));
			compiled.int3 = -1;
		}
		compiled.int4 = int64_t(tracker.get("repeat_count", 1));
		compiled.scalar0 = double(tracker.get("radius", 0.0));
		String team_filter_str = String(tracker.get("team_filter", ""));
		if (team_filter_str == "ally") {
			compiled.team_filter = sn_ally();
		} else if (team_filter_str == "enemy") {
			compiled.team_filter = sn_enemy();
		} else {
			compiled.team_filter = StringName(team_filter_str);
		}
		
		// Compile sub_effects
		Variant sub_effects_value = tracker.get("sub_effects", Variant());
		if (sub_effects_value.get_type() == Variant::ARRAY) {
			Array sub_effects_array = sub_effects_value;
			compiled.sub_effects = compile_effect_array(sub_effects_array);
		} else if (sub_effects_value.get_type() == Variant::DICTIONARY) {
			Dictionary sub_effect_dict = sub_effects_value;
			compiled.sub_effects.push_back(compile_effect(sub_effect_dict));
		}
		
		compiled.reason = String(tracker.get("reason", "Multiple target"));
		return true;
	}
	return false;
}

} // namespace sim::effects::compile::internal
