# Movement System

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → `sim_movement`, `sim_unit_tick_movement`

Unit positioning with collision avoidance and kiting behavior.

Units move toward current target at move_speed (affected by slow modifier). Movement speed multiplier clamped to minimum 0.05 to prevent math instability. Collision detection prevents units from overlapping (UNIT_COLLISION_RADIUS = 0.15).

Separation: units push away from nearby allies to prevent clustering. Kiting: ranged units retreat when too many enemies are close (KITE_DURATION = 1.0s, KITE_SPEED_MODIFIER = 0.5). Kite danger threshold triggers retreat.

Movement modes: chase (move toward target), kite (retreat from danger), separation (avoid allies). Root prevents all movement. Dash effects instant-reposition units to target location with collision validation.

**Support ally anchor:** supports chase `current_ally_target_id` (not their enemy target) up to a standoff of `attack_range * SUPPORT_ALLY_STANDOFF_RATIO` (0.85). Other roles chase enemy targets to attack range as before. Kiting runs before chase for any unit with `prefers_kiting`.

World boundaries clamp positions to [WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX] with recovery velocity for out-of-bounds units.
