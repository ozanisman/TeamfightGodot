#include "teamfight_simulation_test_runner.hpp"

#include "simulation/sim_combat.hpp"
#include "simulation/sim_damage.hpp"
#include "simulation/sim_effects_compile.hpp"
#include "simulation/sim_effects_exec.hpp"
#include "simulation/sim_effects_host.hpp"
#include "simulation/sim_stats.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <array>
#include <algorithm>
#include <iterator>
#include <unordered_map>
#include <vector>

using namespace godot;

namespace {

constexpr double TEST_EPSILON = 1e-6;

Dictionary effect(const char *kind, const Dictionary &params = Dictionary()) {
	Dictionary value;
	value["kind"] = StringName(kind);
	value["params"] = params;
	return value;
}

Dictionary damage_effect(double amount, bool target_self = false) {
	Dictionary params;
	params["flat_amount"] = amount;
	params["damage_type"] = "true";
	params["target_self"] = target_self;
	params["reason"] = "Native test damage";
	return effect("damage", params);
}

Dictionary projectile_effect(double amount) {
	Dictionary params;
	params["on_hit"] = damage_effect(amount);
	params["reason"] = "Native test projectile";
	return effect("projectile", params);
}

Dictionary damage_based_heal_effect(double ratio) {
	Dictionary params;
	params["damage_ratio"] = ratio;
	params["reason"] = "Native test damage heal";
	return effect("damage_based_heal", params);
}

Dictionary damage_based_shield_effect(double ratio) {
	Dictionary params;
	params["damage_ratio"] = ratio;
	params["reason"] = "Native test damage shield";
	return effect("damage_based_shield", params);
}

Dictionary stat_additive_effect(const char *stat_name, double additive, bool target_self, const char *reason) {
	Dictionary params;
	params["stat_name"] = stat_name;
	params["additive"] = additive;
	params["duration"] = 8.0;
	params["target_self"] = target_self;
	params["reason"] = reason;
	return effect("stat_modifier", params);
}

Dictionary constant_multiplier_effect(double multiplier) {
	Dictionary params;
	params["multiplier"] = multiplier;
	return effect("constant_multiplier", params);
}

Dictionary redirect_damage_effect(double redirect_ratio, double reduction_ratio, double redirect_cap = 0.0) {
	Dictionary params;
	params["redirect_ratio"] = redirect_ratio;
	params["reduction_ratio"] = reduction_ratio;
	params["redirect_cap"] = redirect_cap;
	params["reason"] = "Native test redirect";
	return effect("redirect_damage", params);
}

Dictionary require_result(Dictionary effect_dict, const char *from, const char *field, bool value) {
	Dictionary params = effect_dict["params"];
	params["requires_result_from"] = from;
	params["requires_field"] = field;
	params["requires_value"] = value;
	effect_dict["params"] = params;
	return effect_dict;
}

Dictionary multi_target_effect(const Array &sub_effects) {
	Dictionary params;
	params["target_count"] = 1;
	params["selection_strategy"] = "closest";
	params["team_filter"] = "enemy";
	params["sub_effects"] = sub_effects;
	params["reason"] = "Native test multi target";
	return effect("multi_target", params);
}

Dictionary multi_effect(const Array &effects) {
	Dictionary params;
	params["effects"] = effects;
	params["reason"] = "Native test multi effect";
	return effect("multi_effect", params);
}

bool expect(bool condition, const String &message, String &failure) {
	if (condition) {
		return true;
	}
	failure = message;
	return false;
}

bool expect_close(double actual, double expected, const String &label, String &failure) {
	return expect(
			Math::abs(actual - expected) <= TEST_EPSILON,
			vformat("%s: expected %.6f, got %.6f", label, expected, actual),
			failure);
}

struct TestWorld {
	std::vector<sim::UnitState> units;
	std::vector<sim::UnitStateCold> cold;
	std::vector<sim::UnitStateRare> rare;
	std::unordered_map<int64_t, int64_t> unit_index_map;
	std::vector<sim::TargetingFrameEntry> targeting_frame;
	sim::TickContext tick_context;
	std::vector<int64_t> alive_player_indices;
	std::vector<int64_t> alive_enemy_indices;
	double time = 0.0;
	double tick_rate = 0.05;
	std::array<std::vector<int64_t>, sim::SPATIAL_GRID_DIM * sim::SPATIAL_GRID_DIM> spatial_buckets;
	std::vector<uint32_t> spatial_stamp;
	uint32_t spatial_generation = 1;
	sim::SpatialBucketFillCache spatial_fill_cache;
	std::vector<sim::PendingSpawn> pending_spawns;
	std::vector<sim::ProjectileState> projectiles;
	int64_t max_instance_id = 0;
	int64_t next_projectile_id = 1;
	sim::effects::execution::SimExecCallbacks exec_callbacks;
	sim::effects::EffectExecBindings bindings;
	sim::SimHostCallbacks host;

	TestWorld() {
		units.reserve(8);
		cold.reserve(8);
		rare.reserve(8);
		bindings.units = &units;
		bindings.unit_cold = &cold;
		bindings.unit_rare = &rare;
		bindings.unit_index_map = &unit_index_map;
		bindings.targeting_frame = &targeting_frame;
		bindings.tick_ctx = &tick_context;
		bindings.alive_player_indices = &alive_player_indices;
		bindings.alive_enemy_indices = &alive_enemy_indices;
		bindings.time = &time;
		bindings.tick_rate = &tick_rate;
		bindings.spatial_buckets = &spatial_buckets;
		bindings.spatial_stamp = &spatial_stamp;
		bindings.spatial_generation = &spatial_generation;
		bindings.spatial_fill_cache = &spatial_fill_cache;
		bindings.match_host.pending_spawns = &pending_spawns;
		bindings.match_host.projectiles = &projectiles;
		bindings.match_host.max_instance_id = &max_instance_id;
		bindings.exec_callbacks = &exec_callbacks;
		host.effect_exec = &bindings;
		host.next_projectile_id = &next_projectile_id;
		host.execute_effect = &sim::effects::host_execute_effect;
		host.handle_death = &handle_death;
	}

	static void handle_death(void *, sim::UnitState &, sim::UnitState &target) {
		target.alive = false;
	}

	int64_t add_unit(int64_t instance_id, const StringName &team, double hp = 500.0) {
		sim::UnitState unit;
		unit.instance_id = instance_id;
		unit.team = team;
		unit.combat.max_hp = 500.0;
		unit.combat.attack_damage = 20.0;
		unit.combat.armor = 0.0;
		unit.combat.ward = 0.0;
		unit.combat.projectile_speed = 10.0;
		unit.combat.projectile_radius = 0.0;
		unit.cached_max_hp = unit.combat.max_hp;
		unit.cached_attack_damage = unit.combat.attack_damage;
		unit.cached_armor = unit.combat.armor;
		unit.cached_ward = unit.combat.ward;
		unit.cached_projectile_speed = unit.combat.projectile_speed;
		unit.cached_projectile_radius = unit.combat.projectile_radius;
		unit.stats_dirty = false;
		unit.hp = hp;
		unit.alive = true;
		const int64_t index = static_cast<int64_t>(units.size());
		units.push_back(unit);
		cold.emplace_back();
		rare.emplace_back();
		unit_index_map[instance_id] = index;
		if (team == StringName("player")) {
			alive_player_indices.push_back(index);
		} else {
			alive_enemy_indices.push_back(index);
		}
		max_instance_id = std::max(max_instance_id, instance_id);
		return index;
	}

	int64_t add_player(int64_t instance_id, double hp = 500.0) {
		return add_unit(instance_id, StringName("player"), hp);
	}

	int64_t add_enemy(int64_t instance_id, double hp = 500.0) {
		return add_unit(instance_id, StringName("enemy"), hp);
	}

	sim::UnitState &unit_at(int64_t index) {
		return units[static_cast<size_t>(index)];
	}

	void add_passive(int64_t unit_index, size_t bucket, const Dictionary &effect_dict) {
		sim::SimWorld sim_world = world();
		sim::uc(sim_world, unit_at(unit_index)).passive_effects[bucket].push_back(
				sim::effects::compile::compile_effect(effect_dict));
	}

	sim::SimWorld world() {
		return bindings.make_world();
	}

	Dictionary execute(const sim::EffectRecord &record, sim::EffectContext &context) {
		return sim::effects::host_execute_effect(host, record, context);
	}
};

bool test_deferred_projectile_heal(String &failure) {
	TestWorld test;
	const int64_t source_index = test.add_unit(1, StringName("player"), 200.0);
	const int64_t target_index = test.add_unit(2, StringName("enemy"));
	sim::UnitState &source = test.unit_at(source_index);
	sim::UnitState &target = test.unit_at(target_index);
	Array sub_effects;
	sub_effects.append(projectile_effect(60.0));
	sub_effects.append(damage_based_heal_effect(0.5));
	const sim::EffectRecord record = sim::effects::compile::compile_effect(multi_target_effect(sub_effects));
	sim::EffectContext context = sim::combat::build_context(source, &target, nullptr, 0.0, StringName("ability"));
	test.execute(record, context);
	sim::SimWorld world = test.world();
	if (!expect(test.projectiles.size() == 1, "deferred heal: projectile was not created", failure) ||
			!expect(sim::uc(world, source).deferred_effect_outstanding_projectiles == 1, "deferred heal: outstanding count was not set", failure) ||
			!expect_close(source.hp, 200.0, "deferred heal before impact", failure)) {
		return false;
	}
	sim::combat::CombatHostHooks hooks;
	sim::combat::resolve_projectile(world, test.host, hooks, test.projectiles.front());
	return expect_close(target.hp, 440.0, "deferred heal target hp", failure) &&
			expect_close(source.hp, 230.0, "deferred heal source hp", failure) &&
			expect(sim::uc(world, source).deferred_effect_outstanding_projectiles == 0, "deferred heal: outstanding count did not clear", failure) &&
			expect(!sim::uc(world, source).deferred_multi_target_active, "deferred heal: continuation remained active", failure);
}

bool test_deferred_multi_effect_sibling(String &failure) {
	TestWorld test;
	const int64_t source_index = test.add_unit(1, StringName("player"), 200.0);
	const int64_t target_index = test.add_unit(2, StringName("enemy"));
	sim::UnitState &source = test.unit_at(source_index);
	sim::UnitState &target = test.unit_at(target_index);
	Array sub_effects;
	sub_effects.append(projectile_effect(60.0));
	sub_effects.append(damage_based_heal_effect(0.5));
	Array effects;
	effects.append(multi_target_effect(sub_effects));
	effects.append(damage_effect(10.0, true));
	const sim::EffectRecord record = sim::effects::compile::compile_effect(multi_effect(effects));
	sim::EffectContext context = sim::combat::build_context(source, &target, nullptr, 0.0, StringName("ultimate"));
	test.execute(record, context);
	sim::SimWorld world = test.world();
	if (!expect(sim::uc(world, source).deferred_multi_effect_active, "deferred multi effect: continuation was not saved", failure) ||
			!expect_close(source.hp, 200.0, "deferred multi effect before impact", failure)) {
		return false;
	}
	sim::combat::CombatHostHooks hooks;
	sim::combat::resolve_projectile(world, test.host, hooks, test.projectiles.front());
	return expect_close(target.hp, 440.0, "deferred multi effect target hp", failure) &&
			expect_close(source.hp, 220.0, "deferred multi effect source hp", failure) &&
			expect(!sim::uc(world, source).deferred_multi_effect_active, "deferred multi effect: continuation did not clear", failure);
}

bool test_knockback_mid_chain_merge(String &failure) {
	TestWorld test;
	const int64_t source_index = test.add_unit(1, StringName("player"));
	const int64_t target_index = test.add_unit(2, StringName("enemy"));
	sim::UnitState &source = test.unit_at(source_index);
	sim::UnitState &target = test.unit_at(target_index);
	target.pos_x = 1.0;
	sim::SimWorld world = test.world();
	sim::uc(world, source).passive_effects[sim::EFFECT_BUCKET_ON_KNOCKBACK].push_back(
			sim::effects::compile::compile_effect(damage_effect(25.0)));
	Dictionary knockback_params;
	knockback_params["distance"] = 1.0;
	knockback_params["direction"] = "away_from_source";
	Dictionary shield_params;
	shield_params["damage_ratio"] = 1.0;
	Array effects;
	effects.append(damage_effect(40.0));
	effects.append(effect("knockback", knockback_params));
	effects.append(effect("damage_based_shield", shield_params));
	const sim::EffectRecord record = sim::effects::compile::compile_effect(multi_effect(effects));
	sim::EffectContext context = sim::combat::build_context(source, &target, nullptr, 0.0, StringName("ability"));
	test.execute(record, context);
	return expect_close(target.hp, 435.0, "knockback merge target hp", failure) &&
			expect_close(source.shield, 65.0, "knockback merge shield", failure) &&
			expect_close(context.damage, 65.0, "knockback merge context damage", failure);
}

bool test_post_attack_result_chain(String &failure) {
	TestWorld test;
	const int64_t source_index = test.add_unit(1, StringName("player"), 200.0);
	const int64_t target_index = test.add_unit(2, StringName("enemy"));
	sim::UnitState &source = test.unit_at(source_index);
	sim::UnitState &target = test.unit_at(target_index);
	sim::SimWorld world = test.world();
	auto &bucket = sim::uc(world, source).passive_effects[sim::EFFECT_BUCKET_POST_ATTACK];
	bucket.push_back(sim::effects::compile::compile_effect(damage_effect(12.0)));
	Dictionary heal_params;
	heal_params["damage_ratio"] = 0.5;
	heal_params["requires_result_from"] = "damage";
	heal_params["requires_field"] = "success";
	heal_params["requires_value"] = true;
	bucket.push_back(sim::effects::compile::compile_effect(effect("damage_based_heal", heal_params)));
	sim::EffectContext context = sim::combat::build_context(source, &target, nullptr, 20.0, StringName("auto"));
	sim::combat::run_post_attack_effects(world, test.host, source, target, 20.0, context);
	return expect_close(target.hp, 488.0, "post attack target hp", failure) &&
			expect_close(source.hp, 216.0, "post attack source hp", failure) &&
			expect_close(context.damage, 32.0, "post attack merged damage", failure) &&
			expect(context.accumulated_results.has("damage"), "post attack result was not merged", failure);
}

bool test_knockback_action_terminal(String &failure) {
	TestWorld test;
	const int64_t source_index = test.add_unit(1, StringName("player"));
	const int64_t target_index = test.add_unit(2, StringName("enemy"));
	sim::UnitState &source = test.unit_at(source_index);
	sim::UnitState &target = test.unit_at(target_index);
	target.pos_x = 1.0;
	sim::SimWorld world = test.world();
	Dictionary shield_params;
	shield_params["max_hp_ratio"] = 0.15;
	shield_params["target_self"] = true;
	sim::uc(world, source).passive_effects[sim::EFFECT_BUCKET_ON_KNOCKBACK_ACTION].push_back(
			sim::effects::compile::compile_effect(effect("shield", shield_params)));
	Dictionary knockback_params;
	knockback_params["distance"] = 1.0;
	knockback_params["direction"] = "away_from_source";
	sim::EffectContext context = sim::combat::build_context(source, &target, nullptr, 0.0, StringName("ability"));
	test.execute(sim::effects::compile::compile_effect(effect("knockback", knockback_params)), context);
	if (!expect(context.knockback_applied, "knockback action: knockback was not recorded", failure)) {
		return false;
	}
	sim::combat::run_on_knockback_action_effects(world, test.host, source, &target, context);
	return expect_close(source.shield, 75.0, "knockback action shield", failure) &&
			expect_close(context.damage, 0.0, "knockback action parent damage", failure);
}

Dictionary stat_modifier_effect(const char *stat_name, double heal_ratio, const char *reason) {
	Dictionary params;
	params["stat_name"] = stat_name;
	params["heal_gained_ratio"] = heal_ratio;
	params["duration"] = 8.0;
	params["target_self"] = true;
	params["reason"] = reason;
	return effect("stat_modifier", params);
}

bool test_heal_accumulation_and_post_heal_chain(String &failure) {
	{
		TestWorld test;
		const int64_t source_index = test.add_unit(1, StringName("player"), 400.0);
		sim::UnitState &source = test.unit_at(source_index);
		Array effects;
		Dictionary heal_params;
		heal_params["flat_amount"] = 18.0;
		heal_params["target_self"] = true;
		effects.append(effect("heal", heal_params));
		effects.append(stat_modifier_effect("attack_damage", 3.0, "Cumulative heal scale"));
		const sim::EffectRecord record = sim::effects::compile::compile_effect(multi_effect(effects));
		sim::EffectContext context = sim::combat::build_context(source, &source, nullptr, 0.0, StringName("ability"));
		test.execute(record, context);
		if (!expect_close(source.hp, 418.0, "heal accumulation hp", failure) ||
				!expect_close(source.stat_additive_attack_damage, 54.0, "heal accumulation modifier", failure) ||
				!expect_close(context.heal_gained, 18.0, "heal accumulation context", failure)) {
			return false;
		}
	}
	{
		TestWorld test;
		const int64_t source_index = test.add_unit(1, StringName("player"), 400.0);
		sim::UnitState &source = test.unit_at(source_index);
		sim::SimWorld world = test.world();
		auto &bucket = sim::uc(world, source).passive_effects[sim::EFFECT_BUCKET_POST_HEAL];
		bucket.push_back(sim::effects::compile::compile_effect(stat_modifier_effect("attack_damage", 2.0, "Post heal attack")));
		Dictionary chained = stat_modifier_effect("max_hp", 4.0, "Post heal hp");
		Dictionary chained_params = chained["params"];
		chained_params["requires_result_from"] = "stat_modifier";
		chained_params["requires_field"] = "stat_modifier_applied";
		chained_params["requires_value"] = true;
		chained["params"] = chained_params;
		bucket.push_back(sim::effects::compile::compile_effect(chained));
		sim::EffectContext context = sim::combat::build_context(source, &source, nullptr, 0.0, StringName("ability"));
		sim::combat::run_post_heal_effects(world, test.host, source, source, 10.0, 10.0, context);
		if (!expect_close(source.stat_additive_attack_damage, 20.0, "post heal attack modifier", failure) ||
				!expect_close(source.stat_additive_max_hp, 40.0, "post heal hp modifier", failure)) {
			return false;
		}
	}
	return true;
}

bool test_post_heal_inherits_action_chain(String &failure) {
	TestWorld test;
	const int64_t source_index = test.add_unit(1, StringName("player"), 400.0);
	sim::UnitState &source = test.unit_at(source_index);
	sim::SimWorld world = test.world();
	Dictionary modifier = stat_modifier_effect("armor", 1.0, "Post heal inherited condition");
	Dictionary modifier_params = modifier["params"];
	modifier_params["requires_result_from"] = "damage";
	modifier_params["requires_field"] = "success";
	modifier_params["requires_value"] = true;
	modifier["params"] = modifier_params;
	sim::uc(world, source).passive_effects[sim::EFFECT_BUCKET_POST_HEAL].push_back(
			sim::effects::compile::compile_effect(modifier));
	sim::EffectContext context = sim::combat::build_context(source, &source, nullptr, 24.0, StringName("ability"));
	Dictionary damage_result;
	damage_result["success"] = true;
	context.accumulated_results[StringName("damage")] = damage_result;
	context.knockback_applied = true;
	sim::combat::run_post_heal_effects(world, test.host, source, source, 10.0, 10.0, context);
	return expect_close(source.stat_additive_armor, 10.0, "post heal inherited condition modifier", failure) &&
			expect_close(context.damage, 24.0, "post heal should not merge damage into parent", failure) &&
			expect(context.knockback_applied, "post heal should not mutate parent knockback state", failure);
}

bool test_post_take_damage_contract(String &failure) {
	TestWorld test;
	const int64_t attacker_index = test.add_enemy(1);
	const int64_t target_index = test.add_player(2, 400.0);
	sim::UnitState &attacker = test.unit_at(attacker_index);
	sim::UnitState &target = test.unit_at(target_index);
	target.stealth_remaining = 4.0;
	target.stealth_break_on_damage_taken = true;
	Array effects;
	effects.append(damage_based_heal_effect(0.5));
	effects.append(stat_additive_effect("attack_damage", 7.0, false, "Post damage target must stay null"));
	test.add_passive(target_index, sim::EFFECT_BUCKET_POST_TAKE_DAMAGE, multi_effect(effects));
	sim::SimWorld world = test.world();
	sim::EffectContext context = sim::combat::build_context(attacker, &target, nullptr, 0.0, StringName("ability"));
	sim::damage::apply_damage(world, test.host, attacker, target, 40.0, StringName("true"), StringName("ability"), context);
	return expect_close(target.hp, 380.0, "post take damage self heal uses damage payload", failure) &&
			expect_close(attacker.stat_additive_attack_damage, 0.0, "post take damage target should be null", failure) &&
			expect_close(target.stat_additive_attack_damage, 0.0, "post take damage target should not be damaged unit", failure) &&
			expect_close(target.stealth_remaining, 0.0, "post take damage stealth break with bucket", failure) &&
			expect(!target.stealth_break_on_damage_taken, "post take damage stealth flag remained set", failure);
}

bool test_on_takedown_contract(String &failure) {
	TestWorld test;
	const int64_t participant_index = test.add_player(1);
	const int64_t victim_index = test.add_enemy(2);
	sim::UnitState &participant = test.unit_at(participant_index);
	sim::UnitState &victim = test.unit_at(victim_index);
	test.add_passive(participant_index, sim::EFFECT_BUCKET_ON_TAKEDOWN,
			require_result(stat_additive_effect("attack_damage", 4.0, true, "Takedown inherited result"), "damage", "success", true));
	test.add_passive(participant_index, sim::EFFECT_BUCKET_ON_TAKEDOWN, damage_effect(7.0));
	test.add_passive(participant_index, sim::EFFECT_BUCKET_ON_TAKEDOWN, damage_based_shield_effect(1.0));
	sim::SimWorld world = test.world();
	sim::EffectContext context = sim::combat::build_context(participant, &victim, nullptr, 30.0, StringName("takedown"));
	Dictionary inherited_damage_result;
	inherited_damage_result["success"] = true;
	context.accumulated_results[StringName("damage")] = inherited_damage_result;
	sim::combat::run_on_takedown_effects(world, test.host, participant, victim, 30.0, true, context);
	return expect_close(participant.stat_additive_attack_damage, 4.0, "takedown inherited result modifier", failure) &&
			expect_close(victim.hp, 493.0, "takedown victim target", failure) &&
			expect_close(participant.shield, 37.0, "takedown damage payload chain", failure) &&
			expect_close(context.damage, 30.0, "takedown should not merge damage into parent", failure) &&
			expect(!context.accumulated_results.has("damage_based_shield"), "takedown should not merge new results into parent", failure);
}

bool test_on_ally_defense_contract(String &failure) {
	TestWorld test;
	const int64_t attacker_index = test.add_enemy(1);
	const int64_t target_index = test.add_player(2);
	const int64_t protector_index = test.add_player(3);
	const int64_t far_protector_index = test.add_player(4);
	sim::UnitState &attacker = test.unit_at(attacker_index);
	sim::UnitState &target = test.unit_at(target_index);
	sim::UnitState &protector = test.unit_at(protector_index);
	sim::UnitState &far_protector = test.unit_at(far_protector_index);
	target.pos_x = 0.0;
	target.pos_y = 0.0;
	protector.pos_x = 1.0;
	protector.pos_y = 0.0;
	far_protector.pos_x = 10.0;
	far_protector.pos_y = 0.0;
	test.add_passive(protector_index, sim::EFFECT_BUCKET_ON_ALLY_DEFENSE, redirect_damage_effect(0.25, 0.20));
	test.add_passive(protector_index, sim::EFFECT_BUCKET_ON_ALLY_DEFENSE, damage_effect(10.0, true));
	test.add_passive(protector_index, sim::EFFECT_BUCKET_ON_ABILITY, constant_multiplier_effect(2.0));
	test.add_passive(far_protector_index, sim::EFFECT_BUCKET_ON_ALLY_DEFENSE, redirect_damage_effect(0.50, 0.0));
	sim::SimWorld world = test.world();
	sim::uc(world, protector).on_ally_defense_radius = 2.0;
	sim::uc(world, far_protector).on_ally_defense_radius = 1.0;
	sim::PassiveReflectEntry reflect;
	reflect.percentage = 1.0;
	reflect.damage_type = StringName("true");
	reflect.action_kind = StringName("passive");
	sim::uc(world, protector).passive_reflect_entries.push_back(reflect);
	sim::EffectContext context = sim::combat::build_context(attacker, &target, nullptr, 100.0, StringName("ability"));
	sim::damage::apply_damage(world, test.host, attacker, target, 100.0, StringName("true"), StringName("ability"), context);
	return expect_close(target.hp, 425.0, "ally defense redirected target damage", failure) &&
			expect_close(protector.hp, 460.0, "ally defense redirect and preserved ability action", failure) &&
			expect_close(attacker.hp, 500.0, "ally defense redirected damage should suppress reflect", failure) &&
			expect_close(far_protector.hp, 500.0, "ally defense radius gate", failure);
}

bool test_post_attack_multiple_result_merge(String &failure) {
	TestWorld test;
	const int64_t source_index = test.add_player(1, 200.0);
	const int64_t target_index = test.add_enemy(2);
	sim::UnitState &source = test.unit_at(source_index);
	sim::UnitState &target = test.unit_at(target_index);
	test.add_passive(source_index, sim::EFFECT_BUCKET_POST_ATTACK,
			require_result(stat_additive_effect("armor", 3.0, true, "Post attack inherited result"), "damage", "success", true));
	test.add_passive(source_index, sim::EFFECT_BUCKET_POST_ATTACK, damage_effect(8.0));
	test.add_passive(source_index, sim::EFFECT_BUCKET_POST_ATTACK,
			require_result(damage_based_shield_effect(0.5), "damage", "success", true));
	sim::SimWorld world = test.world();
	sim::EffectContext context = sim::combat::build_context(source, &target, nullptr, 20.0, StringName("auto"));
	Dictionary inherited_damage_result;
	inherited_damage_result["success"] = true;
	context.accumulated_results[StringName("damage")] = inherited_damage_result;
	sim::combat::run_post_attack_effects(world, test.host, source, target, 20.0, context);
	return expect_close(source.stat_additive_armor, 3.0, "post attack inherited result modifier", failure) &&
			expect_close(target.hp, 492.0, "post attack chained damage", failure) &&
			expect_close(source.shield, 14.0, "post attack ordered damage result", failure) &&
			expect_close(context.damage, 28.0, "post attack merged damage delta", failure) &&
			expect(context.accumulated_results.has("stat_modifier"), "post attack stat result missing", failure) &&
			expect(context.accumulated_results.has("damage_based_shield"), "post attack shield result missing", failure);
}

bool test_stealth_break_without_post_damage_bucket(String &failure) {
	TestWorld test;
	const int64_t source_index = test.add_unit(1, StringName("player"));
	const int64_t target_index = test.add_unit(2, StringName("enemy"));
	sim::UnitState &source = test.unit_at(source_index);
	sim::UnitState &target = test.unit_at(target_index);
	target.stealth_remaining = 4.0;
	target.stealth_break_on_damage_taken = true;
	sim::SimWorld world = test.world();
	sim::EffectContext context = sim::combat::build_context(source, &target, nullptr, 0.0, StringName("ability"));
	sim::damage::apply_damage(world, test.host, source, target, 10.0, StringName("true"), StringName("ability"), context);
	return expect_close(target.stealth_remaining, 0.0, "stealth break duration", failure) &&
			expect(!target.stealth_break_on_damage_taken, "stealth break flag remained set", failure) &&
			expect(sim::uc(world, target).passive_effects[sim::EFFECT_BUCKET_POST_TAKE_DAMAGE].empty(), "stealth break test unexpectedly has a post-damage effect", failure);
}

using TestFunction = bool (*)(String &failure);

struct TestEntry {
	const char *name;
	TestFunction function;
};

} // namespace

void TeamfightSimulationTestRunner::_bind_methods() {
	ClassDB::bind_method(D_METHOD("run_all"), &TeamfightSimulationTestRunner::run_all);
}

Dictionary TeamfightSimulationTestRunner::run_all() {
	const TestEntry tests[] = {
		{ "deferred_projectile_heal", &test_deferred_projectile_heal },
		{ "deferred_multi_effect_sibling", &test_deferred_multi_effect_sibling },
		{ "knockback_mid_chain_merge", &test_knockback_mid_chain_merge },
		{ "post_attack_result_chain", &test_post_attack_result_chain },
		{ "knockback_action_terminal", &test_knockback_action_terminal },
		{ "heal_accumulation_and_post_heal_chain", &test_heal_accumulation_and_post_heal_chain },
		{ "post_heal_inherits_action_chain", &test_post_heal_inherits_action_chain },
		{ "post_take_damage_contract", &test_post_take_damage_contract },
		{ "on_takedown_contract", &test_on_takedown_contract },
		{ "on_ally_defense_contract", &test_on_ally_defense_contract },
		{ "post_attack_multiple_result_merge", &test_post_attack_multiple_result_merge },
		{ "stealth_break_without_post_damage_bucket", &test_stealth_break_without_post_damage_bucket },
	};

	Array failures;
	int64_t passed = 0;
	for (const TestEntry &test : tests) {
		String failure;
		if (test.function(failure)) {
			passed += 1;
		} else {
			failures.append(vformat("%s: %s", test.name, failure));
		}
	}

	Dictionary result;
	result["total"] = static_cast<int64_t>(std::size(tests));
	result["passed"] = passed;
	result["failures"] = failures;
	return result;
}
