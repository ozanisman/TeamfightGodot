# Targeting AI

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → `sim_targeting*`, `sim_targeting_strategies`.

Scoring-based enemy and ally selection with role-specific strategies.

Each unit maintains a `current_target_id` and retargets periodically (default 0.5s interval). Target selection scores enemies on: distance (weighted by role), HP (`hp_weight`), perceived threat (`enemy_threat_weight` × burst damage signal), threat response (enemy currently attacking self), role priority, execute threshold (low HP bonus), and spacing (currently disabled). Kiting repositioning is movement-only (`prefers_kiting` → `sim_movement.cpp`).

Perceived threat is accumulated on burst damage (`THREAT_BURST_*` in `sim_constants.hpp`) and decayed per tick. Enemy scoring applies `score -= perceived_threat * enemy_threat_weight * SCORE_THREAT_WEIGHT_SCALE * PREY_PERCEIVED_THREAT_SCALE`. Tune global magnitude via `PREY_PERCEIVED_THREAT_SCALE`; tune per-role focus via `enemy_threat_weight` in `sim_targeting_strategies.cpp`.

Role-specific strategies use different weightings defined in UnitStrategy: tanks (distance_weight=1.5, threat_response=2.0), assassins (distance_weight=1.0, hp_weight=2.0, threat_response=0.8), fighters (distance_weight=3.0 when close, threat_response=1.8), marksmen (distance_weight=1.0, threat_response=1.0), mages (distance_weight=1.2, hp_weight=2.5, threat_response=1.1), supports (distance_weight=1.5, ally_hp_weight=5.0, threat_response=1.7).

Ally targeting (for support abilities) scores on: distance, HP (lower HP prioritized), threat (under pressure), and role priority (supports prioritize other supports/carries).

Target switching uses a margin threshold to prevent thrashing. Target stickiness bonus reduces switching frequency.

**Reactive retarget** (`sim_targeting_reactive.cpp`): full enemy rescoring normally runs at most every `RETARGET_INTERVAL` (0.5s). Events set `retarget_timer = 0` on affected units so the next targeting pass rescans without waiting. Priority events also set `retarget_priority_eval` to bypass `TARGET_STICKINESS_THRESHOLD` once and clear `target_switch_lock_timer`.

| Trigger | Affected units | Stickiness bypass |
|---------|----------------|-------------------|
| Current target dies | Units with `target_id` = victim (cleared) | yes |
| Taunt expires | Taunted unit | yes |
| Victim crosses into execute HP (≤ `TARGET_EXECUTE_HP_RATIO`) | Opposing team alive | yes |
| Threat burst on victim (`THREAT_BURST_THRESHOLD`) | Opposing team alive | no |
| Enemy acquires you as target (`set_current_target`) | Acquired unit | yes |
