#ifndef TEAMFIGHT_SIMULATION_CORE_HPP
#define TEAMFIGHT_SIMULATION_CORE_HPP

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <random>

using namespace godot;

class TeamfightSimulationCore : public RefCounted {
	GDCLASS(TeamfightSimulationCore, RefCounted);

protected:
	static void _bind_methods();

private:
	enum EffectOpcode : int64_t {
		EFFECT_OPCODE_UNKNOWN = 0,
		EFFECT_OPCODE_MULTI = 1,
		EFFECT_OPCODE_DAMAGE = 2,
		EFFECT_OPCODE_PROJECTILE = 3,
		EFFECT_OPCODE_STUN = 4,
		EFFECT_OPCODE_SHIELD = 5,
		EFFECT_OPCODE_HEAL = 6,
		EFFECT_OPCODE_SELF_DAMAGE = 7,
		EFFECT_OPCODE_SELF_SHIELD = 8,
		EFFECT_OPCODE_SELF_AOE_TAUNT = 9,
		EFFECT_OPCODE_SELF_AOE_DAMAGE = 10,
		EFFECT_OPCODE_SPLASH_DAMAGE = 11,
		EFFECT_OPCODE_THRESHOLD_SPLASH_DAMAGE = 12,
		EFFECT_OPCODE_MANA_REGEN = 13,
		EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN = 14,
		EFFECT_OPCODE_DAMAGE_BASED_HEAL = 15,
		EFFECT_OPCODE_MANA_RESTORE_ON_HIT = 16,
		EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT = 17,
		EFFECT_OPCODE_EVERY_N_ATTACKS_STUN = 18,
		EFFECT_OPCODE_DODGE = 19,
		EFFECT_OPCODE_CONSTANT_MULTIPLIER = 20,
		EFFECT_OPCODE_TARGET_HP_THRESHOLD_MULTIPLIER = 21,
		EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER = 22,
		EFFECT_OPCODE_SELF_HP_THRESHOLD_MULTIPLIER = 23,
	};

	static constexpr double MATCH_DURATION = 60.0;
	static constexpr double DEFAULT_TICK_RATE = 0.1;
	static constexpr double EPSILON = 0.000001;
	static constexpr double RESPAWN_TIME = 10.0;
	static constexpr double ASSIST_WINDOW = 5.0;
	static constexpr double RETARGET_INTERVAL = 0.5;
	static constexpr double TARGET_SWITCH_LOCK_DURATION = 0.3;
	static constexpr double REGEN_TICK_INTERVAL = 1.0;
	static constexpr double CASTING_WINDUP = 0.5;
	static constexpr double RANGED_THRESHOLD = 1.0;
	static constexpr double MELEE_CONTACT_BUFFER = 0.00001;
	static constexpr double DEFAULT_PROJECTILE_SPEED = 5.0;
	static constexpr double DEFAULT_PROJECTILE_RADIUS = 0.03;
	static constexpr double DEFAULT_PROJECTILE_STUN_DURATION = 0.0;
	static constexpr int64_t SIMULATION_RULES_VERSION = 1;
	static constexpr double WORLD_SIZE = 10.0;
	static constexpr double DRAFT_X_BASE = 0.9;
	static constexpr double ALLY_CRITICAL_HP_RATIO = 0.35;
	static constexpr double ROLE_PRIORITY_GLOBAL_SCALE = 0.85;
	static constexpr double SCORE_HP_WEIGHT_SCALE = 10.0;
	static constexpr double SCORE_THREAT_WEIGHT_SCALE = 5.0;
	static constexpr double SCORE_DISTANCE_WEIGHT_SCALE = 3.0;
	static constexpr double SCORE_KITING_WEIGHT_SCALE = 1.5;
	static constexpr double DISTANCE_EXPONENT = 1.5;
	static constexpr double THREAT_RESPONSE_RANGE_FALLOFF = 0.6;
	static constexpr double THREAT_BURST_THRESHOLD = 0.1;
	static constexpr double THREAT_BURST_MULTIPLIER = 5.0;
	static constexpr double THREAT_DECAY_DEFAULT = 2.0;
	static constexpr double THREAT_DECAY_TANK = 4.0;
	static constexpr double THREAT_DECAY_FIGHTER = 4.5;
	static constexpr double THREAT_DECAY_FRAGILE = 1.0;
	static constexpr double TARGET_SWITCH_MARGIN = 0.75;
	static constexpr double STICKINESS_DEFAULT = 2.0;
	static constexpr double STICKINESS_MARKSMAN = 5.0;
	static constexpr double STICKINESS_SUPPORT = 1.0;
	static constexpr double THREAT_RESPONSE_TANK = 2.0;
	static constexpr double THREAT_RESPONSE_FIGHTER = 1.8;
	static constexpr double THREAT_RESPONSE_ASSASSIN = 0.8;
	static constexpr double THREAT_RESPONSE_MARKSMAN = 1.0;
	static constexpr double THREAT_RESPONSE_MAGE = 1.1;
	static constexpr double THREAT_RESPONSE_SUPPORT = 1.7;
	static constexpr double HP_WEIGHT_ASSASSIN = 2.0;
	static constexpr double HP_WEIGHT_MAGE = 2.5;
	static constexpr double HP_WEIGHT_SUPPORT = 5.0;
	static constexpr double DISTANCE_WEIGHT_TANK = 1.5;
	static constexpr double DISTANCE_WEIGHT_ASSASSIN = 0.15;
	static constexpr double DISTANCE_WEIGHT_MAGE = 1.2;
	static constexpr double DISTANCE_WEIGHT_SUPPORT = 1.5;
	static constexpr double DISTANCE_WEIGHT_FIGHTER_CLOSE = 3.0;
	static constexpr double IN_RANGE_BONUS_TANK = 1.0;
	static constexpr double IN_RANGE_BONUS_FIGHTER = 0.9;
	static constexpr double IN_RANGE_BONUS_ASSASSIN = 0.6;
	static constexpr double IN_RANGE_BONUS_MARKSMAN = 0.35;
	static constexpr double IN_RANGE_BONUS_MAGE = 0.45;
	static constexpr double IN_RANGE_BONUS_SUPPORT = 0.4;
	static constexpr double TANK_PENALTY_TANK = 0.4;
	static constexpr double TANK_PENALTY_FIGHTER = 1.0;
	static constexpr double TANK_PENALTY_ASSASSIN = 7.0;
	static constexpr double TANK_PENALTY_MARKSMAN = 2.6;
	static constexpr double TANK_PENALTY_MAGE = 2.3;
	static constexpr double TANK_PENALTY_SUPPORT = 1.5;
	static constexpr double ASSASSIN_TANK_CONTEXT_PENALTY = 8.0;
	static constexpr double EXECUTE_BONUS_WEIGHT_DEFAULT = 50.0;
	static constexpr double PREY_INCOMING_TARGET_SCALE = 0.75;
	static constexpr double PREY_PERCEIVED_THREAT_SCALE = 0.35;
	static constexpr double PREY_FRONTLINE_SCALE = 0.35;
	static constexpr double SUPPORT_PEEL_BOOST = 2.2;
	static constexpr double SUPPORT_PEEL_THREAT_THRESHOLD = 2.0;
	static constexpr double TARGET_EXECUTE_HP_RATIO = 0.25;
	static constexpr double BODYGUARD_RADIUS = 2.5;
	static constexpr double BODYGUARD_WEIGHT_TANK = 3.0;
	static constexpr double BODYGUARD_WEIGHT_SUPPORT = 4.0;
	static constexpr double IN_RANGE_BONUS_DEFAULT = 0.6;
	static constexpr double THREAT_RESPONSE_RANGE_DEFAULT = 0.0;
	static constexpr double PROJECTILE_TIME_WEIGHT_MARKSMAN = 0.35;
	static constexpr double PROJECTILE_TIME_WEIGHT_MAGE = 0.45;
	static constexpr double PROJECTILE_TIME_WEIGHT_SUPPORT = 0.3;

	static constexpr const char *CHAMPION_SCHEMA_PATH = "res://fixtures/goldens/champion_schema.json";

	Array _units;
	Array _projectiles;
	Array _scratch_projectiles;
	Array _summary_unit_stats;
	Dictionary _summary_cache;
	std::mt19937_64 _rng;
	double _time = 0.0;
	double _tick_rate = DEFAULT_TICK_RATE;
	int64_t _seed = 0;
	StringName _winner_team = StringName("draw");
	bool _record_events = false;
	Array _player_comp;
	Array _enemy_comp;
	int64_t _player_kills = 0;
	int64_t _enemy_kills = 0;
	Dictionary _champion_catalog;
	Dictionary _role_configs;
	Dictionary _passive_registry;
	Dictionary _unit_index_map;
	Dictionary _alive_unit_indices_by_team;
	bool _catalog_loaded = false;

	void _reset_runtime_state();
	double _randf();
	Dictionary _load_json_file(const String &path) const;
	void _ensure_catalog_loaded();
	void _build_role_configs();
	void _build_passive_registry();
	static int64_t _opcode_for_kind(const StringName &kind);
	Dictionary _compile_effect(const Dictionary &effect) const;
	Array _compile_effect_array(const Array &effects) const;
	Dictionary _coerce_match_input(const Variant &match_input) const;
	void _populate_runtime_state(const Dictionary &match_input);
	void _append_team_units(const Array &spawn_specs, const StringName &team, int64_t &next_instance_id, Array &team_comp);
	Dictionary _build_unit_state(const Dictionary &spawn_spec, const StringName &team, int64_t instance_id);
	Array _alive_indices_for_team(const StringName &team) const;
	void _add_alive_index(const StringName &team, int64_t index);
	void _remove_alive_index(const StringName &team, int64_t index);
	void _refresh_target_pressure();
	void _adjust_target_pressure(int64_t old_target_id, int64_t new_target_id);
	void _simulate();
	void _step_tick();
	void _update_cooldowns_and_status();
	void _update_projectiles();
	void _process_actions();
	Dictionary _strategy_for_unit(const Dictionary &unit) const;
	Dictionary _scale_priority_dict(const Dictionary &source) const;
	double _score_enemy_target(const Dictionary &attacker, const Dictionary &enemy, const Dictionary &strategy) const;
	double _score_ally_target(const Dictionary &unit, const Dictionary &ally, const Dictionary &strategy) const;
	bool _should_switch(const Dictionary &unit, double current_score, double new_score, const Dictionary &strategy) const;
	bool _try_cast_ability(Dictionary &unit, Dictionary &target, double distance);
	bool _try_cast_ultimate(Dictionary &unit, Dictionary &target, double distance);
	bool _start_cast(Dictionary &unit, Dictionary &target, double distance, const StringName &action_kind);
	void _resolve_cast(Dictionary &unit);
	void _perform_auto_attack(Dictionary &unit, Dictionary &target, double distance);
	void _move_toward_target(Dictionary &unit, Dictionary &target);
	void _resolve_projectile(const Dictionary &projectile);
	Dictionary _build_context(const Dictionary &source, const Variant &target, const Variant &target_ally, double damage, const StringName &action_kind);
	Array _collect_effects(const Dictionary &unit, const StringName &kind);
	double _apply_attack_modifiers(Dictionary &unit, Dictionary &target, double distance, double damage);
	double _apply_damage(Dictionary &source, Dictionary &target, double damage, const StringName &damage_type, const StringName &action_kind, const Dictionary &context);
	double _defense_multiplier(Dictionary &target, Dictionary &source, double damage, const StringName &action_kind);
	double _damage_type_multiplier(const Dictionary &target, const StringName &damage_type);
	double _evaluate_multiplier_effect(const Variant &effect, const Dictionary &context, double current_value);
	Dictionary _execute_effect(const Variant &effect, const Dictionary &context);
	void _merge_result(Dictionary &target_result, const Dictionary &source_result);
	void _run_post_attack_effects(Dictionary &source, Dictionary &target, double damage, const Dictionary &context);
	void _apply_stun(Dictionary &source, Dictionary &target, double duration);
	void _add_shield(Dictionary &source, Dictionary &target, double amount);
	void _heal_unit(Dictionary &source, Dictionary &target, double amount);
	void _restore_mana(Dictionary &source, Dictionary &target, double amount);
	void _apply_splash_damage(Dictionary &source, Dictionary &target, double damage, double radius, const StringName &damage_type, const StringName &action_kind, const String &reason, double splash_ratio = 0.5);
	void _apply_aoe_taunt(Dictionary &source, double radius, double duration);
	void _apply_aoe_damage(Dictionary &source, Dictionary &center_source, double damage, double radius, const StringName &damage_type, const String &reason, const StringName &action_kind);
	Dictionary _select_enemy_target(Dictionary &unit);
	Dictionary _select_ally_target(Dictionary &unit);
	double _distance_between(const Dictionary &left, const Dictionary &right) const;
	double _distance_between_dicts(const Dictionary &left, const Dictionary &right) const;
	double _attack_range(const Dictionary &unit) const;
	double _effective_attack_range(const Dictionary &unit) const;
	Dictionary _unit_by_id(int64_t instance_id) const;
	int64_t _unit_index_by_id(int64_t instance_id) const;
	void _handle_death(Dictionary &killer, Dictionary &target);
	StringName _determine_winner() const;
	void _respawn_unit(Dictionary &unit);
	void _set_current_target(Dictionary &unit, const Dictionary &target);
	Dictionary _build_summary();
	Dictionary _effect_to_dict(const Variant &effect) const;
	Dictionary _champion_for(const StringName &archetype_id) const;

public:
	TeamfightSimulationCore();
	~TeamfightSimulationCore() override;

	bool is_ready() const;
	void clear();
	Dictionary run_match(const Variant &match_input);
	Array run_matches(const Array &match_inputs);
};

#endif
