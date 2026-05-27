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
#include "stat_definitions.hpp"

#include <array>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace godot;

class TeamfightSimulationCore : public RefCounted {
	GDCLASS(TeamfightSimulationCore, RefCounted);

public:
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
		double scalar4 = 0.0;
		double scalar5 = 0.0;
		int64_t int0 = 0;
		int64_t int1 = 0;
		int64_t int2 = 0;
		int64_t int3 = 0;
		int64_t int4 = 0;
		StringName damage_type;
		StringName stat_name;  // For consume_stacks effects
		String string0;  // General purpose string (stacking_mode for consume_stacks)
		String string1;  // General purpose string (stack_reason for consume_stacks)
		String reason;
		std::vector<EffectRecord> children;
		StringName requires_result_from;  // Which previous effect to check
		StringName requires_field;        // Which field to check
		Variant requires_value;           // What value to require
		StringName requires_target_status;  // Status to check on unit (e.g., "disarm")
		StringName status_target;         // "source" or "target", default "target"
		
		// on_tick_interval timing support for on_tick effects
		double on_tick_interval = 1.0;  // Custom interval (seconds), default 1.0
		double accumulator = 0.0;       // Per-effect timing accumulator
		
		// DoT/HoT support
		StringName stacking_mode;       // "refresh", "extend", "separate"
		StringName effect_type;         // e.g., "burn", "poison", "regen"
		
		// AOE shape parameters (replaces scalar0-scalar5 for AOE effects)
		AoShapeParams aoe_shape_params;
		
		// Multi-target effect parameters
		StringName team_filter;         // "ally", "enemy", or empty for auto-detect
		std::vector<EffectRecord> sub_effects;  // Sub-effects to apply to each target
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
		int64_t takedown_target_id = 0;  // Unit that was killed
		double takedown_damage_dealt = 0.0;  // Damage contributed by participant
		bool is_takedown_kill = false;  // True for killer, false for assists

		// Channel-specific context
		double channel_remaining_duration = 0.0;
		int64_t channel_tick_count = 0;
		double channel_accumulated_damage = 0.0;
		bool channel_completed = false;
	};

	/// Stack behavior types for stat modifier stacking
	enum class StackBehavior : int8_t {
		Refresh = 0,    // Reset duration to max (current behavior)
		Accumulate = 1, // Add duration to current
		Reset = 2       // Reset stacks to 1, refresh duration
	};

	/// Pending spawn entry for mid-tick unit creation
	struct PendingSpawn {
		Dictionary spawn_spec;
		StringName team;
		int64_t summoner_instance_id;
	};

	/// Loadout, compiled effects, spawn snapshot, casting payload, and combat telemetry (updated on events, not inner targeting loops).
	struct UnitStateCold {
		struct DamageSourceEntry {
			double damage = 0.0;
			double last_time = 0.0;
		};
		StringName archetype_id;
		StringName role_id;
		Dictionary stats;
		std::array<std::vector<EffectRecord>, 10> passive_effects;
		double on_ally_defense_radius = 0.0;  // Radius for on_ally_defense triggers
		// Passive AOE radius information for visualization
		struct PassiveAoeInfo {
			StringName passive_id;
			double radius;
		};
		std::vector<PassiveAoeInfo> passive_aoe_info;
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
			StringName effect_type;  // e.g., "burn", "poison", "regen"
			double damage_total = 0.0;  // Total damage over full duration
			double heal_total = 0.0;  // Total heal over full duration
			double remaining_duration = 0.0;
			double tick_interval = 0.0;
			double tick_accumulator = 0.0;
			double original_tick_count = 0.0;  // Total ticks over original duration (duration / tick_interval)
			int64_t source_instance_id = 0;
			StringName damage_type;  // "physical", "magical", "true"
			StringName stacking_mode;  // "refresh", "extend", "separate"
			String reason;
			bool allow_overheal = false;
			int stack_count = 0;
			int max_stacks = 1;
			StringName action_kind;  // "auto", "ability", "ultimate", "passive"
			
			// Total ratio parameters for dynamic calculation mode
			double total_attack_damage_ratio = 0.0;
			double total_max_hp_ratio = 0.0;
			double total_current_hp_ratio = 0.0;
			double total_missing_hp_ratio = 0.0;
			double total_flat_amount = 0.0;
			StringName calculation_mode = StringName("fixed");  // "fixed" or "dynamic"
		};
		std::vector<PeriodicEffect> periodic_effects;

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
		StringName channel_target_mode;  // "fixed" or "dynamic"
		EffectRecord channel_sub_effect;
		EffectRecord channel_post_complete_effect;
		EffectRecord channel_post_interrupt_effect;
	};

	/// Per-tick combat and movement hot path; keep compact—cold lives in `UnitStateCold` at same index.
	struct UnitState {
		int64_t instance_id = 0;
		StringName team;
		int64_t role_slot = -1;
		int64_t summoner_instance_id = 0;  // Tracks which unit summoned this minion (0 if not a minion)
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
		/// Passive `reflect_damage` (on_defense): portions by damage-type applicability.
		double reflect_passive_pct_all = 0.0;
		double reflect_passive_pct_physical = 0.0;
		double reflect_buff_remaining = 0.0;
		double reflect_buff_pct_all = 0.0;
		double reflect_buff_pct_physical = 0.0;
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
		bool is_backliner = false;  // Cached backliner status for O(1) lookup
		
		// Stat modifier system - generated by X-Macro
#define X(name, def, min_val, max_val) \
		double stat_additive_##name = 0.0; \
		double stat_multiplicative_##name = 1.0; \
		double stat_temp_##name = 0.0; \
		double stat_perm_##name = 0.0;
		STAT_LIST
#undef X

		// Per-source stack tracking for stat modifiers
		Dictionary stat_stacks;  // key: "stat_name|reason", value: StackInfo
		Dictionary stat_modifiers;  // key: "stat_name|reason", value: simple modifier info
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
		/// Alive backliner count for team; reset in _prepare_tick_context, decremented in _handle_death (matches `other.alive` scans over backliner lists).
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
		bool use_spatial = false;
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

	// Inline getter functions (hot path) - generated by X-Macro
#define X(name, def, min_val, max_val) \
	static inline double get_effective_##name(const UnitState& unit) { \
		double base = (unit.combat.name + unit.stat_additive_##name) * unit.stat_multiplicative_##name; \
		if (min_val != NO_BOUND) { \
			base = Math::max(min_val, base); \
		} \
		if (max_val != NO_BOUND) { \
			base = Math::min(max_val, base); \
		} \
		return base; \
	}
	STAT_LIST
#undef X

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
		EFFECT_OPCODE_UNKNOWN = 0,
		EFFECT_OPCODE_MULTI_EFFECT = 1,
		EFFECT_OPCODE_DAMAGE = 2,
		EFFECT_OPCODE_PROJECTILE = 3,
		EFFECT_OPCODE_STUN = 4,
		EFFECT_OPCODE_SHIELD = 5,
		EFFECT_OPCODE_HEAL = 6,
		EFFECT_OPCODE_AOE_TAUNT = 9,
		EFFECT_OPCODE_AOE_DAMAGE = 10,
		EFFECT_OPCODE_DAMAGE_THRESHOLD_TRIGGER = 11,
		EFFECT_OPCODE_MANA_REGEN = 13,
		EFFECT_OPCODE_POST_DAMAGE_MANA_GAIN = 14,
		EFFECT_OPCODE_DAMAGE_BASED_HEAL = 15,
		EFFECT_OPCODE_MANA_RESTORE_ON_HIT = 16,
		EFFECT_OPCODE_DRAIN_TARGET_MANA_ON_HIT = 17,
		EFFECT_OPCODE_EVERY_N_ATTACKS_STUN = 18,
		EFFECT_OPCODE_SELF_DASH = 19,
		EFFECT_OPCODE_AUTO_DODGE = 20,
		EFFECT_OPCODE_CONSTANT_MULTIPLIER = 21,
		EFFECT_OPCODE_HP_THRESHOLD_DAMAGE_MULTIPLIER = 22,
		EFFECT_OPCODE_DISTANCE_THRESHOLD_MULTIPLIER = 23,
		// New effect opcodes
		EFFECT_OPCODE_SLOW = 25,
		EFFECT_OPCODE_ROOT = 26,
		EFFECT_OPCODE_SILENCE = 27,
		EFFECT_OPCODE_DISARM = 28,
		EFFECT_OPCODE_KNOCKBACK = 29,
		EFFECT_OPCODE_REFLECT = 30,
		EFFECT_OPCODE_AOE_SLOW = 31,
		EFFECT_OPCODE_AOE_ROOT = 32,
		EFFECT_OPCODE_AOE_SILENCE = 33,
		EFFECT_OPCODE_AOE_DISARM = 34,
		EFFECT_OPCODE_AOE_KNOCKBACK = 35,
		EFFECT_OPCODE_AOE_REFLECT = 36,
		EFFECT_OPCODE_AOE_STUN = 37,
		EFFECT_OPCODE_REFLECT_DAMAGE = 38,
		EFFECT_OPCODE_KNOCKBACK_SHIELD = 39,
		EFFECT_OPCODE_TARGET_STATUS_MULTIPLIER = 40,
		EFFECT_OPCODE_STAT_MODIFIER = 41,
		EFFECT_OPCODE_STEALTH = 42,
		EFFECT_OPCODE_DAMAGE_OVER_TIME = 43,
		EFFECT_OPCODE_HEAL_OVER_TIME = 44,
		EFFECT_OPCODE_AOE_DAMAGE_OVER_TIME = 45,
		EFFECT_OPCODE_AOE_HEAL_OVER_TIME = 46,
		EFFECT_OPCODE_MULTI_TARGET = 47,
		EFFECT_OPCODE_DAMAGE_BASED_SHIELD = 48,
		EFFECT_OPCODE_CONSUME_STACKS_DAMAGE = 49,
		EFFECT_OPCODE_CONSUME_STACKS_HEAL = 50,
		EFFECT_OPCODE_CONSUME_STACKS_SHIELD = 51,
		EFFECT_OPCODE_SET_STACKS = 52,
		EFFECT_OPCODE_CHANNEL = 53,
		EFFECT_OPCODE_REDIRECT_DAMAGE = 54,
		EFFECT_OPCODE_SUMMON_ALLY = 55,
	};

	static constexpr double MATCH_DURATION = 60.0;
	static constexpr double DEFAULT_TICK_RATE = 0.1;
	static constexpr int64_t SUDDEN_DEATH_MAX_TICKS = 10000;
	static constexpr double EPSILON = 0.000001;
	static constexpr double RESPAWN_TIME = 10.0;
	static constexpr double ASSIST_WINDOW = 5.0;
	static constexpr double RETARGET_INTERVAL = 0.5;
	static constexpr double SHIELD_DECAY_RATE = 0.1;
	static constexpr double OVERTIME_DAMAGE_BASE_RATE = 0.0001;
	//Increase rate currently unused.
	static constexpr double OVERTIME_DAMAGE_INCREASE_RATE = 0.0001;
	static constexpr double TARGET_SWITCH_LOCK_DURATION = 0.3;
	static constexpr double TARGET_STICKINESS_THRESHOLD = 5.0;
	static constexpr double STICKINESS_RETARGET_BONUS = 0.5;
	static constexpr double REGEN_TICK_INTERVAL = 1.0;
	static constexpr double CASTING_WINDUP = 0.5;
	static constexpr double RANGED_THRESHOLD = 1.0;
	static constexpr double MELEE_CONTACT_BUFFER = 0.1;
	static constexpr double RANGED_CONTACT_BUFFER = 0.1;
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
	/// Minimum effective slow multiplier so movement math stays stable at extreme slow percentages.
	static constexpr double SLOW_MOVEMENT_MULTIPLIER_MIN = 0.05;
	static constexpr double DRAFT_X_BASE = 0.9;
	// Positions are stored as IEEE 754 double for deterministic arithmetic.
	static constexpr double ALLY_CRITICAL_HP_RATIO = 0.35;
	static constexpr double REACTIVE_PEEL_BONUS = 25.0;
	static constexpr double ROLE_PRIORITY_GLOBAL_SCALE = 1.0;
	static constexpr double SCORE_HP_WEIGHT_SCALE = 10.0;
	static constexpr double SCORE_THREAT_WEIGHT_SCALE = 5.0;
	static constexpr double SCORE_DISTANCE_WEIGHT_SCALE = 10.0;
	static constexpr double SCORE_KITING_WEIGHT_SCALE = 1.5;
	static constexpr double DISTANCE_EXPONENT = 1.5;
	static constexpr double SPACING_EXPONENT = 1.5;
	static constexpr double THREAT_RESPONSE_RANGE_FALLOFF = 0.6;
	static constexpr double THREAT_BURST_THRESHOLD = 0.1;
	static constexpr double THREAT_BURST_MULTIPLIER = 5.0;
	static constexpr double THREAT_DECAY_DEFAULT = 2.0;
	static constexpr double TARGET_SWITCH_MARGIN = 2.0;

	// Random vertical spawning constants
	static constexpr double SPAWN_Y_MIN = 3.0;
	static constexpr double SPAWN_Y_MAX = 7.0;
	static constexpr double RESPAWN_Y_MIN = 3.0; // Same as initial spawn
	static constexpr double RESPAWN_Y_MAX = 7.0; // Same as initial spawn
	static constexpr double PLAYER_SPAWN_X_BASE = 1.0;
	static constexpr double ENEMY_SPAWN_X_BASE = 9.0;
	static constexpr double SPAWN_Y_STEP = 0.5;
	static constexpr double ASSASSIN_TANK_CONTEXT_PENALTY = 15.0;
	static constexpr double EXECUTE_BONUS_WEIGHT_DEFAULT = 20.0;
	static constexpr double PREY_INCOMING_TARGET_SCALE = 0.75;
	static constexpr double PREY_PERCEIVED_THREAT_SCALE = 0.35;
	static constexpr double PREY_FRONTLINE_SCALE = 0.0;
	static constexpr double AOE_DENSITY_RADIUS = 2.0;
	static constexpr double KITE_TARGET_WINDOW_MIN_FACTOR = 0.7;
	static constexpr double KITE_TARGET_WINDOW_MAX_FACTOR = 1.3;
	static constexpr double SWITCH_COMMIT_WINDOW_SECONDS = 0.18;
	static constexpr double SWITCH_COMMIT_WINDOW_SWING_FRACTION = 0.25;
	static constexpr double SUPPORT_PEEL_BOOST = 2.2;
	static constexpr double SUPPORT_PEEL_THREAT_THRESHOLD = 2.0;
	static constexpr double TARGET_EXECUTE_HP_RATIO = 0.25;
	static constexpr double IN_RANGE_BONUS_DEFAULT = 0.6;
	static constexpr double THREAT_RESPONSE_RANGE_DEFAULT = 0.0;
	static constexpr double UNIT_COLLISION_RADIUS = 0.15;

	// Data-driven role priority configurations
	// Tank role priorities include integrated tank penalty values
	static StrategyRolePriorities get_tank_role_priorities() {
		StrategyRolePriorities p;
		p.enemy_priorities[0] = {"assassin", -5.0};
		p.enemy_priorities[1] = {"fighter", -2.0};
		p.enemy_priorities[2] = {"tank", 0.4};
		p.ally_priorities[0] = {"marksman", -5.0};
		p.ally_priorities[1] = {"mage", -5.0};
		p.ally_priorities[2] = {"support", -3.0};
		return p;
	}

	static StrategyRolePriorities get_assassin_role_priorities() {
		StrategyRolePriorities p;
		p.enemy_priorities[0] = {"marksman", -15.0};
		p.enemy_priorities[1] = {"mage", -15.0};
		p.enemy_priorities[2] = {"support", -10.0};
		p.enemy_priorities[3] = {"fighter", 10.0};
		p.enemy_priorities[4] = {"tank", 20.0};
		return p;
	}

	static StrategyRolePriorities get_fighter_role_priorities() {
		StrategyRolePriorities p;
		p.enemy_priorities[0] = {"marksman", -1.0};
		p.enemy_priorities[1] = {"mage", -1.0};
		p.enemy_priorities[2] = {"tank", 1.0};
		return p;
	}

	static StrategyRolePriorities get_marksman_role_priorities() {
		StrategyRolePriorities p;
		p.enemy_priorities[0] = {"tank", 1};
		return p;
	}

	static StrategyRolePriorities get_mage_role_priorities() {
		StrategyRolePriorities p;
		p.enemy_priorities[0] = {"marksman", -4.0};
		p.enemy_priorities[1] = {"support", -2.0};
		p.enemy_priorities[2] = {"tank", 2};
		return p;
	}

	static StrategyRolePriorities get_support_role_priorities() {
		StrategyRolePriorities p;
		p.enemy_priorities[0] = {"assassin", -8.0};
		p.enemy_priorities[1] = {"fighter", -4.0};
		p.enemy_priorities[2] = {"tank", 1.5};
		p.ally_priorities[0] = {"marksman", -5.0};
		p.ally_priorities[1] = {"mage", -5.0};
		p.ally_priorities[2] = {"fighter", -2.0};
		return p;
	}

	// Comprehensive strategy configuration getters
	static StrategyConfig get_tank_config() {
		StrategyConfig c;
		c.display_name = "Protector";
		c.role_name = "tank";
		c.distance_weight = 1.5;
		c.hp_weight = 0.0;
		c.ally_distance_weight = 1.0;
		c.ally_hp_weight = 0.0;
		c.ally_threat_weight = 25.0;
		c.in_range_bonus = 1.0;
		c.threat_response_weight = 2.0;
		c.execute_bonus_weight = 0.0;
		c.spacing_weight = 0.0;
		c.threat_decay_rate = 4.0;
		c.switch_margin = 2.0;
		c.prefers_kiting = false;
		c.uses_ally_targeting = true;
		c.role_priorities = get_tank_role_priorities();
		return c;
	}

	static StrategyConfig get_assassin_config() {
		StrategyConfig c;
		c.display_name = "Diver";
		c.role_name = "assassin";
		c.distance_weight = 0.01;
		c.hp_weight = 2.0;
		c.ally_distance_weight = 1.0;
		c.ally_hp_weight = 0.0;
		c.ally_threat_weight = 0.0;
		c.in_range_bonus = 0.6;
		c.threat_response_weight = 0.8;
		c.execute_bonus_weight = 10.0;
		c.spacing_weight = 0.0;
		c.threat_decay_rate = 1.0;
		c.switch_margin = 2.0;
		c.prefers_kiting = false;
		c.uses_ally_targeting = false;
		c.role_priorities = get_assassin_role_priorities();
		return c;
	}

	static StrategyConfig get_fighter_config() {
		StrategyConfig c;
		c.display_name = "Brawler";
		c.role_name = "fighter";
		c.distance_weight = 3.0;
		c.hp_weight = 0.5;
		c.ally_distance_weight = 1.0;
		c.ally_hp_weight = 0.0;
		c.ally_threat_weight = 0.0;
		c.in_range_bonus = 0.9;
		c.threat_response_weight = 1.8;
		c.execute_bonus_weight = 20.0;
		c.spacing_weight = 0.0;
		c.threat_decay_rate = 4.5;
		c.switch_margin = 2.0;
		c.prefers_kiting = false;
		c.uses_ally_targeting = false;
		c.role_priorities = get_fighter_role_priorities();
		return c;
	}

	static StrategyConfig get_marksman_config() {
		StrategyConfig c;
		c.display_name = "Kiter";
		c.role_name = "marksman";
		c.distance_weight = 1.0;
		c.hp_weight = 0.5;
		c.ally_distance_weight = 1.0;
		c.ally_hp_weight = 0.0;
		c.ally_threat_weight = 0.0;
		c.in_range_bonus = 0.35;
		c.threat_response_weight = 61.0;
		c.execute_bonus_weight = 20.0;
		c.spacing_weight = 2.5;
		c.threat_decay_rate = 1.0;
		c.switch_margin = 2.0;
		c.prefers_kiting = true;
		c.uses_ally_targeting = false;
		c.role_priorities = get_marksman_role_priorities();
		return c;
	}

	static StrategyConfig get_mage_config() {
		StrategyConfig c;
		c.display_name = "Spellcaster";
		c.role_name = "mage";
		c.distance_weight = 1.2;
		c.hp_weight = 2.5;
		c.ally_distance_weight = 1.0;
		c.ally_hp_weight = 0.0;
		c.ally_threat_weight = 0.0;
		c.in_range_bonus = 0.45;
		c.threat_response_weight = 61.1;
		c.execute_bonus_weight = 20.0;
		c.spacing_weight = 1.5;
		c.threat_decay_rate = 1.0;
		c.switch_margin = 2.0;
		c.prefers_kiting = true;
		c.uses_ally_targeting = false;
		c.role_priorities = get_mage_role_priorities();
		return c;
	}

	static StrategyConfig get_support_config() {
		StrategyConfig c;
		c.display_name = "Enchanter";
		c.role_name = "support";
		c.distance_weight = 1.5;
		c.hp_weight = 0.0;
		c.ally_distance_weight = 1.0;
		c.ally_hp_weight = 10.0;
		c.ally_threat_weight = 25.0;
		c.in_range_bonus = 0.4;
		c.threat_response_weight = 1.7;
		c.execute_bonus_weight = 0.0;
		c.spacing_weight = 1.0;
		c.threat_decay_rate = 1.0;
		c.switch_margin = 2.0;
		c.prefers_kiting = true;
		c.uses_ally_targeting = true;
		c.role_priorities = get_support_role_priorities();
		return c;
	}
	static constexpr int SPATIAL_GRID_DIM = 8;
	/// Broad-phase for targeting/density/kite only when a team has this many **alive** units (4+). Standard 5v5 can enter the spatial path under the current benchmark contract.
	static constexpr int SPATIAL_BROAD_PHASE_TEAM_THRESHOLD = 4;
	/// Separation ally scan uses a grid only at this team alive count or above (custom large teams); 5v5 uses brute O(n) with tiny n.
	static constexpr int SPATIAL_SEPARATION_TEAM_THRESHOLD = 6;

	static constexpr const char *CHAMPION_SCHEMA_PATH = "res://fixtures/goldens/champion_schema.json";
	static constexpr const char *MINION_SCHEMA_PATH = "res://fixtures/goldens/minion_schema.json";
	static constexpr const char *BALANCE_PATCHES_PATH = "res://fixtures/goldens/balance_patches.json";
	static constexpr const char *CHAMPION_KITS_PATH = "res://fixtures/goldens/champion_kits.json";

	std::vector<UnitState> _units;
	/// Parallel to `_units` (same length and indices). Push or clear together with `_units` only.
	/// `_uc(u)` is only valid when `u` references an element stored in `_units` (never a stack copy of `UnitState`).
	std::vector<UnitStateCold> _unit_cold;
	std::vector<ProjectileState> _projectiles;
	std::vector<ProjectileState> _scratch_projectiles;
	std::vector<ProjectileState> _active_projectiles;
	std::vector<int64_t> _scratch_critical_allies;
	Array _summary_unit_stats;
	Dictionary _summary_cache;
	CPythonRandom _rng;
	double _time = 0.0;
	int64_t _tick = 0;
	int64_t _sudden_death_ticks = 0;
	double _tick_rate = DEFAULT_TICK_RATE;
	int64_t _seed = 0;
	StringName _winner_team = StringName("draw");
	bool _record_events = false;
	Array _player_comp;
	Array _enemy_comp;
	int64_t _player_kills = 0;
	int64_t _enemy_kills = 0;
	int64_t _max_instance_id = 0;
	
	// Spawn slot tracking per team (indices into spawn_points array)
	std::vector<bool> _player_spawn_slots_used;
	std::vector<bool> _enemy_spawn_slots_used;
	Dictionary _champion_catalog;
	Dictionary _minion_catalog;
	Dictionary _role_configs;
	std::vector<BalancePatch> _balance_patches;
	Dictionary _ability_kits; // "kit_id" -> kit Dictionary (ability, ultimate, passive_ids) - loaded from champion_kits.json
	Dictionary _effective_champion_by_archetype; // key: String archetype_id, value: effective champion Dictionary
	Dictionary _passive_registry;
	std::unordered_map<int64_t, int64_t> _unit_index_map;
	std::vector<int64_t> _alive_player_indices;
	std::vector<int64_t> _alive_enemy_indices;
	std::unordered_set<int64_t> _alive_player_indices_set;
	std::unordered_set<int64_t> _alive_enemy_indices_set;
	std::vector<TargetingFrameEntry> _targeting_frame;
	bool _catalog_loaded = false;
	std::array<UnitStrategy, ROLE_SLOT_COUNT> _role_strategy_cache_by_slot{};
	UnitStrategy _default_strategy;
	TickContext _tick_ctx;
	std::vector<TraceEvent> _trace_buffer;
	static constexpr size_t TRACE_BUFFER_CAP = 4096;
	bool _debug_combat_trace = false;
	bool _debug_targeting_scoring = false;

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
	static constexpr size_t VIEWER_FX_CAP = 256;
	std::vector<ViewerFxEvent> _viewer_fx_events;

	void _viewer_fx_push(const ViewerFxEvent &p_ev);
	void _viewer_record_damage_fx(const UnitState &p_source, const UnitState &p_target, double p_total_damage, const StringName &p_action_kind, const StringName &p_damage_type);
	void _viewer_record_heal_fx(const UnitState &p_target, double p_amount);
	void _viewer_record_shield_fx(const UnitState &p_target, double p_amount);
	void _viewer_record_aoe_ring_fx(const UnitState &p_source, const UnitState &p_center, double p_radius, const StringName &p_kind);
	void _viewer_record_aoe_shape_fx(const UnitState &p_source, const UnitState *p_target, const AoShapeParams &params, const StringName &kind);
	void _viewer_record_hot_status_fx(const UnitState &p_target, double p_duration, const StringName &p_effect_type);
	void _viewer_record_passive_aoe_fx(const UnitState &p_unit, double p_radius, const StringName &p_passive_id);
	String _viewer_state_string(const UnitState &p_u) const;
	void _print_score_breakdown(const ScoreBreakdown &breakdown, const StringName &attacker_archetype, const StringName &enemy_archetype) const;

	mutable std::array<std::vector<int64_t>, SPATIAL_GRID_DIM * SPATIAL_GRID_DIM> _spatial_buckets;
	mutable std::vector<uint32_t> _spatial_stamp;
	mutable uint32_t _spatial_generation = 1;

	/// TEAMFIGHT_SIM_PROFILE env: per-_simulate() wall time (nanoseconds) by _step_tick section.
	uint64_t _sim_profile_ns_projectiles = 0;
	uint64_t _sim_profile_ns_prepare_tick_ctx = 0;
	uint64_t _sim_profile_ns_refresh_pressure_pre = 0;
	uint64_t _sim_profile_ns_update_units = 0;
	uint64_t _sim_profile_ns_refresh_pressure_post = 0;
	int64_t _sim_profile_tick_count = 0;
	/// Sub-timers for _update_unit (summed over all units each tick; same divisor as tick_count).
	uint64_t _sim_profile_uu_dead_respawn = 0;
	uint64_t _sim_profile_uu_cooldowns_cc = 0;
	uint64_t _sim_profile_uu_separation = 0;
	uint64_t _sim_profile_uu_threat_and_assist = 0;
	uint64_t _sim_profile_uu_regen_on_tick = 0;
	uint64_t _sim_profile_uu_casting = 0;
	uint64_t _sim_profile_uu_targeting = 0;
	uint64_t _sim_profile_uu_combat = 0;
	uint64_t _sim_profile_uu_movement = 0;
	/// Sub-timers inside `_score_enemy_target` (summed over calls); use with `_sim_profile_se_calls` for per-call avg.
	uint64_t _sim_profile_se_base = 0;
	int64_t _sim_profile_se_calls = 0;
	/// Sub-timers for uu_combat
	uint64_t _sim_profile_uc_attack_cooldown = 0;
	uint64_t _sim_profile_uc_distance_calc = 0;
	uint64_t _sim_profile_uc_hit_validation = 0;
	uint64_t _sim_profile_uc_damage_apply = 0;
	uint64_t _sim_profile_uc_auto_attack = 0;
	uint64_t _sim_profile_uc_ability = 0;
	/// Sub-timers for uu_cooldowns_cc
	uint64_t _sim_profile_ucc_attack_cd = 0;
	uint64_t _sim_profile_ucc_ability_cd = 0;
	uint64_t _sim_profile_ucc_retarget = 0;
	uint64_t _sim_profile_ucc_target_switch = 0;
	uint64_t _sim_profile_ucc_stun = 0;
	uint64_t _sim_profile_ucc_slow = 0;
	uint64_t _sim_profile_ucc_root = 0;
	uint64_t _sim_profile_ucc_silence = 0;
	uint64_t _sim_profile_ucc_disarm = 0;
	uint64_t _sim_profile_ucc_stealth = 0;
	uint64_t _sim_profile_ucc_shield = 0;
	uint64_t _sim_profile_ucc_reflect = 0;
	uint64_t _sim_profile_ucc_taunt = 0;
	uint64_t _sim_profile_ucc_forced_target = 0;
	/// Sub-timers for ns_prepare_tick_ctx
	uint64_t _sim_profile_ctx_team_centers = 0;
	uint64_t _sim_profile_ctx_role_classification = 0;
	uint64_t _sim_profile_ctx_targeting_sync = 0;
	uint64_t _sim_profile_ctx_spatial_grid = 0;
	uint64_t _sim_profile_ctx_density = 0;
	/// Sub-timers for uu_movement
	uint64_t _sim_profile_um_kiting = 0;
	uint64_t _sim_profile_um_kiting_spatial = 0;
	uint64_t _sim_profile_um_kiting_brute = 0;
	uint64_t _sim_profile_um_toward = 0;
	uint64_t _sim_profile_um_boundary = 0;
	uint64_t _sim_profile_um_nudge = 0;
	/// Sub-timers for uu_regen_on_tick
	uint64_t _sim_profile_ur_hp_mana = 0;
	uint64_t _sim_profile_ur_effects = 0;
	uint64_t _sim_profile_ur_periodic = 0;
	uint64_t _sim_profile_ur_channel = 0;
	bool _sim_profile_active = false;
	bool _sim_profile_targeting_active = false;
	int64_t _sim_profile_tgt_retarget_keeps = 0;
	int64_t _sim_profile_tgt_enemy_scans = 0;
	int64_t _sim_profile_tgt_candidates_scored = 0;
	int64_t _sim_profile_tgt_candidates_prefix_pruned = 0;
	int64_t _sim_profile_tgt_ally_scans = 0;
	int64_t _sim_profile_tgt_frame_syncs = 0;
	int64_t _sim_profile_tgt_ties_adjusted = 0;
	int64_t _sim_profile_tgt_ties_raw = 0;
	int64_t _sim_profile_tgt_ties_distance = 0;
	int64_t _sim_profile_tgt_ties_instance = 0;

	static bool _sim_profile_env_enabled();
	void _sim_profile_reset();
	void _sim_profile_emit_json_stderr() const;

	void _reset_runtime_state();
	double _randf();
	Dictionary _load_json_file(const String &path) const;
	Dictionary _load_json_file_if_exists(const String &path) const;
	Dictionary _effect_to_dict(const Variant &effect) const;
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
	static const StringName &_kind_for_opcode(int64_t opcode);
	EffectRecord _compile_effect(const Dictionary &effect) const;
	std::vector<EffectRecord> _compile_effect_array(const Array &effects) const;
	Dictionary _coerce_match_input(const Variant &match_input) const;
	void _populate_runtime_state(const Dictionary &match_input);
	void _append_team_units(const Array &spawn_specs, const StringName &team, int64_t &next_instance_id, Array &team_comp);
	std::pair<UnitState, UnitStateCold> _build_unit_state(const Dictionary &spawn_spec, const StringName &team, int64_t instance_id);
	TargetingFrameEntry _make_targeting_frame_entry(const UnitState &unit) const;
	void _sync_targeting_frame_index(int64_t index, const UnitState &unit);
	void _sync_targeting_frame_unit(const UnitState &unit);
	UnitStateCold &_uc(UnitState &u);
	const UnitStateCold &_uc(const UnitState &u) const;
	UnitState *_unit_by_id(int64_t instance_id);
	const UnitState *_unit_by_id(int64_t instance_id) const;
	int64_t _unit_index_by_id(int64_t instance_id) const;
	int64_t _target_index_for_unit(UnitState &unit);
	void _set_current_target(UnitState &unit, const UnitState &target);
	std::vector<int64_t> &_alive_indices_for_team(const StringName &team);
	const std::vector<int64_t> &_alive_indices_for_team(const StringName &team) const;
	void _add_alive_index(const StringName &team, int64_t index);
	void _remove_alive_index(const StringName &team, int64_t index);
	void _refresh_target_pressure();
	void _prepare_tick_context();
	void _build_role_strategy_cache();
	const UnitStrategy &_strategy_for_unit(const UnitState &unit) const;
	void _emit_trace(const StringName &kind, int64_t src_id, int64_t tgt_id, double val);
	void _adjust_target_pressure(int64_t old_target_id, int64_t new_target_id);
	void _simulate();
	void _step_tick(bool profile_sim);
	void _update_unit(UnitState &unit, bool profile_sim);
	void _prune_assist_window(UnitState &unit);
	void _update_projectiles();
	bool _kite_from_enemies(UnitState &unit, bool profile_sim = false);
	double _score_enemy_target(const UnitState &attacker, const TargetingFrameEntry &enemy, const TargetingFrameEntry *ally_for_peel, const UnitStrategy &strategy, const TickContext &ctx, const TargetScoreContext &score_ctx, double attacker_enemy_distance = -1.0, bool profile_score = false, int64_t enemy_index = -1, ScoreBreakdown *breakdown = nullptr);
	/// When `unit_ally_distance` is >= 0, used as the unit–ally distance (avoids a duplicate sqrt vs `_distance_between`).
	double _score_ally_target(const UnitState &unit, const TargetingFrameEntry &ally, const TeamfightSimulationCore::UnitStrategy &strategy, double unit_ally_distance = -1.0) const;
	/// When `current_target_distance` is >= 0, used instead of recomputing distance to `unit.target_id` for commit-window logic.
	bool _should_switch(const UnitState &unit, double current_score, double new_score, const UnitStrategy &strategy, double current_target_distance = -1.0) const;
	bool _try_cast_ability(UnitState &unit, UnitState &target, double distance);
	bool _try_cast_ultimate(UnitState &unit, UnitState &target, double distance);
	bool _start_cast(UnitState &unit, UnitState &target, double distance, const StringName &action_kind);
	void _resolve_cast(UnitState &unit);
	void _perform_auto_attack(UnitState &unit, UnitState &target, double distance);
	void _move_toward_target(UnitState &unit, UnitState &target);
	void _move_toward_target_with_range(UnitState &unit, UnitState &target, double target_range);
	void _resolve_projectile(const ProjectileState &projectile);
	EffectContext _build_context(UnitState &source, UnitState *target, UnitState *target_ally, double damage, const StringName &action_kind);
	int _effect_bucket_index(const StringName &kind) const;
	const std::vector<EffectRecord> &_collect_effects(const UnitState &unit, const StringName &kind);
	double _apply_attack_modifiers(UnitState &unit, UnitState &target, double distance, double damage);
	double _apply_ability_modifiers(UnitState &unit, UnitState *target, double damage);
	double _apply_ultimate_modifiers(UnitState &unit, UnitState *target, double damage);
	double _apply_damage(UnitState &source, UnitState &target, double damage, const StringName &damage_type, const StringName &action_kind, const EffectContext &context);
	double _defense_multiplier(UnitState &target, UnitState &source, double damage, const StringName &action_kind);
	double _auto_dodge_multiplier(UnitState &target, UnitState &source, double damage);
	double _damage_type_multiplier(const UnitState &target, const StringName &damage_type);
	double _evaluate_multiplier_effect(const EffectRecord &effect, const EffectContext &context, double current_value);
	Dictionary _execute_effect(const EffectRecord &effect, EffectContext &context);
	void _merge_accumulated_results(Dictionary &target, const Dictionary &source);
	bool _check_condition(const EffectRecord &effect, const Dictionary &results);
	bool _check_target_status_condition(const EffectRecord &effect, const EffectContext &context);
	bool _check_all_conditions(const EffectRecord &effect, const Dictionary &results, const EffectContext &context);
	void _merge_result(Dictionary &target_result, const Dictionary &source_result);
	bool _target_has_status(const UnitState &target, const StringName &status_kind) const;
	bool _effect_record_contains_opcode(const EffectRecord &effect, EffectOpcode opcode) const;
	void _finalize_reflect_passives(UnitState &unit, UnitStateCold &cold);
	void _apply_reflect_buff(UnitState &unit, double pct, double duration, bool all_damage_types);
	void _apply_aoe_reflect(UnitState &source, double radius, double pct, double duration, bool all_damage_types);
	void _maybe_apply_reflect_damage(UnitState &attacker, UnitState &defender, double total_damage_applied, const StringName &damage_type, const EffectContext &context);
	bool _apply_knockback(UnitState &source, UnitState &target, double distance, bool away_from_source);
	bool _apply_aoe_knockback(UnitState &source, double radius, double distance, bool away_from_source);
	void _run_post_attack_effects(UnitState &source, UnitState &target, double damage, const EffectContext &context);
	void _run_post_heal_effects(UnitState &source, UnitState &target, double heal_amount, double heal_gained, const StringName &action_kind, const EffectContext &base_context);
	void _run_on_takedown_effects(UnitState &participant, UnitState &victim, double damage_dealt, bool is_kill, const StringName &action_kind, const EffectContext &base_context);
	void _apply_stun(UnitState &source, UnitState &target, double duration);
	void _apply_slow(UnitState &source, UnitState &target, double slow_percentage, double duration);
	void _apply_root(UnitState &source, UnitState &target, double duration);
	void _apply_silence(UnitState &source, UnitState &target, double duration, bool block_abilities, bool block_ultimate);
	void _apply_disarm(UnitState &source, UnitState &target, double duration);
	void _apply_stealth(UnitState &source, UnitState &target, double duration, bool break_on_attack, bool break_on_ability, bool break_on_damage_taken);
	void _apply_aoe_slow(UnitState &source, double radius, double slow_percentage, double duration);
	void _apply_aoe_slow_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double slow_percentage, double duration);
	void _apply_aoe_root(UnitState &source, double radius, double duration);
	void _apply_aoe_root_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration);
	void _apply_aoe_silence(UnitState &source, double radius, double duration, bool block_abilities, bool block_ultimate);
	void _apply_aoe_silence_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration, bool block_abilities, bool block_ultimate);
	void _apply_aoe_disarm(UnitState &source, double radius, double duration);
	void _apply_aoe_disarm_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration);
	void _apply_aoe_stun(UnitState &source, double radius, double duration);
	void _apply_aoe_stun_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration);
	bool _apply_aoe_knockback_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double distance, bool away_from_source);
	void _apply_aoe_reflect_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double pct, double duration, bool all_damage_types);
	std::vector<UnitState*> _select_targets(UnitState &source, UnitState *target, int64_t target_count, TargetSelectionStrategy strategy, bool include_source, ExcessTargetHandling excess_handling, const StringName &team_filter);
	double _movement_speed_multiplier(const UnitState &unit) const;
	void _touch_damage_source(UnitState &target, int64_t source_id, double incoming_damage);
	double _trigger_ally_defense_effects(UnitState &target, UnitState &source, double damage, const StringName &damage_type, const StringName &action_kind, const EffectContext &context);
	double _distance_between(const UnitState &unit1, const UnitState &unit2) const;
	double _attack_range(const UnitState &unit) const;
	double _effective_attack_range(const UnitState &unit) const;
	int64_t _assign_spawn_slot(const StringName &team);
	Vector2 _get_random_spawn_position(const StringName &team, bool is_respawn);
	Vector2 _find_random_spawn_position_near(double center_x, double center_y, double radius);
	Vector2 _find_random_spawn_position_near_excluding(double center_x, double center_y, double radius, int64_t exclude_instance_id);
	Vector2 _find_random_spawn_position_near_excluding_with_expansion(double center_x, double center_y, double initial_radius, double max_radius, int64_t exclude_instance_id, const std::vector<Vector2> &pending_positions = std::vector<Vector2>());
	bool _position_collides_with_pending(double x, double y, const std::vector<Vector2> &pending_positions) const;
	void _add_shield(UnitState &source, UnitState &target, double amount, const StringName &action_kind);
	void _heal_unit(UnitState &source, UnitState &target, double amount, const StringName &action_kind, bool allow_overheal = false);
	void _restore_mana(UnitState &source, UnitState &target, double amount);
	void _apply_stat_modifier(UnitState &source, UnitState &target, StringName stat_name, double additive, double multiplicative, double duration, bool is_match_duration);
	void _apply_simple_stat_modifier(UnitState &source, UnitState &target, StringName stat_name, double additive, double multiplicative, double duration, bool is_match_duration, const String &reason);
	void _set_stat_modifier_duration(UnitState &unit, StringName stat_name, double duration, bool is_match_duration);
	void _apply_stacked_stat_modifier(UnitState &source, UnitState &target, StringName stat_name, double additive, double multiplicative, double duration, bool is_match_duration, int max_stacks, StackBehavior stack_behavior, const String &reason);
	void _clear_all_stat_modifiers(UnitState &unit);
	void _update_stat_modifier_durations(UnitState &unit, double delta);
	void _clear_expired_stat_modifiers(UnitState &unit);
	void _handle_death(UnitState &killer, UnitState &target);
	Vector2 _find_valid_dash_position(double tx, double ty, double new_x, double new_y, double effective_distance, int64_t target_instance_id) const;
	void _tick_periodic_effects(UnitState &unit, double delta);
	void _cleanse_dots(UnitState &unit, const StringName &effect_type_filter);
	void _clear_periodic_effects(UnitState &unit);
	UnitState *_select_enemy_target(UnitState &unit, bool profile_sim);
	UnitState *_select_ally_target(UnitState &unit);
	bool _position_collides_with_unit(double x, double y, int64_t exclude_instance_id) const;
	void _release_spawn_slot(const StringName &team, int64_t slot_index);
	StringName _determine_winner() const;
	void _respawn_unit(UnitState &unit);
	Dictionary _build_summary();
	Dictionary _build_stats_summary();
	
	// Pending spawns queue for mid-tick unit creation
	std::vector<PendingSpawn> _pending_spawns;
	
	// Pending spawns processing
	void _process_pending_spawns();

	// Stack management functions
	void _reset_stat_temp_tracker(UnitState &unit, StringName stat_name);
	String _get_stack_key(StringName stat_name, const String &reason);
	void _update_stacks(UnitState &unit, double delta, double current_time);
	void _cleanup_expired_stacks(UnitState &unit, double current_time);
	bool _is_valid_stat_name(const StringName &stat_name) const;
	int _consume_stat_stacks(UnitState &unit, StringName stat_name, String reason);
	void _set_stat_stacks(UnitState &unit, StringName stat_name, String reason, int stack_count, double duration, bool to_max, int fallback_max_stacks, double fallback_additive_per_stack, double fallback_multiplicative_per_stack);
	
	// Stack debugging functions
	void _debug_print_stack_state(const UnitState &unit) const;
	String _join_team_names(const Array &team) const;
	void _apply_aoe_taunt(UnitState &source, double radius, double duration);
	void _apply_aoe_taunt_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double duration);
	double _apply_aoe_damage(UnitState &source, UnitState &center_source, double damage, double radius, const StringName &damage_type, const StringName &reason, const StringName &action_kind);
	double _apply_aoe_damage_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double damage, const StringName &damage_type, const StringName &action_kind);
	void _apply_dot(UnitState &source, UnitState &target, double attack_damage_ratio, double max_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &damage_type, const StringName &stacking_mode, int max_stacks, const StringName &effect_type, const String &reason, const StringName &action_kind, bool is_dynamic = false);
	void _apply_hot(UnitState &source, UnitState &target, double max_hp_ratio, double current_hp_ratio, double missing_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &stacking_mode, int max_stacks, bool allow_overheal, const StringName &effect_type, const String &reason, const StringName &action_kind, bool is_dynamic = false);
	void _apply_aoe_dot(UnitState &source, double radius, double attack_damage_ratio, double max_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &damage_type, const StringName &stacking_mode, int max_stacks, const StringName &effect_type, const String &reason, bool target_self, const StringName &action_kind, bool is_dynamic = false);
	void _apply_aoe_dot_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double attack_damage_ratio, double max_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &damage_type, const StringName &stacking_mode, int max_stacks, const StringName &effect_type, const String &reason, bool target_self, const StringName &action_kind, bool is_dynamic = false);
	void _apply_aoe_hot(UnitState &source, double radius, double max_hp_ratio, double current_hp_ratio, double missing_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &stacking_mode, int max_stacks, bool allow_overheal, const StringName &effect_type, const String &reason, bool target_self, const StringName &action_kind, bool is_dynamic = false);
	void _apply_aoe_hot_shape(UnitState &source, UnitState *target, const EffectRecord &effect, double max_hp_ratio, double current_hp_ratio, double missing_hp_ratio, double flat_amount, double duration, double tick_interval, const StringName &stacking_mode, int max_stacks, bool allow_overheal, const StringName &effect_type, const String &reason, bool target_self, const StringName &action_kind, bool is_dynamic = false);

	// Channel effect functions
	void _process_channel_tick(UnitState &unit, double delta);
	bool _should_interrupt_channel(UnitState &unit, const UnitStateCold &cold);
	void _complete_channel(UnitState &unit, UnitStateCold &cold);
	void _interrupt_channel(UnitState &unit, UnitStateCold &cold);
	void _clear_channel_state(UnitStateCold &cold);
	int64_t _get_channel_tick_count(const UnitStateCold &cold);

	Dictionary _champion_for(const StringName &archetype_id) const;

	Vector2 _resolve_aoe_direction(const UnitState &source, const AoShapeParams &params, const UnitState *target_override = nullptr) const;
	AoShapeParams _parse_aoe_shape_metadata(const Dictionary &params) const;

	/// Parameters for circular AoE iteration over an alive-team index list (`_alive_*_indices`).
	/// `spatial_team` must match `UnitState::team` for units referenced by `indices` (used by broad-phase stamp).
	struct AoCircleIterationParams {
		double center_x = 0.0;
		double center_y = 0.0;
		double radius = 0.0;
		const std::vector<int64_t> *indices = nullptr;
		StringName spatial_team;
		int64_t exclude_instance_id = 0;
	};

	/// Parameters for universal AoE shape iteration over an alive-team index list.
	struct AoShapeIterationParams {
		AoShapeParams shape_params;
		const std::vector<int64_t> *indices = nullptr;
		StringName spatial_team;
		int64_t exclude_instance_id = 0;
		const UnitState *source = nullptr;
		const UnitState *target_override = nullptr;
	};

	/// Inclusive disk: `dist_sq <= radius²`. Uses spatial broad-phase when enabled (threshold on alive counts).
	template<typename Fn>
	void _for_each_unit_in_circle(const AoCircleIterationParams &p, Fn &&fn) {
		if (p.radius <= 0.0 || p.indices == nullptr) {
			return;
		}
		const double r2 = p.radius * p.radius;
		const std::vector<int64_t> indices = *p.indices;
		auto visit = [&](int64_t idx) {
			if (idx < 0 || idx >= int64_t(_units.size())) {
				return;
			}
			UnitState &u = _units[static_cast<size_t>(idx)];
			if (!u.alive) {
				return;
			}
			if (p.exclude_instance_id != 0 && u.instance_id == p.exclude_instance_id) {
				return;
			}
			const double dx = u.pos_x - p.center_x;
			const double dy = u.pos_y - p.center_y;
			if (dx * dx + dy * dy <= r2) {
				fn(u);
			}
		};
		if (!_use_spatial_broad_phase()) {
			for (int64_t idx : indices) {
				visit(idx);
			}
			return;
		}
		_spatial_fill_buckets_for_indices(indices);
		_spatial_stamp_circle(p.center_x, p.center_y, p.radius, p.spatial_team);
		for (int64_t idx : indices) {
			if (!_spatial_stamp_has(idx)) {
				continue;
			}
			visit(idx);
		}
	}

	/// Universal AoE shape iteration: supports circle, cone, rectangle, line.
	/// Uses spatial broad-phase when enabled (threshold on alive counts).
	template<typename Fn>
	void _for_each_unit_in_shape(const AoShapeIterationParams &p, Fn &&fn) {
		if (p.indices == nullptr) {
			return;
		}
		const std::vector<int64_t> indices = *p.indices;
		
		// Resolve anchor position
		double center_x = 0.0;
		double center_y = 0.0;
		if (p.shape_params.anchor == AoAnchorKind::Self && p.source != nullptr) {
			center_x = p.source->pos_x;
			center_y = p.source->pos_y;
		} else if (p.shape_params.anchor == AoAnchorKind::Target && p.target_override != nullptr) {
			center_x = p.target_override->pos_x;
			center_y = p.target_override->pos_y;
		} else if (p.shape_params.anchor == AoAnchorKind::Point) {
			center_x = p.shape_params.anchor_x;
			center_y = p.shape_params.anchor_y;
		} else if (p.shape_params.anchor == AoAnchorKind::Forward && p.source != nullptr) {
			// Calculate forward position from champion center
			Vector2 direction = _resolve_aoe_direction(*p.source, p.shape_params, p.target_override);
			if (p.shape_params.shape == AoShapeKind::Rectangle) {
				center_x = p.source->pos_x + direction.x * p.shape_params.height * 0.5;
				center_y = p.source->pos_y + direction.y * p.shape_params.height * 0.5;
			} else if (p.shape_params.shape == AoShapeKind::Cone) {
				// Cone apex at source, no offset needed (cone extends forward from apex)
				center_x = p.source->pos_x;
				center_y = p.source->pos_y;
			} else if (p.shape_params.shape == AoShapeKind::Circle) {
				center_x = p.source->pos_x + direction.x * p.shape_params.radius * 0.5;
				center_y = p.source->pos_y + direction.y * p.shape_params.radius * 0.5;
			} else {
				center_x = p.source->pos_x;
				center_y = p.source->pos_y;
			}
		} else if (p.source != nullptr) {
			center_x = p.source->pos_x;
			center_y = p.source->pos_y;
		}
		
		// Resolve direction
		Vector2 forward(1.0, 0.0);
		if (p.source != nullptr) {
			forward = _resolve_aoe_direction(*p.source, p.shape_params, p.target_override);
		}
		
		// Shape-specific filtering
		auto shape_contains = [&](const UnitState &u) -> bool {
			const double dx = u.pos_x - center_x;
			const double dy = u.pos_y - center_y;
			
			switch (p.shape_params.shape) {
				case AoShapeKind::Circle: {
					const double r2 = p.shape_params.radius * p.shape_params.radius;
					return dx * dx + dy * dy <= r2;
				}
				case AoShapeKind::Cone: {
					const double r2 = p.shape_params.radius * p.shape_params.radius;
					if (dx * dx + dy * dy > r2) {
						return false;
					}
					const double half_angle = p.shape_params.width * 0.5;
					Vector2 to_unit(dx, dy);
					if (to_unit.length_squared() < EPSILON * EPSILON) {
						return true;
					}
					to_unit = to_unit.normalized();
					const double dot = to_unit.dot(forward);
					const double angle_cos = Math::cos(half_angle);
					return dot >= angle_cos;
				}
				case AoShapeKind::Rectangle: {
					const double half_w = p.shape_params.width * 0.5;
					const double half_h = p.shape_params.height * 0.5;
					Vector2 to_unit(dx, dy);
					Vector2 right = Vector2(-forward.y, forward.x);
					const double forward_dist = to_unit.dot(forward);
					const double right_dist = to_unit.dot(right);
					return Math::abs(forward_dist) <= half_h && Math::abs(right_dist) <= half_w;
				}
				default:
					return false;
			}
		};
		
		auto visit = [&](int64_t idx) {
			if (idx < 0 || idx >= int64_t(_units.size())) {
				return;
			}
			UnitState &u = _units[static_cast<size_t>(idx)];
			if (!u.alive) {
				return;
			}
			if (p.exclude_instance_id != 0 && u.instance_id == p.exclude_instance_id) {
				return;
			}
			if (shape_contains(u)) {
				fn(u);
			}
		};
		
		if (!_use_spatial_broad_phase()) {
			for (int64_t idx : indices) {
				visit(idx);
			}
			return;
		}
		
		// For spatial broad-phase, use circle bounds as approximation
		double bounds_radius = p.shape_params.radius;
		if (p.shape_params.shape == AoShapeKind::Rectangle) {
			bounds_radius = Math::sqrt(p.shape_params.width * p.shape_params.width + p.shape_params.height * p.shape_params.height) * 0.5;
		}
		
		_spatial_fill_buckets_for_indices(indices);
		_spatial_stamp_circle(center_x, center_y, bounds_radius, p.spatial_team);
		for (int64_t idx : indices) {
			if (!_spatial_stamp_has(idx)) {
				continue;
			}
			visit(idx);
		}
	}

	static inline double _distance_between_coords(double x1, double y1, double x2, double y2) {
		return Math::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
	}

	void _spatial_ensure_stamp_size() const;
	void _spatial_clear_buckets() const;
	double _spatial_cell_size() const;
	int _spatial_flat_index(double x, double y) const;
	void _spatial_add_alive_team(const StringName &team) const;
	void _spatial_next_generation() const;
	void _spatial_stamp_circle(double cx, double cy, double radius, const StringName &team) const;
	void _spatial_stamp_kite_threat(double cx, double cy, double danger_radius) const;
	void _spatial_stamp_separation_candidates(double cx, double cy, double radius, const StringName &team, int64_t self_instance_id) const;
	bool _spatial_stamp_has(int64_t unit_index) const;
	void _spatial_fill_buckets_for_indices(const std::vector<int64_t> &indices) const;
	int _spatial_count_neighbors_in_grid(int64_t self_index, double cx, double cy, double radius) const;
	bool _use_spatial_broad_phase() const;

public:
	TeamfightSimulationCore();
	~TeamfightSimulationCore() override;

	bool is_ready() const;
	void clear();
	Dictionary run_match(const Variant &match_input);
	Dictionary run_match_stats(const Variant &match_input);
	Array run_matches(const Array &match_inputs);
	/// Runs `run_match_stats` semantics for each input; clears between matches like `run_matches`.
	Array run_matches_stats(const Array &match_inputs);
	void run_match_simulation_only(const Variant &match_input);
	void run_matches_simulation_only(const Array &match_inputs);
	void run_generated_matches_simulation_only(int64_t base_seed, int64_t batch_count, int64_t team_size);
	Dictionary run_generated_matches_stats_partial(int64_t base_seed, int64_t batch_count, int64_t team_size, bool include_match_log, double tick_rate = DEFAULT_TICK_RATE);

	/// Incremental match API (used by gameplay loops and viewer stepping). Does not replace run_match for batch parity runs.
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

	/// Force libc stdio flush (headless / PowerShell often fully-buffers stdout).
	void flush_stdio();

	/// Thread-safe batch progress (workers call add; main thread reads + prints — never print from worker threads).
	void benchmark_progress_reset(int64_t total_matches);
	void benchmark_progress_add(int64_t delta_matches);
	int64_t benchmark_progress_read();

	/// When true (or env TEAMFIGHT_SIM_PROFILE), _simulate emits one stderr JSON line per match with per-section tick timings.
	void sim_profile_set_enabled(bool enabled);
	void targeting_profile_set_enabled(bool enabled);
};

#endif
