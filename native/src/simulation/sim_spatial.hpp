#ifndef SIM_SPATIAL_HPP
#define SIM_SPATIAL_HPP

#include "sim_world.hpp"

#include <cstdint>
#include <vector>

namespace sim {

const std::vector<int64_t> &alive_indices_for_team(const SimWorld &world, const StringName &team);

void invalidate_spatial_bucket_fill(SimWorld &world);
void on_unit_position_changed(SimWorld &world, const UnitState &unit);
void fill_buckets_for_indices_cached(SimWorld &world, const std::vector<int64_t> &indices);

void ensure_stamp_size(SimWorld &world);
void clear_buckets(SimWorld &world);
double cell_size();
int flat_index(double x, double y);
void add_alive_team(SimWorld &world, const StringName &team);
void next_generation(SimWorld &world);
bool stamp_has(const SimWorld &world, int64_t unit_index);
void stamp_circle(SimWorld &world, double cx, double cy, double radius, const StringName &team);
void stamp_separation_candidates(SimWorld &world, double cx, double cy, double radius, const StringName &team, int64_t self_instance_id);
void stamp_kite_threat(SimWorld &world, double cx, double cy, double danger_radius);
void fill_buckets_for_indices(SimWorld &world, const std::vector<int64_t> &indices);
int count_neighbors_in_grid(const SimWorld &world, int64_t self_index, double cx, double cy, double radius);
bool use_spatial_broad_phase(const SimWorld &world);

} // namespace sim

#endif // SIM_SPATIAL_HPP
