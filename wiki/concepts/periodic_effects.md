# Periodic Effects

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → `sim_periodic*`

Damage over time and heal over time with configurable stacking.

DoT/HoT effects tick at intervals (default 1.0s tick_interval) for duration. **Important**: Input parameters (attack_damage_ratio, max_hp_ratio, current_hp_ratio, missing_hp_ratio, flat_amount) now represent TOTAL amounts over the full duration, not per-tick amounts. Per-tick values are calculated as `total_amount / (Math::floor(duration / tick_interval) + 1)` to account for the instant tick applied on effect application.

**Duration rounding**: Non-divisible durations are rounded down to the nearest tick_interval multiple using Math::floor. For example, duration=1.6s with tick_interval=0.5s becomes tick_count=3 (1.5s duration). A warning is logged when rounding occurs. If the rounded tick_count is less than 1, the effect is rejected with an error.

**Calculation modes**:
- "fixed" (default): Calculate per-tick values at effect application time using current stats, then use those values for all ticks
- "dynamic": Recalculate ratio-based amounts on each tick using current unit stats (e.g., current missing HP each tick). While the source is alive, DoT effects use the source's current attack damage and HoT effects use the target's current HP state. If the source unit dies, dynamic mode effects fall back to stored total values calculated at application time, allowing effects to continue ticking even after the source dies.

Stacking modes (string for periodic effects): refresh (reset duration and total amounts), extend (add duration with proportional scaling), separate (independent instances). Note: Stat modifiers use a different StackBehavior enum (Refresh, Accumulate, Reset) with different semantics.

Effect_type identifies DoT/HoT for cleanse/resist interactions (burn, poison, regen, etc.). Max_stacks limits concurrent instances. Tick accumulator ensures deterministic timing: when accumulator >= tick_interval, process tick and subtract tick_interval.

Channel effects are periodic with tick_interval, allow_movement flag, post_complete_effect, post_interrupt_effect. Interrupt on stun, silence, death, or manual interrupt.
