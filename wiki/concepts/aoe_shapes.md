# AOE Shapes

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → `sim_aoe.hpp`, `sim_effects_exec_aoe`

Area-of-effect targeting with expandable shape system.

Shapes: circle (radius), cone (radius, width, rotation), rectangle (width, height, rotation), line (width). Anchors: self (caster position), target (target position), point (custom coordinates), forward (caster facing direction).

Shape iteration uses spatial broad-phase when alive team count >= 4 (SPATIAL_BROAD_PHASE_TEAM_THRESHOLD = 4). Spatial grid divides world into 8x8 buckets (SPATIAL_GRID_DIM = 8) for O(1) lookup instead of O(n) brute force.

AOE effect opcodes: aoe_damage, aoe_stun, aoe_slow, aoe_root, aoe_silence, aoe_disarm, aoe_taunt, aoe_knockback, aoe_reflect, aoe_damage_over_time, aoe_heal_over_time. Each applies effect to all units matching shape and team_filter (ally/enemy).
