#ifndef SIM_WORLD_HPP
#define SIM_WORLD_HPP

#include "sim_constants.hpp"
#include "simulation_types.hpp"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <array>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sim {

/// Per-tick reuse of spatial bucket fills for the same alive-team index list (kite/separation).
struct SpatialBucketFillCache {
	const std::vector<int64_t> *indices = nullptr;
	bool valid = false;
};

struct SimWorld {
	std::vector<UnitState> &units;
	std::vector<UnitStateCold> &unit_cold;
	std::unordered_map<int64_t, int64_t> &unit_index_map;
	std::vector<TargetingFrameEntry> &targeting_frame;
	TickContext &tick_ctx;
	std::vector<int64_t> &alive_player_indices;
	std::vector<int64_t> &alive_enemy_indices;
	double &time;
	double &tick_rate;

	mutable std::array<std::vector<int64_t>, SPATIAL_GRID_DIM * SPATIAL_GRID_DIM> *spatial_buckets = nullptr;
	mutable std::vector<uint32_t> *spatial_stamp = nullptr;
	mutable uint32_t *spatial_generation = nullptr;
	SpatialBucketFillCache *spatial_fill_cache = nullptr;

	SimWorld(
			std::vector<UnitState> &p_units,
			std::vector<UnitStateCold> &p_unit_cold,
			std::unordered_map<int64_t, int64_t> &p_unit_index_map,
			std::vector<TargetingFrameEntry> &p_targeting_frame,
			TickContext &p_tick_ctx,
			std::vector<int64_t> &p_alive_player_indices,
			std::vector<int64_t> &p_alive_enemy_indices,
			double &p_time,
			double &p_tick_rate,
			std::array<std::vector<int64_t>, SPATIAL_GRID_DIM * SPATIAL_GRID_DIM> *p_spatial_buckets,
			std::vector<uint32_t> *p_spatial_stamp,
			uint32_t *p_spatial_generation,
			SpatialBucketFillCache *p_spatial_fill_cache = nullptr) :
			units(p_units),
			unit_cold(p_unit_cold),
			unit_index_map(p_unit_index_map),
			targeting_frame(p_targeting_frame),
			tick_ctx(p_tick_ctx),
			alive_player_indices(p_alive_player_indices),
			alive_enemy_indices(p_alive_enemy_indices),
			time(p_time),
			tick_rate(p_tick_rate),
			spatial_buckets(p_spatial_buckets),
			spatial_stamp(p_spatial_stamp),
			spatial_generation(p_spatial_generation),
			spatial_fill_cache(p_spatial_fill_cache) {}
};

inline UnitStateCold &uc(SimWorld &world, UnitState &unit) {
	const size_t index = static_cast<size_t>(&unit - world.units.data());
	return world.unit_cold[index];
}

inline const UnitStateCold &uc(const SimWorld &world, const UnitState &unit) {
	const size_t index = static_cast<size_t>(&unit - world.units.data());
	return world.unit_cold[index];
}

struct EffectContext;
struct EffectRecord;
struct ViewerHooks;

namespace effects {
struct EffectExecBindings;
}

struct SimHostCallbacks {
	void *user_data = nullptr;
	const ViewerHooks *viewer_hooks = nullptr;
	effects::EffectExecBindings *effect_exec = nullptr;

	Dictionary (*execute_effect)(SimHostCallbacks &host, const EffectRecord &effect, EffectContext &context) = nullptr;
	void (*handle_death)(void *user_data, UnitState &killer, UnitState &target) = nullptr;
	void (*sync_targeting_frame_unit)(void *user_data, const UnitState &unit) = nullptr;
	void (*sync_targeting_frame_index)(void *user_data, int64_t index, const UnitState &unit) = nullptr;
	void (*emit_trace)(void *user_data, const StringName &kind, int64_t src_id, int64_t tgt_id, double val) = nullptr;
	void (*viewer_record_damage_fx)(
			void *user_data,
			const UnitState &source,
			const UnitState &target,
			double total_damage,
			const StringName &action_kind,
			const StringName &damage_type) = nullptr;
	void (*viewer_record_heal_fx)(void *user_data, const UnitState &target, double amount) = nullptr;
	void (*viewer_record_shield_fx)(void *user_data, const UnitState &target, double amount) = nullptr;
	void (*viewer_record_hot_status_fx)(
			void *user_data,
			const UnitState &target,
			double duration,
			const StringName &effect_type) = nullptr;
	double (*randf)(void *user_data) = nullptr;
};

} // namespace sim

#endif // SIM_WORLD_HPP
