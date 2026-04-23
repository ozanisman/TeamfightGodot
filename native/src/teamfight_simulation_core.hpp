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
	static constexpr double MATCH_DURATION = 60.0;
	static constexpr double DEFAULT_TICK_RATE = 0.1;
	static constexpr double EPSILON = 0.000001;
	static constexpr double RESPAWN_TIME = 10.0;
	static constexpr double ASSIST_WINDOW = 5.0;
	static constexpr double REGEN_TICK_INTERVAL = 1.0;
	static constexpr double CASTING_WINDUP = 0.5;
	static constexpr double RANGED_THRESHOLD = 1.0;
	static constexpr double MELEE_CONTACT_BUFFER = 0.00001;
	static constexpr double DEFAULT_PROJECTILE_SPEED = 5.0;
	static constexpr double DEFAULT_PROJECTILE_RADIUS = 0.03;
	static constexpr int64_t SIMULATION_RULES_VERSION = 1;

	static constexpr const char *CHAMPION_SCHEMA_PATH = "res://fixtures/goldens/champion_schema.json";

	Array _units;
	Array _projectiles;
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
	bool _catalog_loaded = false;

	void _reset_runtime_state();
	double _randf();
	Dictionary _load_json_file(const String &path) const;
	void _ensure_catalog_loaded();
	void _build_role_configs();
	void _build_passive_registry();
	Dictionary _coerce_match_input(const Variant &match_input) const;
	void _populate_runtime_state(const Dictionary &match_input);
	void _append_team_units(const Array &spawn_specs, const StringName &team, int64_t &next_instance_id, Array &team_comp);
	Dictionary _build_unit_state(const Dictionary &spawn_spec, const StringName &team, int64_t instance_id);
	void _simulate();
	void _step_tick();
	void _update_cooldowns_and_status();
	void _update_projectiles();
	void _process_actions();
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
	Dictionary _select_enemy_target(const Dictionary &unit);
	Dictionary _select_ally_target(const Dictionary &unit);
	bool _is_better_target(const Dictionary &attacker, const Dictionary &candidate, const Dictionary &current_best);
	bool _is_better_ally(const Dictionary &attacker, const Dictionary &candidate, const Dictionary &current_best);
	double _distance_between(const Dictionary &left, const Dictionary &right) const;
	double _distance_between_dicts(const Dictionary &left, const Dictionary &right) const;
	double _attack_range(const Dictionary &unit) const;
	double _effective_attack_range(const Dictionary &unit) const;
	Dictionary _unit_by_id(int64_t instance_id) const;
	void _handle_death(Dictionary &killer, Dictionary &target);
	StringName _determine_winner() const;
	void _respawn_unit(Dictionary &unit);
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
