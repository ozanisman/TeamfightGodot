#include "sim_spatial.hpp"

#include "sim_constants.hpp"

#include <algorithm>

namespace sim {

const std::vector<int64_t> &alive_indices_for_team(const SimWorld &world, const StringName &team) {
	static const StringName player_team("player");
	if (team == player_team) {
		return world.alive_player_indices;
	}
	return world.alive_enemy_indices;
}

void ensure_stamp_size(SimWorld &world) {
	if (world.spatial_stamp->size() < world.units.size()) {
		world.spatial_stamp->resize(world.units.size(), 0);
	}
}

void clear_buckets(SimWorld &world) {
	for (auto &bucket : *world.spatial_buckets) {
		bucket.clear();
	}
}

double cell_size() {
	return (WORLD_BOUNDARY_MAX - WORLD_BOUNDARY_MIN) / double(SPATIAL_GRID_DIM);
}

int flat_index(double x, double y) {
	double cs = cell_size();
	int ix = int(Math::floor((x - WORLD_BOUNDARY_MIN) / cs));
	int iy = int(Math::floor((y - WORLD_BOUNDARY_MIN) / cs));
	ix = CLAMP(ix, 0, SPATIAL_GRID_DIM - 1);
	iy = CLAMP(iy, 0, SPATIAL_GRID_DIM - 1);
	return iy * SPATIAL_GRID_DIM + ix;
}

void add_alive_team(SimWorld &world, const StringName &team) {
	const std::vector<int64_t> &indices = alive_indices_for_team(world, team);
	for (int64_t idx : indices) {
		const UnitState &u = world.units[idx];
		if (!u.alive) {
			continue;
		}
		int fi = flat_index(u.pos_x, u.pos_y);
		(*world.spatial_buckets)[static_cast<size_t>(fi)].push_back(idx);
	}
}

void next_generation(SimWorld &world) {
	ensure_stamp_size(world);
	*world.spatial_generation += 1;
	if (*world.spatial_generation == 0) {
		std::fill(world.spatial_stamp->begin(), world.spatial_stamp->end(), 0);
		*world.spatial_generation = 1;
	}
}

bool stamp_has(const SimWorld &world, int64_t unit_index) {
	if (unit_index < 0 || unit_index >= int64_t(world.spatial_stamp->size())) {
		return false;
	}
	return (*world.spatial_stamp)[static_cast<size_t>(unit_index)] == *world.spatial_generation;
}

void stamp_circle(SimWorld &world, double cx, double cy, double radius, const StringName &team) {
	next_generation(world);
	if (radius <= 0.0) {
		return;
	}
	const double r2 = radius * radius;
	double cs = cell_size();
	int icx = int(Math::floor((cx - WORLD_BOUNDARY_MIN) / cs));
	int icy = int(Math::floor((cy - WORLD_BOUNDARY_MIN) / cs));
	int span = int(Math::ceil(radius / cs)) + 1;
	for (int dy = -span; dy <= span; ++dy) {
		for (int dx = -span; dx <= span; ++dx) {
			int ix = icx + dx;
			int iy = icy + dy;
			if (ix < 0 || iy < 0 || ix >= SPATIAL_GRID_DIM || iy >= SPATIAL_GRID_DIM) {
				continue;
			}
			int fi = iy * SPATIAL_GRID_DIM + ix;
			for (int64_t idx : (*world.spatial_buckets)[static_cast<size_t>(fi)]) {
				const UnitState &u = world.units[idx];
				if (!u.alive || u.team != team) {
					continue;
				}
				double ox = u.pos_x - cx;
				double oy = u.pos_y - cy;
				double dist_sq = ox * ox + oy * oy;
				// Inclusive disk (parity with AoE `distance_between(...) <= radius`); matches refined tests below.
				if (dist_sq <= r2) {
					(*world.spatial_stamp)[static_cast<size_t>(idx)] = *world.spatial_generation;
				}
			}
		}
	}
}

void stamp_separation_candidates(SimWorld &world, double cx, double cy, double radius, const StringName &team, int64_t self_instance_id) {
	next_generation(world);
	if (radius <= 0.0) {
		return;
	}
	double r2 = radius * radius;
	double cs = cell_size();
	int icx = int(Math::floor((cx - WORLD_BOUNDARY_MIN) / cs));
	int icy = int(Math::floor((cy - WORLD_BOUNDARY_MIN) / cs));
	int span = int(Math::ceil(radius / cs)) + 1;
	for (int dy = -span; dy <= span; ++dy) {
		for (int dx = -span; dx <= span; ++dx) {
			int ix = icx + dx;
			int iy = icy + dy;
			if (ix < 0 || iy < 0 || ix >= SPATIAL_GRID_DIM || iy >= SPATIAL_GRID_DIM) {
				continue;
			}
			int fi = iy * SPATIAL_GRID_DIM + ix;
			for (int64_t idx : (*world.spatial_buckets)[static_cast<size_t>(fi)]) {
				const UnitState &u = world.units[idx];
				if (!u.alive || u.team != team || u.instance_id == self_instance_id) {
					continue;
				}
				double ox = u.pos_x - cx;
				double oy = u.pos_y - cy;
				double d2 = ox * ox + oy * oy;
				if (d2 <= EPSILON || d2 >= r2) {
					continue;
				}
				(*world.spatial_stamp)[static_cast<size_t>(idx)] = *world.spatial_generation;
			}
		}
	}
}

void stamp_kite_threat(SimWorld &world, double cx, double cy, double danger_radius) {
	next_generation(world);
	if (danger_radius <= 0.0) {
		return;
	}
	double danger_r2 = danger_radius * danger_radius;
	double cs = cell_size();
	int icx = int(Math::floor((cx - WORLD_BOUNDARY_MIN) / cs));
	int icy = int(Math::floor((cy - WORLD_BOUNDARY_MIN) / cs));
	int span = int(Math::ceil(danger_radius / cs)) + 1;
	for (int dy = -span; dy <= span; ++dy) {
		for (int dx = -span; dx <= span; ++dx) {
			int ix = icx + dx;
			int iy = icy + dy;
			if (ix < 0 || iy < 0 || ix >= SPATIAL_GRID_DIM || iy >= SPATIAL_GRID_DIM) {
				continue;
			}
			int fi = iy * SPATIAL_GRID_DIM + ix;
			for (int64_t idx : (*world.spatial_buckets)[static_cast<size_t>(fi)]) {
				const UnitState &enemy = world.units[idx];
				if (!enemy.alive) {
					continue;
				}
				double ex = enemy.pos_x;
				double ey = enemy.pos_y;
				double ddx = cx - ex;
				double ddy = cy - ey;
				double d2 = ddx * ddx + ddy * ddy;
				if (d2 <= EPSILON || d2 >= danger_r2) {
					continue;
				}
				(*world.spatial_stamp)[static_cast<size_t>(idx)] = *world.spatial_generation;
			}
		}
	}
}

void fill_buckets_for_indices(SimWorld &world, const std::vector<int64_t> &indices) {
	clear_buckets(world);
	for (int64_t idx : indices) {
		const UnitState &u = world.units[idx];
		if (!u.alive) {
			continue;
		}
		int fi = flat_index(u.pos_x, u.pos_y);
		(*world.spatial_buckets)[static_cast<size_t>(fi)].push_back(idx);
	}
}

int count_neighbors_in_grid(const SimWorld &world, int64_t self_index, double cx, double cy, double radius) {
	const UnitState &self = world.units[self_index];
	double r2 = radius * radius;
	double cs = cell_size();
	int icx = int(Math::floor((cx - WORLD_BOUNDARY_MIN) / cs));
	int icy = int(Math::floor((cy - WORLD_BOUNDARY_MIN) / cs));
	int span = int(Math::ceil(radius / cs)) + 1;
	int count = 0;
	for (int dy = -span; dy <= span; ++dy) {
		for (int dx = -span; dx <= span; ++dx) {
			int ix = icx + dx;
			int iy = icy + dy;
			if (ix < 0 || iy < 0 || ix >= SPATIAL_GRID_DIM || iy >= SPATIAL_GRID_DIM) {
				continue;
			}
			int fi = iy * SPATIAL_GRID_DIM + ix;
			for (int64_t idx : (*world.spatial_buckets)[static_cast<size_t>(fi)]) {
				if (idx == self_index) {
					continue;
				}
				const UnitState &other = world.units[idx];
				if (!other.alive) {
					continue;
				}
				double odx = other.pos_x - self.pos_x;
				double ody = other.pos_y - self.pos_y;
				if (odx * odx + ody * ody <= r2) {
					count += 1;
				}
			}
		}
	}
	return count;
}

bool use_spatial_broad_phase(const SimWorld &world) {
	return int64_t(world.alive_player_indices.size()) >= SPATIAL_BROAD_PHASE_TEAM_THRESHOLD
			|| int64_t(world.alive_enemy_indices.size()) >= SPATIAL_BROAD_PHASE_TEAM_THRESHOLD;
}

} // namespace sim
