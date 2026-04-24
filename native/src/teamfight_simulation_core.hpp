#ifndef TEAMFIGHT_SIMULATION_CORE_HPP
#define TEAMFIGHT_SIMULATION_CORE_HPP

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "python_random.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>

using namespace godot;

class TeamfightSimulationCore : public RefCounted {
	GDCLASS(TeamfightSimulationCore, RefCounted);

protected:
	static void _bind_methods();

private:
	struct UnitState;

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
		StringName role_id;
		/// Numeric combat fields copied from `stats` at spawn; use in hot paths instead of Dictionary::get.
		struct CombatStats {
			double max_hp = 0.0;
			double max_mana = 0.0;
			double ability_cd = 0.0;
			double ultimate_cd = 0.0;
			double attack_range = 0.0;
			double move_speed = 0.0;
			double attack_speed = 1.0;
			double attack_damage = 0.0;
			double projectile_speed = 0.0;
			double projectile_radius = 0.0;
			double life_steal = 0.0;
			double mana_per_attack = 0.0;
			double respawn_time = 0.0;
			double armor = 0.0;
			double magic_resist = 0.0;
			double tenacity = 0.0;
		} combat;
		Dictionary stats;
		std::array<std::vector<EffectRecord>, 5> passive_effects;
		EffectRecord ability_effect;
		EffectRecord ultimate_effect;
		bool has_ability_effect = false;
		bool has_ultimate_effect = false;
		double spawn_pos_x = 0.0;
		double spawn_pos_y = 0.0;
		double pos_x = 0.0;
		double pos_y = 0.0;
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
		struct DamageSourceEntry {
			double damage = 0.0;
			double last_time = 0.0;
		};
		std::unordered_map<int64_t, DamageSourceEntry> damage_sources;
		std::unordered_map<int64_t, double> recent_benefactors;
		double last_hit_time = 0.0;
		double regen_accumulator = 0.0;
	};

	struct UnitStrategy {
		String display_name;
		double distance_weight = 1.0;
		double hp_weight = 0.0;
		double ally_distance_weight = 1.0;
		double ally_hp_weight = 0.0;
		double ally_threat_weight = 0.0;
		std::map<StringName, double> ally_role_priorities;
		double stickiness_bonus = 2.0;
		bool prefers_kiting = false;
		double bucket_margin = 0.75;
		std::array<StringName, 8> bucket_order{};
		int bucket_order_len = 0;
		double switch_margin = 0.75;
		double in_range_bonus = 0.6;
		double tank_penalty = 2.0;
		double threat_response_weight = 0.0;
		double projectile_time_weight = 0.0;
		double execute_bonus_weight = 0.0;
		double carry_peel_weight = 0.0;
		double prey_instinct_weight = 0.0;
		double cluster_weight = 0.0;
		double spacing_weight = 0.0;
		double bodyguard_weight = 0.0;
		double obscurance_weight = 0.0;
		double flanking_weight = 0.0;
		double threat_decay_rate = 2.0;
		std::map<StringName, double> role_priorities;
	};

	struct TickContext {
		std::vector<int64_t> density_by_unit_index;
		Vector2 player_team_center;
		Vector2 enemy_team_center;
		bool has_player_center = false;
		bool has_enemy_center = false;
		std::vector<int64_t> player_backliner_indices;
		std::vector<int64_t> enemy_backliner_indices;
	};

	struct TraceEvent {
		double t = 0.0;
		StringName kind;
		int64_t src = 0;
		int64_t tgt = 0;
		double val = 0.0;
	};

	struct BalancePatch {
		std::vector<StringName> targets; // empty = applies to all archetypes
		std::vector<StringName> roles;   // empty = applies to all roles
		Dictionary stat_multipliers;
		Dictionary stat_additions;
		StringName kit_id; // empty = none; resolved against _ability_kits
		// Optional subtree overrides (Variant::NIL means not specified in patch JSON).
		Variant ability_override;
		Variant ultimate_override;
		Variant passive_ids_override;
	};

	struct ProjectileState {
		int64_t source_id = 0;
		int64_t target_id = 0;
		double pos_x = 0.0;
		double pos_y = 0.0;
		double speed = 0.0;
		double damage = 0.0;
		StringName damage_type;
		double stun_duration = 0.0;
		double radius = 0.0;
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
	// POS_SCALE removed: positions are now stored as IEEE 754 double to match Python oracle arithmetic.
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
	static constexpr int SPATIAL_GRID_DIM = 8;
	/// Broad-phase for targeting/density/kite/obscurance only when a team has this many **alive** units (6+). Standard 5v5 (5 alive) stays brute — avoids grid overhead at small n.
	static constexpr int SPATIAL_BROAD_PHASE_TEAM_THRESHOLD = 6;
	/// Separation ally scan uses a grid only at this team alive count or above (custom large teams); 5v5 uses brute O(n) with tiny n.
	static constexpr int SPATIAL_SEPARATION_TEAM_THRESHOLD = 6;

	static constexpr const char *CHAMPION_SCHEMA_PATH = "res://fixtures/goldens/champion_schema.json";
	static constexpr const char *BALANCE_PATCHES_PATH = "res://fixtures/goldens/balance_patches.json";
	static constexpr const char *ABILITY_KITS_PATH = "res://fixtures/goldens/ability_kits.json";

	std::vector<UnitState> _units;
	std::vector<ProjectileState> _projectiles;
	std::vector<ProjectileState> _scratch_projectiles;
	Array _summary_unit_stats;
	Dictionary _summary_cache;
	CPythonRandom _rng;
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
	std::vector<BalancePatch> _balance_patches;
	Dictionary _ability_kits; // "kit_id" -> kit Dictionary (ability, ultimate, passive_ids)
	Dictionary _effective_champion_by_archetype; // key: String archetype_id, value: effective champion Dictionary
	Dictionary _passive_registry;
	std::unordered_map<int64_t, int64_t> _unit_index_map;
	std::vector<int64_t> _alive_player_indices;
	std::vector<int64_t> _alive_enemy_indices;
	bool _catalog_loaded = false;
	std::map<StringName, UnitStrategy> _role_strategy_cache;
	UnitStrategy _default_strategy;
	TickContext _tick_ctx;
	std::vector<TraceEvent> _trace_buffer;
	static constexpr size_t TRACE_BUFFER_CAP = 4096;
	bool _debug_combat_trace = false;

	mutable std::array<std::vector<int64_t>, SPATIAL_GRID_DIM * SPATIAL_GRID_DIM> _spatial_buckets;
	mutable std::array<std::vector<int64_t>, SPATIAL_GRID_DIM * SPATIAL_GRID_DIM> _spatial_buckets_aux;
	mutable std::vector<uint32_t> _spatial_stamp;
	mutable uint32_t _spatial_generation = 1;

	void _reset_runtime_state();
	double _randf();
	Dictionary _load_json_file(const String &path) const;
	Dictionary _load_json_file_if_exists(const String &path) const;
	void _ensure_catalog_loaded();
	void _build_role_configs();
	void _build_passive_registry();
	bool _patch_applies_to(const BalancePatch &patch, const StringName &archetype_id, const StringName &role) const;
	void _apply_stat_patch_to_stats(const BalancePatch &patch, Dictionary &stats) const;
	void _merge_kit_into_champion(Dictionary &champion, const Dictionary &kit) const;
	void _rebuild_effective_champion_cache();
	bool _validate_effective_champion(const StringName &archetype_id, const Dictionary &champion) const;
	Dictionary _effective_champion_for(const StringName &archetype_id) const;
	static void _parse_balance_patch_from_dict(const Dictionary &pd, BalancePatch &patch);
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
	/// When update_cluster_density is false, only refresh incoming_target_count (Python: end-of-tick
	/// refresh_target_pressure does not recompute density cache).
	void _refresh_target_pressure(bool update_cluster_density = true);
	void _prepare_tick_context();
	void _build_role_strategy_cache();
	const UnitStrategy &_strategy_for_unit(const UnitState &unit) const;
	void _emit_trace(const StringName &kind, int64_t src_id, int64_t tgt_id, double val);
	void _adjust_target_pressure(int64_t old_target_id, int64_t new_target_id);
	void _simulate();
	void _step_tick();
	void _update_unit(UnitState &unit);
	void _prune_assist_window(UnitState &unit);
	void _update_projectiles();
	bool _kite_from_enemies(UnitState &unit);
	double _score_enemy_target(const UnitState &attacker, const UnitState &enemy, const UnitStrategy &strategy, const TickContext &ctx);
	double _score_ally_target(const UnitState &unit, const UnitState &ally, const UnitStrategy &strategy) const;
	bool _should_switch(const UnitState &unit, double current_score, double new_score, const UnitStrategy &strategy) const;
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
	void _touch_damage_source(UnitState &target, int64_t source_id, double incoming_damage);
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

	void _spatial_ensure_stamp_size() const;
	void _spatial_clear_buckets() const;
	void _spatial_clear_buckets_aux() const;
	double _spatial_cell_size() const;
	int _spatial_flat_index(double x, double y) const;
	void _spatial_add_alive_team(const StringName &team) const;
	void _spatial_rebuild_all_alive() const;
	void _spatial_next_generation() const;
	void _spatial_stamp_circle(double cx, double cy, double radius, const StringName &team) const;
	void _spatial_stamp_kite_threat(double cx, double cy, double danger_radius) const;
	void _spatial_stamp_separation_candidates(double cx, double cy, double radius, const StringName &team, int64_t self_instance_id) const;
	bool _spatial_stamp_has(int64_t unit_index) const;
	void _spatial_fill_buckets_for_indices(const std::vector<int64_t> &indices) const;
	int _spatial_count_neighbors_in_grid(int64_t self_index, double cx, double cy, double radius) const;
	int _spatial_count_obscurance_blockers(double ux, double uy, double tx, double ty, const std::vector<int64_t> &enemy_indices, int64_t target_instance_id) const;
	bool _use_spatial_broad_phase() const;

public:
	TeamfightSimulationCore();
	~TeamfightSimulationCore() override;

	bool is_ready() const;
	void clear();
	Dictionary run_match(const Variant &match_input);
	Array run_matches(const Array &match_inputs);
	void run_match_simulation_only(const Variant &match_input);
	void run_matches_simulation_only(const Array &match_inputs);

	/// Incremental match API (used by SimRunner / gameplay loops). Does not replace run_match for batch parity runs.
	void begin_match(const Variant &match_input);
	void advance_one_tick();
	bool match_ticks_exhausted() const;
	Dictionary finish_and_summarize();
	/// Debug: compact per-unit state after the last completed tick (parity tooling).
	Dictionary get_tick_snapshot() const;
	Array get_trace_events() const;

	// Balance patch API: inject patches at runtime without modifying balance_patches.json.
	// Each element in `patches` must be a Dictionary with keys:
	//   "targets"         Array[String]   — archetype IDs to target (empty = all)
	//   "roles"           Array[String]   — roles to target (empty = all)
	//   "stat_multipliers" Dictionary     — { stat_name: multiplier }
	//   "stat_additions"  Dictionary      — { stat_name: delta }
	// Calling this resets the catalog so patches take effect on the next run_match().
	void set_balance_patches(const Array &patches);
	Array get_balance_patches() const;
};

#endif
