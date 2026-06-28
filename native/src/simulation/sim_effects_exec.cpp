#include "sim_effects_exec.hpp"
#include "sim_effects_exec_internal.hpp"
#include "sim_effects_compile.hpp"

#include "sim_targeting.hpp"

#include <godot_cpp/variant/utility_functions.hpp>

#include <cstdint>
#include <unordered_map>

namespace sim::effects::execution {
namespace internal {

namespace {

Dictionary run_multi_effect_children_impl(
		const EffectRecord &effect,
		size_t start_child_index,
		EffectContext &context,
		Dictionary combined_results,
		SimWorld &world,
		SimHostCallbacks &host,
		const SimExecCallbacks &hooks,
		const ::sim::effects::SimMatchHost &match_host) {
	bool any_success = false;
	for (size_t child_index = start_child_index; child_index < effect.children.size(); ++child_index) {
		const EffectRecord &child = effect.children[child_index];
		context.multi_effect_has_remaining_siblings = (child_index + 1 < effect.children.size());
		context.accumulated_results = combined_results;
		Dictionary child_result = execute_recursive(child, context, world, host, hooks, match_host);
		if (child_result.has("success") && bool(child_result.get("success", false))) {
			any_success = true;
		}
		const StringName &child_key = result_slot_key(child);
		if (!child_key.is_empty()) {
			combined_results[child_key] = child_result;
			context.accumulated_results = combined_results;
		}
		if (child.opcode == EFFECT_OPCODE_MULTI_TARGET && context.source != nullptr) {
			UnitStateCold &cold = uc(world, *context.source);
			if (cold.deferred_multi_target_active || cold.deferred_effect_outstanding_projectiles > 0) {
				cold.deferred_multi_effect_active = true;
				cold.deferred_multi_effect_record = effect;
				cold.deferred_multi_effect_next_child = child_index + 1;
				cold.deferred_multi_effect_context = context;
				cold.deferred_multi_effect_combined_results = combined_results;
				combined_results["success"] = any_success;
				return combined_results;
			}
		}
	}
	combined_results["success"] = any_success;
	return combined_results;
}

} // namespace

Dictionary run_multi_effect_children(
		const EffectRecord &effect,
		size_t start_child_index,
		EffectContext &context,
		Dictionary combined_results,
		SimWorld &world,
		SimHostCallbacks &host,
		const SimExecCallbacks &hooks,
		const ::sim::effects::SimMatchHost &match_host) {
	return run_multi_effect_children_impl(effect, start_child_index, context, combined_results, world, host, hooks, match_host);
}

void resume_deferred_multi_effect(SimWorld &world, SimHostCallbacks &host, UnitState &source) {
	UnitStateCold &cold = uc(world, source);
	if (!cold.deferred_multi_effect_active || host.effect_exec == nullptr || host.effect_exec->exec_callbacks == nullptr) {
		return;
	}
	EffectContext context = cold.deferred_multi_effect_context;
	context.damage += cold.deferred_effect_projectile_dealt_damage;
	context.channel_accumulated_damage = cold.channel_accumulated_damage;
	cold.deferred_effect_projectile_dealt_damage = 0.0;
	run_multi_effect_children_impl(
			cold.deferred_multi_effect_record,
			cold.deferred_multi_effect_next_child,
			context,
			cold.deferred_multi_effect_combined_results,
			world,
			host,
			*host.effect_exec->exec_callbacks,
			host.effect_exec->match_host);
	if (cold.deferred_effect_outstanding_projectiles == 0 && !cold.deferred_multi_target_active) {
		cold.deferred_multi_effect_active = false;
	}
}

static void record_multi_target_sub_result(
		const EffectRecord &sub_effect,
		const Dictionary &sub_result,
		int64_t target_id,
		Dictionary &nested_results,
		Dictionary &summary_applications) {
	const StringName &effect_kind = sim::effects::compile::kind_for_opcode(sub_effect.opcode);
	const StringName &result_key = sim::effects::compile::result_slot_key(sub_effect);
	if (result_key.is_empty()) {
		return;
	}
	if (!sub_result.has("success") || !bool(sub_result.get("success", false))) {
		return;
	}
	String result_key_str = String(result_key);
	if (!nested_results.has(result_key_str)) {
		Dictionary effect_dict;
		Dictionary by_target_dict;
		effect_dict["by_target"] = by_target_dict;
		effect_dict["total"] = 0.0;
		nested_results[result_key_str] = effect_dict;
	}
	Dictionary effect_dict = nested_results[result_key_str];
	Dictionary by_target = effect_dict["by_target"];
	if (!by_target.has(target_id)) {
		by_target[target_id] = 0.0;
	}
	if (!summary_applications.has(result_key_str)) {
		Dictionary summary_by_target;
		summary_applications[result_key_str] = summary_by_target;
	}
	Dictionary summary_by_target = summary_applications[result_key_str];
	if (!summary_by_target.has(target_id)) {
		Array applications;
		summary_by_target[target_id] = applications;
	}
	double value = 0.0;
	if (effect_kind == sn_damage()) {
		value = double(sub_result.get("damage_dealt", 0.0));
	} else if (effect_kind == sn_heal()) {
		value = double(sub_result.get("amount", 0.0));
	} else if (effect_kind == sn_shield()) {
		value = double(sub_result.get("amount", 0.0));
	} else {
		value = 1.0;
	}
	Array applications = summary_by_target[target_id];
	applications.append(value);
	summary_by_target[target_id] = applications;
	summary_applications[result_key_str] = summary_by_target;
	by_target[target_id] = double(by_target[target_id]) + value;
	effect_dict["total"] = double(effect_dict["total"]) + value;
	nested_results[result_key_str] = effect_dict;
}

static bool sub_effect_reads_chain_damage(const EffectRecord &sub_effect) {
	switch (sub_effect.opcode) {
		case EFFECT_OPCODE_DAMAGE_BASED_HEAL:
		case EFFECT_OPCODE_DAMAGE_BASED_SHIELD:
		case EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER:
			return true;
		case EFFECT_OPCODE_SHIELD:
			return sub_effect.scalar1 > 0.0;
		case EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN:
			return sub_effect.scalar0 != 0.0;
		case EFFECT_OPCODE_DAMAGE:
			return sub_effect.int1 != 0;
		case EFFECT_OPCODE_AOE_DAMAGE:
			// TODO: aoe_damage only defers when reading channel_accumulated_damage.
			//       To support splash scaled from projectile impact damage, add a use_context_damage flag to the opcode.
			return sub_effect.int0 != 0;
		default:
			return false;
	}
}

static bool remaining_sub_effects_read_chain_damage(const EffectRecord &effect, size_t after_sub_index) {
	for (size_t i = after_sub_index + 1; i < effect.sub_effects.size(); ++i) {
		if (sub_effect_reads_chain_damage(effect.sub_effects[i])) {
			return true;
		}
	}
	return false;
}

static void save_deferred_multi_target_state(
		UnitStateCold &cold,
		const EffectRecord &effect,
		EffectContext &parent_context,
		EffectContext &target_context,
		const Dictionary &nested_results,
		const Dictionary &summary_applications,
		const std::vector<UnitState *> &targets,
		int64_t scratch_target_id,
		size_t next_sub_effect,
		int64_t next_repeat,
		bool any_success) {
	cold.deferred_multi_target_active = true;
	cold.deferred_multi_target_effect = effect;
	cold.deferred_multi_target_parent_context = parent_context;
	cold.deferred_multi_target_target_context = target_context;
	cold.deferred_multi_target_nested_results = nested_results;
	cold.deferred_multi_target_summary_applications = summary_applications;
	cold.deferred_multi_target_any_success = any_success;
	cold.deferred_multi_target_target_ids.clear();
	cold.deferred_multi_target_target_ids.reserve(targets.size());
	for (UnitState *target_unit : targets) {
		if (target_unit != nullptr) {
			cold.deferred_multi_target_target_ids.push_back(target_unit->instance_id);
		}
	}
	cold.deferred_multi_target_scratch_target_id = scratch_target_id;
	cold.deferred_multi_target_target_entry_index = 0;
	for (size_t i = 0; i < cold.deferred_multi_target_target_ids.size(); ++i) {
		if (cold.deferred_multi_target_target_ids[i] == scratch_target_id) {
			cold.deferred_multi_target_target_entry_index = i;
			break;
		}
	}
	cold.deferred_multi_target_next_sub_effect = next_sub_effect;
	cold.deferred_multi_target_next_repeat = next_repeat;
}

static Dictionary run_multi_target_impl(
		const EffectRecord &effect,
		EffectContext &context,
		SimWorld &world,
		SimHostCallbacks &host,
		const SimExecCallbacks &hooks,
		const ::sim::effects::SimMatchHost &match_host,
		UnitState &source,
		UnitState *target,
		size_t start_target_entry_index,
		size_t start_sub_effect_index,
		int64_t start_repeat_index,
		Dictionary nested_results,
		Dictionary summary_applications,
		std::unordered_map<int64_t, EffectContext> *resume_scratch_by_target,
		bool &any_success) {
	Dictionary multi_result;
	multi_result["success"] = true;

	const int64_t repeat_count = effect.int4;
	std::vector<UnitState *> targets;
	if (resume_scratch_by_target != nullptr) {
		UnitStateCold &cold = uc(world, source);
		targets.reserve(cold.deferred_multi_target_target_ids.size());
		for (int64_t target_id : cold.deferred_multi_target_target_ids) {
			UnitState *target_unit = unit_by_id(world, target_id);
			if (target_unit != nullptr) {
				targets.push_back(target_unit);
			}
		}
	} else {
		targets = sim::targeting::select_targets(
				world,
				host,
				source,
				target,
				effect.int0,
				static_cast<TargetSelectionStrategy>(effect.int1),
				effect.int2 != 0,
				static_cast<ExcessTargetHandling>(effect.int3),
				effect.team_filter,
				effect.scalar0);
	}

	if (targets.empty()) {
		multi_result["success"] = false;
		multi_result["targets_affected"] = 0;
		return multi_result;
	}

	std::unordered_map<int64_t, EffectContext> scratch_by_target;
	if (resume_scratch_by_target != nullptr) {
		scratch_by_target = std::move(*resume_scratch_by_target);
	}

	for (size_t ti = start_target_entry_index; ti < targets.size(); ++ti) {
		UnitState *current_target = targets[ti];
		if (current_target == nullptr) {
			continue;
		}
		const int64_t target_id = current_target->instance_id;
		auto scratch_it = scratch_by_target.find(target_id);
		if (scratch_it == scratch_by_target.end()) {
			EffectContext target_context = context;
			target_context.target = current_target;
			target_context.target_ally = current_target;
			scratch_by_target.emplace(target_id, target_context);
			scratch_it = scratch_by_target.find(target_id);
		}
		EffectContext &target_context = scratch_it->second;
		target_context.target = current_target;
		target_context.target_ally = current_target;
		const double damage_before_pass = target_context.damage;

		const size_t sub_start = (ti == start_target_entry_index) ? start_sub_effect_index : 0;
		for (size_t si = sub_start; si < effect.sub_effects.size(); ++si) {
			const EffectRecord &sub_effect = effect.sub_effects[si];
			const int64_t repeat_start = (ti == start_target_entry_index && si == sub_start) ? start_repeat_index : 0;
			for (int64_t ri = repeat_start; ri < repeat_count; ++ri) {
				const bool needs_projectile_defer =
						context.multi_effect_has_remaining_siblings ||
						remaining_sub_effects_read_chain_damage(effect, si);
				const int32_t outstanding_before = uc(world, source).deferred_effect_outstanding_projectiles;
				if (sub_effect.opcode == EFFECT_OPCODE_PROJECTILE && needs_projectile_defer) {
					target_context.track_spawned_projectile_for_deferred_chain = true;
				}
				Dictionary sub_result = execute_recursive(sub_effect, target_context, world, host, hooks, match_host);
				target_context.track_spawned_projectile_for_deferred_chain = false;
				if (sub_result.has("success") && bool(sub_result.get("success", false))) {
					any_success = true;
				}

				const StringName &sub_key = result_slot_key(sub_effect);
				if (!sub_key.is_empty()) {
					Dictionary accumulated = target_context.accumulated_results;
					accumulated[sub_key] = sub_result;
					target_context.accumulated_results = accumulated;
				}

				context.knockback_applied = context.knockback_applied || target_context.knockback_applied;
				record_multi_target_sub_result(sub_effect, sub_result, target_id, nested_results, summary_applications);

				const bool spawned_projectile =
						sub_effect.opcode == EFFECT_OPCODE_PROJECTILE && bool(sub_result.get("projectile_created", false));
				if (spawned_projectile && needs_projectile_defer &&
						uc(world, source).deferred_effect_outstanding_projectiles > outstanding_before) {
					size_t next_sub = si;
					int64_t next_repeat = ri + 1;
					if (next_repeat >= repeat_count) {
						next_repeat = 0;
						next_sub = si + 1;
					}
					context.damage += target_context.damage - damage_before_pass;
					save_deferred_multi_target_state(
							uc(world, source),
							effect,
							context,
							target_context,
							nested_results,
							summary_applications,
							targets,
							target_id,
							next_sub,
							next_repeat,
							any_success);
					multi_result["targets_affected"] = targets.size();
					multi_result["results"] = nested_results;
					multi_result["success"] = any_success;
					return multi_result;
				}
			}
		}

		context.damage += target_context.damage - damage_before_pass;
	}

	uc(world, source).deferred_multi_target_active = false;
	multi_result["targets_affected"] = targets.size();
	multi_result["results"] = nested_results;
	multi_result["success"] = any_success;
	return multi_result;
}

void resume_deferred_multi_target(SimWorld &world, SimHostCallbacks &host, UnitState &source) {
	UnitStateCold &cold = uc(world, source);
	if (!cold.deferred_multi_target_active || host.effect_exec == nullptr || host.effect_exec->exec_callbacks == nullptr) {
		return;
	}

	while (cold.deferred_multi_target_target_entry_index < cold.deferred_multi_target_target_ids.size()) {
		const int64_t resume_target_id =
				cold.deferred_multi_target_target_ids[cold.deferred_multi_target_target_entry_index];
		UnitState *resume_target = unit_by_id(world, resume_target_id);
		if (resume_target != nullptr && resume_target->alive) {
			break;
		}
		cold.deferred_multi_target_target_entry_index += 1;
		cold.deferred_multi_target_next_sub_effect = 0;
		cold.deferred_multi_target_next_repeat = 0;
	}
	if (cold.deferred_multi_target_target_entry_index >= cold.deferred_multi_target_target_ids.size()) {
		cold.deferred_multi_target_active = false;
		return;
	}

	EffectContext parent_context = cold.deferred_multi_target_parent_context;
	const double projectile_dealt = cold.deferred_effect_projectile_dealt_damage;
	parent_context.damage += projectile_dealt;
	parent_context.channel_accumulated_damage = cold.channel_accumulated_damage;
	cold.deferred_effect_projectile_dealt_damage = 0.0;

	const int64_t target_id = cold.deferred_multi_target_target_ids[cold.deferred_multi_target_target_entry_index];
	const int64_t scratch_target_id = cold.deferred_multi_target_scratch_target_id;
	std::unordered_map<int64_t, EffectContext> scratch_by_target;
	if (target_id == scratch_target_id) {
		EffectContext target_scratch = cold.deferred_multi_target_target_context;
		target_scratch.damage += projectile_dealt;
		target_scratch.channel_accumulated_damage = cold.channel_accumulated_damage;
		scratch_by_target.emplace(target_id, target_scratch);
	}

	std::vector<UnitState *> targets;
	targets.reserve(cold.deferred_multi_target_target_ids.size());
	for (int64_t resume_target_id : cold.deferred_multi_target_target_ids) {
		UnitState *target_unit = unit_by_id(world, resume_target_id);
		if (target_unit != nullptr) {
			targets.push_back(target_unit);
		}
	}
	size_t start_ti = 0;
	bool found_start = false;
	for (size_t i = 0; i < targets.size(); ++i) {
		if (targets[i]->instance_id == target_id) {
			start_ti = i;
			found_start = true;
			break;
		}
	}
	if (!found_start) {
		cold.deferred_multi_target_active = false;
		return;
	}

	run_multi_target_impl(
			cold.deferred_multi_target_effect,
			parent_context,
			world,
			host,
			*host.effect_exec->exec_callbacks,
			host.effect_exec->match_host,
			source,
			parent_context.target,
			start_ti,
			cold.deferred_multi_target_next_sub_effect,
			cold.deferred_multi_target_next_repeat,
			cold.deferred_multi_target_nested_results,
			cold.deferred_multi_target_summary_applications,
			&scratch_by_target,
			cold.deferred_multi_target_any_success);
	if (!cold.deferred_multi_target_active && cold.deferred_multi_effect_active) {
		cold.deferred_multi_effect_context.damage = parent_context.damage;
		cold.deferred_multi_effect_context.channel_accumulated_damage = parent_context.channel_accumulated_damage;
		cold.deferred_multi_effect_context.knockback_applied =
				cold.deferred_multi_effect_context.knockback_applied || parent_context.knockback_applied;
	}
}

void try_complete_deferred_projectile_chains(SimWorld &world, SimHostCallbacks &host, UnitState &source) {
	UnitStateCold &cold = uc(world, source);
	if (cold.deferred_effect_outstanding_projectiles > 0) {
		return;
	}
	if (cold.deferred_multi_target_active) {
		resume_deferred_multi_target(world, host, source);
	}
	if (!cold.deferred_multi_target_active && cold.deferred_multi_effect_active) {
		resume_deferred_multi_effect(world, host, source);
	}
}

void abandon_deferred_projectile(SimWorld &world, SimHostCallbacks &host, const ProjectileState &projectile) {
	if (!projectile.counts_toward_deferred_outstanding) {
		return;
	}
	UnitState *source = unit_by_id(world, projectile.source_id);
	if (source == nullptr || !source->alive) {
		return;
	}
	UnitStateCold &cold = uc(world, *source);
	if (cold.deferred_effect_outstanding_projectiles > 0) {
		cold.deferred_effect_outstanding_projectiles -= 1;
		if (cold.deferred_effect_outstanding_projectiles == 0) {
			try_complete_deferred_projectile_chains(world, host, *source);
		}
	}
}

ExecRoute exec_route_for_opcode(int64_t opcode) {
	switch (opcode) {
		// Core effects (1-99)
		case EFFECT_OPCODE_MULTI_EFFECT: // 1
			return ExecRoute::MultiEffect;
		case EFFECT_OPCODE_DAMAGE: // 2
			return ExecRoute::Damage;
		case EFFECT_OPCODE_PROJECTILE: // 3
			return ExecRoute::Spawn;
		case EFFECT_OPCODE_MULTI_TARGET: // 4
			return ExecRoute::MultiTarget;
		case EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER: // 5
			return ExecRoute::Damage;
		case EFFECT_OPCODE_AUTO_DODGE: // 6
			return ExecRoute::Damage;
		case EFFECT_OPCODE_CONSTANT_MULTIPLIER: // 7
			return ExecRoute::Damage;
		case EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER: // 8
			return ExecRoute::Damage;
		case EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER: // 9
			return ExecRoute::Damage;
		case EFFECT_OPCODE_REFLECT_DAMAGE: // 10
			return ExecRoute::Damage;
		case EFFECT_OPCODE_TARGET_STATUS_MULTIPLIER: // 11
			return ExecRoute::Damage;
		case EFFECT_OPCODE_STAT_MODIFIER: // 12
			return ExecRoute::Damage;
		case EFFECT_OPCODE_DAMAGE_OVER_TIME: // 13
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_CONSUME_STACKS_DAMAGE: // 14
			return ExecRoute::Damage;
		case EFFECT_OPCODE_REDIRECT_DAMAGE: // 15
			return ExecRoute::Damage;
		case EFFECT_OPCODE_SUMMON_ALLY: // 16
			return ExecRoute::Spawn;

		// Status effects (100-199)
		case EFFECT_OPCODE_STUN: // 100
			return ExecRoute::Status;
		case EFFECT_OPCODE_SHIELD: // 101
			return ExecRoute::Status;
		case EFFECT_OPCODE_HEAL: // 102
			return ExecRoute::Status;
		case EFFECT_OPCODE_MANA_REGEN: // 103
			return ExecRoute::Status;
		case EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN: // 104
			return ExecRoute::Status;
		case EFFECT_OPCODE_DAMAGE_BASED_HEAL: // 105
			return ExecRoute::Status;
		case EFFECT_OPCODE_MANA_RESTORE_ON_HIT: // 106
			return ExecRoute::Status;
		case EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT: // 107
			return ExecRoute::Status;
		case EFFECT_OPCODE_EVERY_N_ATTACKS_STUN: // 108
			return ExecRoute::Status;
		case EFFECT_OPCODE_SELF_DASH: // 109
			return ExecRoute::Status;
		case EFFECT_OPCODE_SLOW: // 110
			return ExecRoute::Status;
		case EFFECT_OPCODE_ROOT: // 111
			return ExecRoute::Status;
		case EFFECT_OPCODE_SILENCE: // 112
			return ExecRoute::Status;
		case EFFECT_OPCODE_DISARM: // 113
			return ExecRoute::Status;
		case EFFECT_OPCODE_KNOCKBACK: // 114
			return ExecRoute::Status;
		case EFFECT_OPCODE_REFLECT: // 115
			return ExecRoute::Status;
		case EFFECT_OPCODE_STEALTH: // 116
			return ExecRoute::Status;
		case EFFECT_OPCODE_HEAL_OVER_TIME: // 118
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_DAMAGE_BASED_SHIELD: // 119
			return ExecRoute::Status;
		case EFFECT_OPCODE_CONSUME_STACKS_HEAL: // 120
			return ExecRoute::Status;
		case EFFECT_OPCODE_CONSUME_STACKS_SHIELD: // 121
			return ExecRoute::Status;
		case EFFECT_OPCODE_SET_STACKS: // 122
			return ExecRoute::Status;
		case EFFECT_OPCODE_CHANNEL: // 123
			return ExecRoute::Status;

		// AOE effects (200-299)
		case EFFECT_OPCODE_AOE_TAUNT: // 200
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_DAMAGE: // 201
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_SLOW: // 202
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_ROOT: // 203
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_SILENCE: // 204
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_DISARM: // 205
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_KNOCKBACK: // 206
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_REFLECT: // 207
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_STUN: // 208
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME: // 209
			return ExecRoute::Aoe;
		case EFFECT_OPCODE_AOE_HEAL_OVER_TIME: // 210
			return ExecRoute::Aoe;

		default:
			return ExecRoute::DefaultOk;
	}
}

Dictionary execute_recursive(
		const EffectRecord &effect,
		EffectContext &context,
		SimWorld &world,
		SimHostCallbacks &host,
		const SimExecCallbacks &hooks,
		const ::sim::effects::SimMatchHost &match_host) {
	return execute_impl(effect, context, world, host, hooks, match_host);
}

Dictionary execute_impl(const EffectRecord &effect, EffectContext &context, SimWorld &world, SimHostCallbacks &host, const SimExecCallbacks &hooks, const effects::SimMatchHost &match_host) {
	Dictionary result;
	
	// DEBUG: Log when _execute_effect is called with unknown opcode
	if (effect.opcode == EFFECT_OPCODE_UNKNOWN) {
		UtilityFunctions::push_error(vformat("[DEBUG] _execute_effect: called with EFFECT_OPCODE_UNKNOWN for source_id=%d", context.source != nullptr ? context.source->instance_id : 0));
	}
	
	if (context.source == nullptr) {
		return result;
	}
	UnitState &source = *context.source;
	UnitState *target = context.target;
	UnitState *target_ally = context.target_ally;
	
	// Check conditional requirements
	if (!check_all_conditions(effect, context.accumulated_results, context, world)) {
		Dictionary failed_result;
		failed_result["success"] = false;
		failed_result["condition_failed"] = true;
		return failed_result;
	}
	
	switch (exec_route_for_opcode(effect.opcode)) {
		case ExecRoute::Damage:
			return exec_damage(effect, context, world, host, hooks, match_host, source, target, target_ally);
		case ExecRoute::Status:
			return exec_status(effect, context, world, host, hooks, match_host, source, target, target_ally);
		case ExecRoute::Spawn:
			return exec_spawn(effect, context, world, host, hooks, match_host, source, target, target_ally);
		case ExecRoute::Aoe:
			return exec_aoe(effect, context, world, host, hooks, match_host, source, target, target_ally);
		case ExecRoute::MultiEffect: {
			Dictionary combined_results;
			return run_multi_effect_children_impl(effect, 0, context, combined_results, world, host, hooks, match_host);
		}
		case ExecRoute::MultiTarget: {
			Dictionary multi_result;
			multi_result["success"] = true;
			
			// Extract multi-target parameters
			int64_t target_count = effect.int0;
			TargetSelectionStrategy strategy = static_cast<TargetSelectionStrategy>(effect.int1);
			bool include_self = effect.int2 != 0;
			ExcessTargetHandling excess_handling = static_cast<ExcessTargetHandling>(effect.int3);
			int64_t repeat_count = effect.int4;
			double radius = effect.scalar0;
			
			// Validate target_count (must be >= 1, where -1 means "all")
			if (target_count != -1 && target_count < 1) {
				UtilityFunctions::push_error(vformat("Invalid target_count %d, must be >= 1 (-1 for all targets)", target_count));
				multi_result["success"] = false;
				return multi_result;
			}
			
			// Validate repeat_count (must be >= 1)
			if (repeat_count < 1) {
				UtilityFunctions::push_error(vformat("Invalid repeat_count %d, must be >= 1", repeat_count));
				multi_result["success"] = false;
				return multi_result;
			}
			
			if (effect.int1 < TARGET_SELECTION_CLOSEST || effect.int1 > TARGET_SELECTION_HIGHEST_PERCENT_HP) {
				UtilityFunctions::push_error(vformat("Invalid selection_strategy opcode %d for multi_target effect", effect.int1));
				multi_result["success"] = false;
				return multi_result;
			}
			
			if (effect.int3 != EXCESS_TARGET_DROP && effect.int3 != EXCESS_TARGET_STACK) {
				UtilityFunctions::push_error(vformat("Invalid excess_handling opcode %d for multi_target effect", effect.int3));
				multi_result["success"] = false;
				return multi_result;
			}
			
			// Validate sub_effects
			if (effect.sub_effects.empty()) {
				UtilityFunctions::push_error("No sub_effects provided for multi_target effect");
				multi_result["success"] = false;
				return multi_result;
			}
			
			// Validate team_filter is explicitly provided
			if (effect.team_filter.is_empty()) {
				UtilityFunctions::push_error("team_filter is required for multi_target effect, must be 'ally' or 'enemy'");
				multi_result["success"] = false;
				return multi_result;
			}
			
			// Warn about sub-effects with target_self=true
			for (const EffectRecord &sub_effect : effect.sub_effects) {
				if (sub_effect.int0 == 1) {
					UtilityFunctions::push_warning("Sub-effect has target_self=true in multi_target effect, which may override selected targets");
				}
			}
			
			// Select targets and run applications (may defer in-flight projectile sub-effects).
			bool any_success = false;
			return run_multi_target_impl(
					effect,
					context,
					world,
					host,
					hooks,
					match_host,
					source,
					target,
					0,
					0,
					0,
					Dictionary(),
					Dictionary(),
					nullptr,
					any_success);
		}
		case ExecRoute::DefaultOk:
		default: {
			Dictionary default_result;
			default_result["success"] = true;
			return default_result;
		}
	}
}

} // namespace internal

Dictionary execute(
		const EffectRecord &effect,
		EffectContext &context,
		SimWorld &world,
		SimHostCallbacks &host,
		const SimExecCallbacks &hooks,
		::sim::effects::SimMatchHost match_host) {
	return internal::execute_impl(effect, context, world, host, hooks, match_host);
}

void resume_deferred_multi_effect(SimWorld &world, SimHostCallbacks &host, UnitState &source) {
	internal::resume_deferred_multi_effect(world, host, source);
}

void resume_deferred_multi_target(SimWorld &world, SimHostCallbacks &host, UnitState &source) {
	internal::resume_deferred_multi_target(world, host, source);
}

void try_complete_deferred_projectile_chains(SimWorld &world, SimHostCallbacks &host, UnitState &source) {
	internal::try_complete_deferred_projectile_chains(world, host, source);
}

void abandon_deferred_projectile(SimWorld &world, SimHostCallbacks &host, const ProjectileState &projectile) {
	internal::abandon_deferred_projectile(world, host, projectile);
}

} // namespace sim::effects::execution
