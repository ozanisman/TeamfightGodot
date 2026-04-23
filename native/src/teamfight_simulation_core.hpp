#ifndef TEAMFIGHT_SIMULATION_CORE_HPP
#define TEAMFIGHT_SIMULATION_CORE_HPP

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <array>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>

using namespace godot;

class TeamfightSimulationCore : public RefCounted {
	GDCLASS(TeamfightSimulationCore, RefCounted);

protected:
	static void _bind_methods();

private:
	struct UnitState;

	struct Pcg32 {
		// Minimal PCG32 (XSH RR) with 64-bit state.
		// Deterministic across platforms/compilers.
		uint64_t state = 0u;
		uint64_t inc = 0u;

		void seed(uint64_t init_state, uint64_t init_seq = 54u) {
			state = 0u;
			inc = (init_seq << 1u) | 1u;
			next_u32();
			state += init_state;
			next_u32();
		}

		uint32_t next_u32() {
			uint64_t oldstate = state;
			state = oldstate * 6364136223846793005ULL + inc;
			uint32_t xorshifted = uint32_t(((oldstate >> 18u) ^ oldstate) >> 27u);
			uint32_t rot = uint32_t(oldstate >> 59u);
			return (xorshifted >> rot) | (xorshifted << ((-int32_t(rot)) & 31));
		}

		double next_unit_f64() {
			// [0, 1)
			return double(next_u32()) / 4294967296.0;
		}
	};

	struct EffectRecord {
		int64_t opcode = 0;
		double scalar0 = 0.0;
		double scalar1 = 0.0;
		double scalar2 = 0.0;
		double scalar3 = 0.0;
		int64_t int0 = 0;
		int64_t int1 = 0;
		int64_t int2 = 0;
		StringName damage_type;
		String reason;
		std::vector<EffectRecord> children;
	};

	struct EffectContext {
		UnitState *source = nullptr;
		UnitState *target = nullptr;
		UnitState *target_ally = nullptr;
		double damage = 0.0;
		StringName action_kind;
		double distance = 0.0;
	};

	struct UnitState {
		int64_t instance_id = 0;
		StringName archetype_id;
		StringName team;
		Dictionary stats;
		std::array<std::vector<EffectRecord>, 5> passive_effects;
		EffectRecord ability_effect;
		EffectRecord ultimate_effect;
		bool has_ability_effect = false;
		bool has_ultimate_effect = false;
		int64_t spawn_x_fp = 0;
		int64_t spawn_y_fp = 0;
		int64_t x_fp = 0;
		int64_t y_fp = 0;
		double hp = 0.0;
		double shield = 0.0;
		double mana = 0.0;
		double attack_cooldown = 0.0;
		double ability_cooldown = 0.0;
		double ultimate_cooldown = 0.0;
		double casting_remaining = 0.0;
		StringName casting_kind;
		EffectRecord casting_effect;
		bool has_casting_effect = false;
		int64_t casting_target_id = 0;
		int64_t casting_ally_target_id = 0;
		bool cast_resolved_this_tick = false;
		int64_t target_id = 0;
		int64_t current_ally_target_id = 0;
		double retarget_timer = 0.0;
		double target_switch_lock_timer = 0.0;
		double last_kite_timer = 0.0;
		double current_target_score = 0.0;
		double stun_remaining = 0.0;
		double respawn_timer = 0.0;
		bool respawned_this_tick = false;
		bool alive = true;
		int64_t incoming_target_count = 0;
		int64_t last_density_count = 0;
		double perceived_threat = 0.0;
		int64_t attack_count = 0;
		double damage_dealt = 0.0;
		double damage_dealt_auto = 0.0;
		double damage_dealt_ability = 0.0;
		double damage_dealt_ultimate = 0.0;
		double damage_received = 0.0;
		double damage_mitigated = 0.0;
		double healing_done = 0.0;
		double shielding_done = 0.0;
		int64_t auto_attacks = 0;
		int64_t abilities = 0;
		int64_t ultimates = 0;
		int64_t stuns = 0;
		int64_t kills = 0;
		int64_t deaths = 0;
		int64_t assists = 0;
		int64_t taunt_target_id = 0;
		double taunt_remaining = 0.0;
		int64_t forced_target_id = 0;
		double forced_target_remaining = 0.0;
		StringName forced_target_kind;
		std::unordered_map<int64_t, double> damage_sources;
		std::unordered_map<int64_t, double> recent_benefactors;
		double last_hit_time = 0.0;
		double regen_accumulator = 0.0;
	};

	struct ProjectileState {
		int64_t source_id = 0;
		int64_t target_id = 0;
		double damage = 0.0;
		StringName damage_type;
		double stun_duration = 0.0;
		double radius = 0.0;
		double time_remaining = 0.0;
		StringName action_kind;
		String reason;
	};

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
	static constexpr double WORLD_BOUNDARY_MIN = 0.2;
	static constexpr double WORLD_BOUNDARY_MAX = 9.8;
	static constexpr double BOUNDARY_DETECTION_MARGIN = 0.05;
	static constexpr double RECOVERY_VELOCITY = 1.0;
	static constexpr double SEPARATION_RADIUS_RANGED = 0.8;
	static constexpr double SEPARATION_RADIUS_MELEE = 0.25;
	static constexpr double SEPARATION_RANGE_THRESHOLD = 1.5;
	static constexpr double NUDGE_SPEED_MODIFIER = 0.4;
	static constexpr double KITE_SPEED_MODIFIER = 0.5;
	static constexpr double KITE_DURATION = 1.0;
	static constexpr double KITE_DANGER_THRESHOLD = 0.9;
	static constexpr double DRAFT_X_BASE = 0.9;
	static constexpr int64_t POS_SCALE = 1000000; // 1 world unit == 1,000,000 fixed units.
	static constexpr double ALLY_CRITICAL_HP_RATIO = 0.35;
	static constexpr double ROLE_PRIORITY_GLOBAL_SCALE = 0.85;
	static constexpr double SCORE_HP_WEIGHT_SCALE = 10.0;
	static constexpr double SCORE_THREAT_WEIGHT_SCALE = 5.0;
	static constexpr double SCORE_DISTANCE_WEIGHT_SCALE = 3.0;
	static constexpr double SCORE_KITING_WEIGHT_SCALE = 1.5;
	static constexpr double DISTANCE_EXPONENT = 1.5;
	static constexpr double SPACING_EXPONENT = 1.5;
	static constexpr double THREAT_RESPONSE_RANGE_FALLOFF = 0.6;
	static constexpr double THREAT_BURST_THRESHOLD = 0.1;
	static constexpr double THREAT_BURST_MULTIPLIER = 5.0;
	static constexpr double TAUNT_SCORE_BONUS = -100.0;
	static constexpr double THREAT_DECAY_DEFAULT = 2.0;
	static constexpr double THREAT_DECAY_TANK = 4.0;
	static constexpr double THREAT_DECAY_FIGHTER = 4.5;
	static constexpr double THREAT_DECAY_FRAGILE = 1.0;
	static constexpr double TARGET_SWITCH_MARGIN = 0.75;
	static constexpr double TARGET_BUCKET_MARGIN = 0.75;
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
	static constexpr double AOE_DENSITY_RADIUS = 2.0;
	static constexpr double OBSCURANCE_WEIGHT_DEFAULT = 4.5;
	static constexpr double OBSCURANCE_LINE_RADIUS = 0.35;
	static constexpr double KITE_TARGET_WINDOW_MIN_FACTOR = 0.7;
	static constexpr double KITE_TARGET_WINDOW_MAX_FACTOR = 1.3;
	static constexpr double SWITCH_COMMIT_WINDOW_SECONDS = 0.18;
	static constexpr double SWITCH_COMMIT_WINDOW_SWING_FRACTION = 0.25;
	static constexpr double FLANKING_TEAM_CENTER_SCALE = 0.35;
	static constexpr double FLANKING_WEIGHT_ASSASSIN = 2.0;
	static constexpr double CLUSTER_WEIGHT_TANK = 1.5;
	static constexpr double CLUSTER_WEIGHT_MAGE = 3.0;
	static constexpr double SPACING_WEIGHT_MARKSMAN = 2.5;
	static constexpr double SPACING_WEIGHT_MAGE = 1.5;
	static constexpr double SPACING_WEIGHT_SUPPORT = 1.0;
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

	std::vector<UnitState> _units;
	std::vector<ProjectileState> _projectiles;
	std::vector<ProjectileState> _scratch_projectiles;
	Array _summary_unit_stats;
	Dictionary _summary_cache;
	Pcg32 _rng;
	double _time = 0.0;
	int64_t _tick = 0;
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
	std::unordered_map<int64_t, int64_t> _unit_index_map;
	std::vector<int64_t> _alive_player_indices;
	std::vector<int64_t> _alive_enemy_indices;
	bool _catalog_loaded = false;
	// Debug-only trace for 1v1 fixtures when record_events=true.
	bool _debug_duel_trace = false;
	int64_t _debug_duel_unit_a = 0;
	int64_t _debug_duel_unit_b = 0;
	bool _debug_targeting = false;
	bool _debug_combat_trace = false;
	String _debug_fixture_name;
	bool _compute_density_cache = false;

	void _reset_runtime_state();
	bool _is_debug_duel_unit(int64_t instance_id) const;
	double _randf();
	Dictionary _load_json_file(const String &path) const;
	void _ensure_catalog_loaded();
	void _build_role_configs();
	void _build_passive_registry();
	static int64_t _opcode_for_kind(const StringName &kind);
	EffectRecord _compile_effect(const Dictionary &effect) const;
	std::vector<EffectRecord> _compile_effect_array(const Array &effects) const;
	Dictionary _coerce_match_input(const Variant &match_input) const;
	void _populate_runtime_state(const Dictionary &match_input);
	void _append_team_units(const Array &spawn_specs, const StringName &team, int64_t &next_instance_id, Array &team_comp);
	UnitState _build_unit_state(const Dictionary &spawn_spec, const StringName &team, int64_t instance_id);
	std::vector<int64_t> &_alive_indices_for_team(const StringName &team);
	const std::vector<int64_t> &_alive_indices_for_team(const StringName &team) const;
	void _add_alive_index(const StringName &team, int64_t index);
	void _remove_alive_index(const StringName &team, int64_t index);
	void _refresh_target_pressure();
	void _adjust_target_pressure(int64_t old_target_id, int64_t new_target_id);
	void _simulate();
	void _step_tick();
	void _update_unit(UnitState &unit);
	void _update_projectiles();
	void _process_actions();
	bool _kite_from_enemies(UnitState &unit);
	Dictionary _strategy_for_unit(const UnitState &unit) const;
	Dictionary _scale_priority_dict(const Dictionary &source) const;
	double _score_enemy_target(const UnitState &attacker, const UnitState &enemy, const Dictionary &strategy) const;
	double _score_ally_target(const UnitState &unit, const UnitState &ally, const Dictionary &strategy) const;
	bool _should_switch(const UnitState &unit, double current_score, double new_score, const Dictionary &strategy) const;
	bool _try_cast_ability(UnitState &unit, UnitState &target, double distance);
	bool _try_cast_ultimate(UnitState &unit, UnitState &target, double distance);
	bool _start_cast(UnitState &unit, UnitState &target, double distance, const StringName &action_kind);
	void _resolve_cast(UnitState &unit);
	void _perform_auto_attack(UnitState &unit, UnitState &target, double distance);
	void _move_toward_target(UnitState &unit, UnitState &target);
	void _resolve_projectile(const ProjectileState &projectile);
	EffectContext _build_context(UnitState &source, UnitState *target, UnitState *target_ally, double damage, const StringName &action_kind);
	const std::vector<EffectRecord> &_collect_effects(const UnitState &unit, const StringName &kind);
	double _apply_attack_modifiers(UnitState &unit, UnitState &target, double distance, double damage);
	double _apply_damage(UnitState &source, UnitState &target, double damage, const StringName &damage_type, const StringName &action_kind, const EffectContext &context);
	double _defense_multiplier(UnitState &target, UnitState &source, double damage, const StringName &action_kind);
	double _damage_type_multiplier(const UnitState &target, const StringName &damage_type);
	double _evaluate_multiplier_effect(const EffectRecord &effect, const EffectContext &context, double current_value);
	void _execute_effect(const EffectRecord &effect, EffectContext &context);
	void _merge_result(Dictionary &target_result, const Dictionary &source_result);
	void _run_post_attack_effects(UnitState &source, UnitState &target, double damage, const EffectContext &context);
	void _apply_stun(UnitState &source, UnitState &target, double duration);
	void _add_shield(UnitState &source, UnitState &target, double amount);
	void _heal_unit(UnitState &source, UnitState &target, double amount);
	void _restore_mana(UnitState &source, UnitState &target, double amount);
	void _apply_splash_damage(UnitState &source, UnitState &target, double damage, double radius, const StringName &damage_type, const StringName &action_kind, const String &reason, double splash_ratio = 0.5);
	void _apply_aoe_taunt(UnitState &source, double radius, double duration);
	void _apply_aoe_damage(UnitState &source, UnitState &center_source, double damage, double radius, const StringName &damage_type, const String &reason, const StringName &action_kind);
	UnitState *_select_enemy_target(UnitState &unit);
	UnitState *_select_ally_target(UnitState &unit);
	double _distance_between(const UnitState &left, const UnitState &right) const;
	double _attack_range(const UnitState &unit) const;
	double _effective_attack_range(const UnitState &unit) const;
	UnitState *_unit_by_id(int64_t instance_id);
	const UnitState *_unit_by_id(int64_t instance_id) const;
	int64_t _unit_index_by_id(int64_t instance_id) const;
	void _handle_death(UnitState &killer, UnitState &target);
	StringName _determine_winner() const;
	void _respawn_unit(UnitState &unit);
	void _set_current_target(UnitState &unit, const UnitState &target);
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
