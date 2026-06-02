#ifndef SIMULATION_TYPES_HPP
#define SIMULATION_TYPES_HPP

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "stat_definitions.hpp"

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

using namespace godot;

namespace sim {

enum RoleSlot : int64_t {
	ROLE_SLOT_TANK = 0,
	ROLE_SLOT_FIGHTER = 1,
	ROLE_SLOT_ASSASSIN = 2,
	ROLE_SLOT_MARKSMAN = 3,
	ROLE_SLOT_MAGE = 4,
	ROLE_SLOT_SUPPORT = 5,
	ROLE_SLOT_COUNT = 6,
};

/// AOE shape kinds for expandable shape system.
enum class AoShapeKind : int {
	Circle = 0,
	Cone = 1,
	Rectangle = 2,
};

/// AOE anchor kinds for expandable anchor system.
enum class AoAnchorKind : int64_t {
	Self = 0,
	Target = 1,
	Point = 2,
	Forward = 3,
};

/// Parameters for AOE shape iteration.
struct AoShapeParams {
	AoShapeKind shape = AoShapeKind::Circle;
	AoAnchorKind anchor = AoAnchorKind::Self;
	double radius = 0.0;
	double width = 0.0;
	double height = 0.0;
	double rotation_radians = 0.0;
	double anchor_x = 0.0;
	double anchor_y = 0.0;
	int64_t target_id = 0;
};

struct UnitState;

struct EffectRecord {
	int64_t opcode = 0;
	double scalar0 = 0.0;
	double scalar1 = 0.0;
	double scalar2 = 0.0;
	double scalar3 = 0.0;
	double scalar4 = 0.0;
	double scalar5 = 0.0;
	// Casting windup override; -1.0 means use global CASTING_WINDUP. NOTE: Compiling for all effect kinds adds memory overhead (~8 bytes per effect). Revisit if this becomes an issue.
	double windup = -1.0;
	int64_t int0 = 0;
	int64_t int1 = 0;
	int64_t int2 = 0;
	int64_t int3 = 0;
	int64_t int4 = 0;
	StringName damage_type;
	StringName stat_name; // For consume_stacks effects
	String string0; // General purpose string (stacking_mode for consume_stacks)
	String string1; // General purpose string (stack_reason for consume_stacks)
	String reason;
	std::vector<EffectRecord> children;
	StringName requires_result_from; // Which previous effect to check
	StringName requires_field; // Which field to check
	Variant requires_value; // What value to require
	StringName requires_target_status; // Status to check on unit (e.g., "disarm")
	StringName status_target; // "source" or "target", default "target"

	// on_tick_interval timing support for on_tick effects
	double on_tick_interval = 1.0; // Custom interval (seconds), default 1.0
	double accumulator = 0.0; // Per-effect timing accumulator

	// DoT/HoT support
	StringName stacking_mode; // "refresh", "extend", "separate"
	StringName effect_type; // e.g., "burn", "poison", "regen"

	// AOE shape parameters (replaces scalar0-scalar5 for AOE effects)
	AoShapeParams aoe_shape_params;

	// Multi-target effect parameters
	StringName team_filter; // "ally", "enemy", or empty for auto-detect
	std::vector<EffectRecord> sub_effects; // Sub-effects to apply to each target
};

struct EffectContext {
	UnitState *source = nullptr;
	UnitState *target = nullptr;
	UnitState *target_ally = nullptr;
	double damage = 0.0;
	StringName action_kind;
	double distance = 0.0;
	double heal_amount = 0.0;
	double heal_gained = 0.0;
	bool use_heal_gained = false;
	struct EffectExecResult {
		bool present = false;
		bool success_set = false;
		bool success = false;
		bool condition_failed_set = false;
		bool condition_failed = false;
		bool target_killed_set = false;
		bool target_killed = false;
		bool projectile_created_set = false;
		bool projectile_created = false;
		bool stun_applied_set = false;
		bool stun_applied = false;
		bool shield_applied_set = false;
		bool shield_applied = false;
		bool heal_applied_set = false;
		bool heal_applied = false;
		bool taunt_applied_set = false;
		bool taunt_applied = false;
		bool splash_applied_set = false;
		bool splash_applied = false;
		bool splash_triggered_set = false;
		bool splash_triggered = false;
		bool knockback_applied_set = false;
		bool knockback_applied = false;
		bool stealth_applied_set = false;
		bool stealth_applied = false;
		bool stealth_broken_set = false;
		bool stealth_broken = false;
		bool reached_target_set = false;
		bool reached_target = false;
		bool damage_dealt_set = false;
		double damage_dealt = 0.0;
		bool damage_set = false;
		double damage = 0.0;
		bool amount_set = false;
		double amount = 0.0;
		bool duration_set = false;
		double duration = 0.0;
		bool radius_set = false;
		double radius = 0.0;
		bool mana_restored_set = false;
		double mana_restored = 0.0;
		bool mana_drained_set = false;
		double mana_drained = 0.0;
		bool distance_traveled_set = false;
		double distance_traveled = 0.0;

		void clear() {
			*this = EffectExecResult();
		}
	};

	struct EffectExecStore {
		static constexpr size_t SLOT_COUNT = 64;
		std::array<EffectExecResult, SLOT_COUNT> by_opcode{};
		std::array<bool, SLOT_COUNT> present{};

		static inline bool opcode_to_slot(int64_t opcode, size_t &slot) {
			if (opcode <= 0 || opcode >= int64_t(SLOT_COUNT)) {
				return false;
			}
			slot = static_cast<size_t>(opcode);
			return true;
		}

		void clear() {
			present.fill(false);
			for (EffectExecResult &result : by_opcode) {
				result.clear();
			}
		}

		EffectExecResult *ensure(int64_t opcode) {
			size_t slot = 0;
			if (!opcode_to_slot(opcode, slot)) {
				return nullptr;
			}
			present[slot] = true;
			by_opcode[slot].present = true;
			return &by_opcode[slot];
		}

		const EffectExecResult *get(int64_t opcode) const {
			size_t slot = 0;
			if (!opcode_to_slot(opcode, slot) || !present[slot]) {
				return nullptr;
			}
			return &by_opcode[slot];
		}

		EffectExecResult *get(int64_t opcode) {
			size_t slot = 0;
			if (!opcode_to_slot(opcode, slot) || !present[slot]) {
				return nullptr;
			}
			return &by_opcode[slot];
		}
	};

	Dictionary accumulated_results;
	/// Prevents reflected damage from chaining into further reflects.
	bool suppress_reflect_chain = false;

	// Takedown-specific context
	int64_t takedown_target_id = 0; // Unit that was killed
	double takedown_damage_dealt = 0.0; // Damage contributed by participant
	bool is_takedown_kill = false; // True for killer, false for assists

	// Channel-specific context
	double channel_remaining_duration = 0.0;
	int64_t channel_tick_count = 0;
	double channel_accumulated_damage = 0.0;
	bool channel_completed = false;
};

/// Stack behavior types for stat modifier stacking
enum class StackBehavior : int8_t {
	Refresh = 0, // Reset duration to max (current behavior)
	Accumulate = 1, // Add duration to current
	Reset = 2 // Reset stacks to 1, refresh duration
};

/// Pending spawn entry for mid-tick unit creation
struct PendingSpawn {
	Dictionary spawn_spec;
	StringName team;
	int64_t summoner_instance_id;
};

/// Passive reflect entry for tracking reflect damage sources
struct PassiveReflectEntry {
	double percentage = 0.0;
	StringName damage_type; // "all" or "physical"
	StringName action_kind; // "passive", "ability", "ultimate"
};

/// Loadout, compiled effects, spawn snapshot, casting payload, and combat telemetry (updated on events, not inner targeting loops).
struct UnitStateCold {
	struct DamageSourceEntry {
		double damage = 0.0;
		double last_time = 0.0;
	};
	StringName unit_id;
	StringName role_id;
	Dictionary stats;
	std::array<std::vector<EffectRecord>, 10> passive_effects;
	double on_ally_defense_radius = 0.0; // Radius for on_ally_defense triggers
	// Passive AOE radius information for visualization
	struct PassiveAoeInfo {
		StringName passive_id;
		double radius;
	};
	std::vector<PassiveAoeInfo> passive_aoe_info;
	std::vector<PassiveReflectEntry> passive_reflect_entries;
	EffectRecord ability_effect;
	EffectRecord ultimate_effect;
	double spawn_pos_x = 0.0;
	double spawn_pos_y = 0.0;
	StringName casting_kind;
	EffectRecord casting_effect;
	double damage_dealt = 0.0;
	double damage_dealt_auto = 0.0;
	double damage_dealt_ability = 0.0;
	double damage_dealt_ultimate = 0.0;
	double damage_dealt_passive = 0.0;
	double damage_received = 0.0;
	double damage_mitigated = 0.0;
	double healing_done = 0.0;
	double healing_done_auto = 0.0;
	double healing_done_ability = 0.0;
	double healing_done_ultimate = 0.0;
	double healing_done_passive = 0.0;
	double shielding_done = 0.0;
	double shielding_done_auto = 0.0;
	double shielding_done_ability = 0.0;
	double shielding_done_ultimate = 0.0;
	double shielding_done_passive = 0.0;
	int64_t auto_attacks = 0;
	int64_t abilities = 0;
	int64_t ultimates = 0;
	int64_t stuns = 0;
	int64_t kills = 0;
	int64_t deaths = 0;
	int64_t assists = 0;
	std::unordered_map<int64_t, DamageSourceEntry> damage_sources;

	// Minion stats aggregated to this summoner
	double minion_damage_dealt = 0.0;
	double minion_damage_received = 0.0;
	std::unordered_map<int64_t, double> recent_benefactors;
	double last_hit_time = 0.0;
	int64_t respawn_slot_index = -1; // -1 = no assigned slot
	StringName forced_target_kind;

	std::vector<double> on_tick_effect_accumulators;

	// Periodic effects (DoT/HoT)
	struct PeriodicEffect {
		StringName effect_type; // e.g., "burn", "poison", "regen"
		double damage_total = 0.0; // Total damage over full duration
		double heal_total = 0.0; // Total heal over full duration
		double remaining_duration = 0.0;
		double tick_interval = 0.0;
		double tick_accumulator = 0.0;
		double original_tick_count = 0.0; // Total ticks over original duration (duration / tick_interval)
		int64_t source_instance_id = 0;
		StringName damage_type; // "physical", "magical", "true"
		StringName stacking_mode; // "refresh", "extend", "separate"
		String reason;
		bool allow_overheal = false;
		int stack_count = 0;
		int max_stacks = 0;
		StringName action_kind; // "auto", "ability", "ultimate", "passive"

		// Total ratio parameters for dynamic calculation mode
		double total_attack_damage_ratio = 0.0;
		double total_max_hp_ratio = 0.0;
		double total_current_hp_ratio = 0.0;
		double total_missing_hp_ratio = 0.0;
		double total_flat_amount = 0.0;
		StringName calculation_mode = StringName("fixed"); // "fixed" or "dynamic"
	};
	std::vector<PeriodicEffect> periodic_effects;

	// Reflect buff effects (active reflect from abilities/ultimates)
	struct ReflectBuff {
		double percentage = 0.0;
		double remaining_duration = 0.0;
		StringName action_kind; // "auto", "ability", "ultimate", "passive"
		int64_t source_instance_id = 0; // Who granted this buff (for AOE reflect attribution)
		StringName damage_type; // "all" or "physical"
		String reason;
	};
	std::vector<ReflectBuff> reflect_buffs;

	// Channel effect state
	bool is_channeling = false;
	double channel_remaining_duration = 0.0;
	double channel_tick_interval = 0.0;
	double channel_tick_accumulator = 0.0;
	double channel_accumulated_damage = 0.0;
	int64_t channel_target_instance_id = 0;
	int64_t channel_source_instance_id = 0;
	StringName channel_reason;
	StringName channel_action_kind;
	bool channel_allow_movement = false;
	StringName channel_target_mode; // "fixed" or "dynamic"
	EffectRecord channel_sub_effect;
	EffectRecord channel_post_complete_effect;
	EffectRecord channel_post_interrupt_effect;
};

/// Per-tick combat and movement hot path; keep compact—cold lives in `UnitStateCold` at same index.
struct UnitState {
	int64_t instance_id = 0;
	StringName team;
	int64_t role_slot = -1;
	int64_t summoner_instance_id = 0; // Tracks which unit summoned this minion (0 if not a minion)
	/// Numeric combat fields copied from `stats` at spawn; use in hot paths instead of Dictionary::get.
	struct CombatStats {
#define X(name, def, min_val, max_val) double name = def;
		STAT_LIST
#undef X
	} combat;
	bool has_ability_effect = false;
	bool has_ultimate_effect = false;
	bool ability_requires_target_in_range = true;
	bool ultimate_requires_target_in_range = true;
	double pos_x = 0.0;
	double pos_y = 0.0;
	double hp = 0.0;
	double shield = 0.0;
	double mana = 0.0;
	double attack_cooldown = 0.0;
	double attack_period = 0.0;
	double ability_cooldown = 0.0;
	double casting_remaining = 0.0;
	bool has_casting_effect = false;
	int64_t casting_target_id = 0;
	int64_t casting_ally_target_id = 0;
	bool cast_resolved_this_tick = false;
	int64_t target_id = 0;
	int64_t target_index = -1;
	int64_t current_ally_target_id = 0;
	double retarget_timer = 0.0;
	double target_switch_lock_timer = 0.0;
	double last_kite_timer = 0.0;
	double current_target_score = 0.0;
	double stun_remaining = 0.0;
	/// Seconds alive during hard CC (stun_remaining > 0); incremented per tick by min(remaining, tick_rate).
	double hard_cc_seconds = 0.0;
	/// Movement slow: remaining duration and multiplier on base move speed (1.0 = no slow).
	double slow_remaining = 0.0;
	double slow_move_mult = 1.0;
	/// Cannot move (separation / kite / chase) while > 0; can still cast and auto if in range.
	double root_remaining = 0.0;
	double silence_remaining = 0.0;
	double silence_ability_remaining = 0.0;
	double silence_ultimate_remaining = 0.0;
	bool silence_blocks_abilities = false;
	bool silence_blocks_ultimates = false;
	double disarm_remaining = 0.0;
	/// Stealth: remaining duration. Cannot be targeted by enemies while > 0.
	double stealth_remaining = 0.0;
	/// Stealth break conditions: when to break stealth (on_attack, on_ability, on_damage_taken).
	bool stealth_break_on_attack = false;
	bool stealth_break_on_ability = false;
	bool stealth_break_on_damage_taken = false;
	double respawn_timer = 0.0;
	bool respawned_this_tick = false;
	bool alive = true;
	int64_t incoming_target_count = 0;
	double perceived_threat = 0.0;
	int64_t attack_count = 0;
	int64_t taunt_target_id = 0;
	double taunt_remaining = 0.0;
	int64_t forced_target_id = 0;
	double forced_target_remaining = 0.0;
	double regen_accumulator = 0.0;
	bool is_tank_role = false;
	bool is_fighter_role = false;
	bool is_assassin_role = false;
	bool is_marksman_role = false;
	bool is_mage_role = false;
	bool is_support_role = false;
	bool is_backliner = false; // Cached backliner status for O(1) lookup

	// Stat modifier system - generated by X-Macro
#define X(name, def, min_val, max_val) \
	double stat_additive_##name = 0.0; \
	double stat_multiplicative_##name = 1.0;
	STAT_LIST
#undef X

	// Per-source stack tracking for stat modifiers
	Dictionary stat_stacks; // key: "stat_name|reason", value: StackInfo
	Dictionary stat_modifiers; // key: "stat_name|reason", value: simple modifier info
};

// Data-driven role priority configuration
struct RolePriorityConfig {
	StringName role;
	double priority;
};

struct StrategyRolePriorities {
	std::array<RolePriorityConfig, 8> enemy_priorities;
	std::array<RolePriorityConfig, 8> ally_priorities;
};

// Debug: scoring breakdown for targeting decisions
// TODO: Consider changing to compile-time flag (#ifdef) for zero overhead in production
struct ScoreBreakdown {
	double distance = 0.0;
	double distance_weighted = 0.0;
	double hp_ratio = 0.0;
	double hp_weighted = 0.0;
	double role_priority = 0.0;
	double threat = 0.0;
	double threat_weighted = 0.0;
	double in_range_bonus = 0.0;
	double execute_bonus = 0.0;
	double support_peel = 0.0;
	double spacing = 0.0;
	double kiting_tempo = 0.0;
	double total = 0.0;
};

struct UnitStrategy {
	String display_name;
	double distance_weight = 1.0;
	double hp_weight = 0.0;
	double ally_distance_weight = 1.0;
	double ally_hp_weight = 0.0;
	double ally_threat_weight = 0.0;
	std::array<double, ROLE_SLOT_COUNT> ally_role_priorities{};
	bool prefers_kiting = false;
	double switch_margin = 2.0;
	double in_range_bonus = 0.6;
	double threat_response_weight = 0.0;
	double execute_bonus_weight = 0.0;
	double spacing_weight = 0.0;
	double threat_decay_rate = 2.0;
	bool uses_ally_targeting = false;
	std::array<double, ROLE_SLOT_COUNT> role_priorities{};
};

// Comprehensive strategy configuration for data-driven role behavior
struct StrategyConfig {
	// Identity
	String display_name;
	StringName role_name;

	// Targeting weights
	double distance_weight;
	double hp_weight;
	double ally_distance_weight;
	double ally_hp_weight;
	double ally_threat_weight;

	// Behavior modifiers
	double in_range_bonus;
	double threat_response_weight;
	double execute_bonus_weight;

	// Positioning weights
	double spacing_weight;

	// Timing
	double threat_decay_rate;

	// Switching
	double switch_margin;

	// Flags
	bool prefers_kiting;
	bool uses_ally_targeting;

	// Role priorities
	StrategyRolePriorities role_priorities;
};

struct TickContext {
	std::vector<int64_t> density_by_unit_index;
	Vector2 player_team_center;
	Vector2 enemy_team_center;
	bool has_player_center = false;
	bool has_enemy_center = false;
	std::vector<int64_t> player_backliner_indices;
	std::vector<int64_t> enemy_backliner_indices;
	/// Alive backliner count for team; reset in _prepare_tick_context, decremented in match lifecycle handle_death (matches `other.alive` scans over backliner lists).
	int player_backliner_alive_count = 0;
	int enemy_backliner_alive_count = 0;
	/// Alive tank+fighter per team (spatial grid insert).
	std::vector<int64_t> player_frontline_indices;
	std::vector<int64_t> enemy_frontline_indices;
};

struct TargetingFrameEntry {
	int64_t instance_id = 0;
	bool is_player_team = true;
	int64_t role_slot = -1;
	bool is_tank_role = false;
	bool is_fighter_role = false;
	bool is_assassin_role = false;
	bool is_marksman_role = false;
	bool is_mage_role = false;
	bool is_support_role = false;
	double pos_x = 0.0;
	double pos_y = 0.0;
	double hp = 0.0;
	double max_hp = 0.0;
	bool alive = true;
	int64_t target_id = 0;
	int64_t incoming_target_count = 0;
	double perceived_threat = 0.0;
	double stealth_remaining = 0.0;
};

struct TargetScoreContext {
	double attack_range = 0.0;
	double effective_range = 0.0;
	bool has_kite_bounds = false;
	double kite_min_w = 0.0;
	double kite_max_w = 0.0;
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
	std::vector<StringName> roles; // empty = applies to all roles
	Dictionary stat_multipliers;
	Dictionary stat_additions;
	StringName kit_id; // empty = none; resolved against _ability_kits
	// Optional subtree overrides (Variant::NIL means not specified in patch JSON).
	Variant ability_override;
	Variant ultimate_override;
	Variant passive_ids_override;
};

struct ProjectileState {
	int64_t projectile_id = 0;
	int64_t source_id = 0;
	int64_t target_id = 0;
	double pos_x = 0.0;
	double pos_y = 0.0;
	double speed = 0.0;
	double radius = 0.0;
	EffectRecord impact_effect;
	StringName motion;
	StringName collision;
	StringName on_target_lost;
	StringName visual_id;
	StringName action_kind;
	String reason;
};

enum TargetSelectionStrategy : int64_t {
	TARGET_SELECTION_CLOSEST = 0,
	TARGET_SELECTION_RANDOM = 1,
	TARGET_SELECTION_LOWEST_HP = 2,
	TARGET_SELECTION_HIGHEST_HP = 3,
	TARGET_SELECTION_CLOSEST_TO_TARGET = 4,
	TARGET_SELECTION_LOWEST_PERCENT_HP = 5,
	TARGET_SELECTION_HIGHEST_PERCENT_HP = 6,
};

enum ExcessTargetHandling : int64_t {
	EXCESS_TARGET_DROP = 0,
	EXCESS_TARGET_STACK = 1,
};

enum EffectOpcode : int64_t {
	// Sentinel value for unrecognized/invalid effects
	EFFECT_OPCODE_UNKNOWN = 0,

	// Core effects (1-99): Fundamental effects
	EFFECT_OPCODE_MULTI_EFFECT = 1,
	EFFECT_OPCODE_DAMAGE = 2,
	EFFECT_OPCODE_PROJECTILE = 3,
	EFFECT_OPCODE_MULTI_TARGET = 4,
	EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER = 5,
	EFFECT_OPCODE_AUTO_DODGE = 6,
	EFFECT_OPCODE_CONSTANT_MULTIPLIER = 7,
	EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER = 8,
	EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER = 9,
	EFFECT_OPCODE_REFLECT_DAMAGE = 10,
	EFFECT_OPCODE_TARGET_STATUS_MULTIPLIER = 11,
	EFFECT_OPCODE_STAT_MODIFIER = 12,
	EFFECT_OPCODE_DAMAGE_OVER_TIME = 13,
	EFFECT_OPCODE_CONSUME_STACKS_DAMAGE = 14,
	EFFECT_OPCODE_REDIRECT_DAMAGE = 15,
	EFFECT_OPCODE_SUMMON_ALLY = 16,
	// 17-99: Reserved for future core effects

	// Status effects (100-199): Unit status effects
	EFFECT_OPCODE_STUN = 100,
	EFFECT_OPCODE_SHIELD = 101,
	EFFECT_OPCODE_HEAL = 102,
	EFFECT_OPCODE_MANA_REGEN = 103,
	EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN = 104,
	EFFECT_OPCODE_DAMAGE_BASED_HEAL = 105,
	EFFECT_OPCODE_MANA_RESTORE_ON_HIT = 106,
	EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT = 107,
	EFFECT_OPCODE_EVERY_N_ATTACKS_STUN = 108,
	EFFECT_OPCODE_SELF_DASH = 109,
	EFFECT_OPCODE_SLOW = 110,
	EFFECT_OPCODE_ROOT = 111,
	EFFECT_OPCODE_SILENCE = 112,
	EFFECT_OPCODE_DISARM = 113,
	EFFECT_OPCODE_KNOCKBACK = 114,
	EFFECT_OPCODE_REFLECT = 115,
	EFFECT_OPCODE_KNOCKBACK_SHIELD = 116,
	EFFECT_OPCODE_STEALTH = 117,
	EFFECT_OPCODE_HEAL_OVER_TIME = 118,
	EFFECT_OPCODE_DAMAGE_BASED_SHIELD = 119,
	EFFECT_OPCODE_CONSUME_STACKS_HEAL = 120,
	EFFECT_OPCODE_CONSUME_STACKS_SHIELD = 121,
	EFFECT_OPCODE_SET_STACKS = 122,
	EFFECT_OPCODE_CHANNEL = 123,
	// 124-199: Reserved for future status effects

	// AOE effects (200-299): Area-of-effect variants
	EFFECT_OPCODE_AOE_TAUNT = 200,
	EFFECT_OPCODE_AOE_DAMAGE = 201,
	EFFECT_OPCODE_AOE_SLOW = 202,
	EFFECT_OPCODE_AOE_ROOT = 203,
	EFFECT_OPCODE_AOE_SILENCE = 204,
	EFFECT_OPCODE_AOE_DISARM = 205,
	EFFECT_OPCODE_AOE_KNOCKBACK = 206,
	EFFECT_OPCODE_AOE_REFLECT = 207,
	EFFECT_OPCODE_AOE_STUN = 208,
	EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME = 209,
	EFFECT_OPCODE_AOE_HEAL_OVER_TIME = 210,
	// 211-299: Reserved for future AOE effects

	// Custom effects (300-399): Future custom effects
	// 300-399: Reserved for future custom effects
};

/// Compact HUD/floating labels for the Godot simulation viewer (cleared each tick, filled during sim).
struct ViewerFxEvent {
	StringName kind;
	int64_t target_id = 0;
	int64_t src_id = 0;
	double pos_x = 0.0;
	double pos_y = 0.0;
	double val = 0.0;
	/// World-space radius (aoe_ring / aoe_taunt / aoe_splash); 0 if unused.
	double radius = 0.0;
	/// Damage type for coloring (physical/magic/true); empty if unused.
	StringName damage_type;
	/// Extra parameters for complex effects (e.g., AOE shape parameters).
	Variant extra;
};

} // namespace sim

#endif // SIMULATION_TYPES_HPP
