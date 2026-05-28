#include "sim_targeting_internal.hpp"

namespace sim {
namespace targeting {
namespace internal {

double random_unit(SimHostCallbacks &host) {
	if (host.randf != nullptr) {
		return host.randf(host.user_data);
	}
	return 0.0;
}

double strategy_role_prio(const std::array<double, ROLE_SLOT_COUNT> &slots, int64_t role_slot) {
	if (role_slot < 0 || role_slot >= ROLE_SLOT_COUNT) {
		return 0.0;
	}
	return slots[static_cast<size_t>(role_slot)];
}

uint64_t profile_elapsed_ns(std::chrono::steady_clock::time_point t0) {
	using namespace std::chrono;
	return duration_cast<nanoseconds>(steady_clock::now() - t0).count();
}

ProfileAccScope::ProfileAccScope(bool profile, uint64_t *out_accum)
		: enabled(profile && out_accum != nullptr), accum(out_accum) {
	if (enabled) {
		t0 = std::chrono::steady_clock::now();
	}
}

ProfileAccScope::~ProfileAccScope() {
	if (enabled && accum != nullptr) {
		*accum += profile_elapsed_ns(t0);
	}
}

void sync_frame_index(SimWorld &world, int64_t index, const UnitState &unit, const SimHostCallbacks *host) {
	if (host != nullptr && host->sync_targeting_frame_index != nullptr) {
		host->sync_targeting_frame_index(host->user_data, index, unit);
		return;
	}
	sync_targeting_frame_index(world, index, unit);
}

void sync_frame_unit(SimWorld &world, const UnitState &unit, const SimHostCallbacks *host) {
	if (host != nullptr && host->sync_targeting_frame_unit != nullptr) {
		host->sync_targeting_frame_unit(host->user_data, unit);
		return;
	}
	sync_targeting_frame_unit(world, unit);
}

} // namespace internal
} // namespace targeting
} // namespace sim
