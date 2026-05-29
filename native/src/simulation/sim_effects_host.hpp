#ifndef SIM_EFFECTS_HOST_HPP
#define SIM_EFFECTS_HOST_HPP

#include "sim_catalog.hpp"
#include "sim_world.hpp"

#include <vector>

namespace sim {
namespace effects {

namespace execution {
struct SimExecCallbacks;
}

/// Coordinator-owned match state accessed by spawn/stat-stack effect opcodes.
struct SimMatchHost {
	std::vector<PendingSpawn> *pending_spawns = nullptr;
	std::vector<ProjectileState> *projectiles = nullptr;
	int64_t *max_instance_id = nullptr;
	const catalog::CatalogState *catalog = nullptr;
};

/// World refs and exec hooks for the effect trampoline (no coordinator hop).
struct EffectExecBindings {
	std::vector<UnitState> *units = nullptr;
	std::vector<UnitStateCold> *unit_cold = nullptr;
	std::unordered_map<int64_t, int64_t> *unit_index_map = nullptr;
	std::vector<TargetingFrameEntry> *targeting_frame = nullptr;
	TickContext *tick_ctx = nullptr;
	std::vector<int64_t> *alive_player_indices = nullptr;
	std::vector<int64_t> *alive_enemy_indices = nullptr;
	double *time = nullptr;
	double *tick_rate = nullptr;
	std::array<std::vector<int64_t>, SPATIAL_GRID_DIM * SPATIAL_GRID_DIM> *spatial_buckets = nullptr;
	std::vector<uint32_t> *spatial_stamp = nullptr;
	uint32_t *spatial_generation = nullptr;
	SpatialBucketFillCache *spatial_fill_cache = nullptr;
	SimMatchHost match_host;
	const execution::SimExecCallbacks *exec_callbacks = nullptr;

	SimWorld make_world() const;
};

Dictionary host_execute_effect(SimHostCallbacks &host, const EffectRecord &effect, EffectContext &context);

} // namespace effects
} // namespace sim

#endif // SIM_EFFECTS_HOST_HPP
