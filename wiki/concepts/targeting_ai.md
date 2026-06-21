# Targeting AI

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → `sim_targeting*`, `sim_targeting_strategies`.

Scoring-based enemy and ally selection with role-specific strategies.

Each unit maintains a `current_target_id` and retargets periodically (default 0.5s interval). Target selection scores enemies on: distance (weighted by role), HP (`hp_weight`), perceived threat (`enemy_threat_weight` × burst damage signal), threat response (enemy currently attacking self), role priority, execute threshold (low HP bonus), and spacing (currently disabled). Kiting repositioning is movement-only (`prefers_kiting` → `sim_movement.cpp`).

Perceived threat is accumulated on burst damage (`THREAT_BURST_*` in `sim_constants.hpp`) and decayed per tick. Enemy scoring applies `score -= perceived_threat * enemy_threat_weight * SCORE_THREAT_WEIGHT_SCALE * PREY_PERCEIVED_THREAT_SCALE`. Tune global magnitude via `PREY_PERCEIVED_THREAT_SCALE`; tune per-role focus via `enemy_threat_weight` in `sim_targeting_strategies.cpp`.

Role-specific strategies use different weightings defined in UnitStrategy: tanks (distance_weight=1.5, threat_response=2.0), assassins (distance_weight=1.0, hp_weight=2.0, threat_response=0.8), fighters (distance_weight=3.0 when close, threat_response=1.8), marksmen (distance_weight=1.0, threat_response=1.0), mages (distance_weight=1.2, hp_weight=2.5, threat_response=1.1), supports (distance_weight=1.5, ally_hp_weight=5.0, threat_response=1.7).

Ally targeting (for support abilities) scores on: distance, HP (lower HP prioritized), threat (under pressure), and role priority (supports prioritize carries). `select_ally_target` sets `current_ally_target_id` each tick.

**Support positioning:** supports still select an enemy target for autos and peel scoring. Movement uses three modes (`sim_unit_tick.cpp`): (1) outside `attack_range * SUPPORT_ALLY_STANDOFF_RATIO` from the role-prioritized ally anchor → close toward ally; (2) inside that band but the current enemy is still out of attack range → advance toward the enemy like the rest of the team (round-start travel); (3) otherwise hold in the standoff band. Outside ally standoff, supports skip auto-attacks and defer kiting until repositioned.

**Cast range (`EffectCastRangeSpec`):** compiled at unit build from ability/ultimate effect trees (`compile_cast_range_spec` in `sim_combat_internal.cpp`). Stored on `UnitState` as `ability_cast_range_spec` / `ultimate_cast_range_spec`. `is_in_cast_range` gates casts in `sim_unit_tick_combat.cpp`; `snapshot_ally_cast_target_id` sets `casting_ally_target_id` at cast start (`sim_combat_actions.cpp`).

| Spec flag | Meaning |
|-----------|---------|
| `needs_enemy` | Enemy must be within range (stun, damage, enemy AOE, etc.) |
| `needs_single_ally` | Point ally heal/shield/HoT; cast blocked if no ally selected or selected ally out of range |
| `skips_proximity` | No single-target proximity gate (self-AOE, summon, ally `multi_target`, etc.) |

When `requires_target_in_range` is false on the action (e.g. Cleric Heal), proximity is not checked. Otherwise: ally `multi_target` skips single-ally proximity; single-ally effects require a selected ally (`current_ally_target_id != 0`) and **block** when no ally is selected or the selected ally is out of range (unit moves closer). Self-cast variants must set `target_self=true` to bypass the ally requirement. Mixed `multi_effect` kits require each set component (enemy AND single-ally when both flags are set).

Target switching uses `switch_margin` in `should_switch` to prevent thrashing. Routine stickiness keeps the current target when the best alternative is not sufficiently better: `improvement = current_score - best_raw` must exceed `clamp(STICKINESS_RATIO * |current_score|, STICKINESS_FLOOR, STICKINESS_CEILING)` (lower score = better). A stickiness keep also extends the retarget interval by `STICKINESS_RETARGET_BONUS`.

**Reactive retarget** (`sim_targeting_reactive.cpp`): full enemy rescoring normally runs at most every `RETARGET_INTERVAL` (0.5s). Events set `retarget_timer = 0` on affected units so the next targeting pass rescans without waiting. Priority events also set `retarget_priority_eval` to bypass stickiness once and clear `target_switch_lock_timer`.

| Trigger | Affected units | Stickiness bypass |
|---------|----------------|-------------------|
| Current target dies | Units with `target_id` = victim (cleared) | yes |
| Taunt expires | Taunted unit | yes |
| Victim crosses into execute HP (≤ `TARGET_EXECUTE_HP_RATIO`) | Opposing team alive | yes |
| Threat burst on victim (`THREAT_BURST_THRESHOLD`) | Opposing team alive | no |
| Enemy acquires you as target (`set_current_target`) | Acquired unit | yes |
