# Movement System

Unit positioning with collision avoidance and kiting behavior.

Units move toward current target at move_speed (affected by slow modifier). Movement speed multiplier clamped to minimum 0.05 to prevent math instability. Collision detection prevents units from overlapping (UNIT_COLLISION_RADIUS = 0.15).

Separation: units push away from nearby allies to prevent clustering. Kiting: ranged units retreat when too many enemies are close (KITE_DURATION = 1.0s, KITE_SPEED_MODIFIER = 0.5). Kite danger threshold triggers retreat.

Movement modes: chase (move toward target), kite (retreat from danger), separation (avoid allies). Root prevents all movement. Dash effects instant-reposition units to target location with collision validation.

World boundaries clamp positions to [WORLD_BOUNDARY_MIN, WORLD_BOUNDARY_MAX] with recovery velocity for out-of-bounds units.
