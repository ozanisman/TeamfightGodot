# Projectile System

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → `sim_combat_projectile`, `sim_combat_actions`

Delayed damage/effects with travel time and collision.

Projectiles have: source_id, target_id, position (pos_x, pos_y), speed, damage, damage_type, stun_duration, radius, action_kind, reason. Move each tick by speed toward target position.

Collision detection: when projectile reaches target position (distance <= radius), resolve effect. Melee attacks have projectile_speed = 0 (instant). Ranged attacks use projectile_speed from champion stats.

Projectile resolution applies damage, stun, or other effects to target. Splash damage via effect's splash dict applies AOE at impact point. Projectile state tracked in _projectiles vector, cleared after resolution.

Projectile time weighting used in targeting (marksman/mage prefer targets with lower projectile travel time).
