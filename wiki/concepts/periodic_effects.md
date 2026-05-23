# Periodic Effects

Damage over time and heal over time with configurable stacking.

DoT/HoT effects tick at intervals (default 1.0s tick_interval) for duration. **Important**: Input parameters (attack_damage_ratio, max_hp_ratio, current_hp_ratio, missing_hp_ratio, flat_amount) now represent TOTAL amounts over the full duration, not per-tick amounts. Per-tick values are calculated as `total_amount / (duration / tick_interval)`.

**Assumption**: Duration and tick_interval are always evenly divisible (e.g., 5s duration with 1s tick_interval = 5 ticks). This ensures tick_count is always an integer. Non-divisible values will log an error but the effect will still apply (may result in incorrect total damage/heal distribution).

**Calculation modes**:
- "fixed" (default): Calculate per-tick values at effect application time using current stats, then use those values for all ticks
- "dynamic": Recalculate ratio-based amounts on each tick using current unit stats (e.g., current missing HP each tick). If the source unit dies, dynamic mode effects fall back to stored total values to continue functioning.

Stacking modes (string): refresh (reset duration and total amounts), extend (add duration with proportional scaling), separate (independent instances). StackBehavior enum (for stat modifiers): Refresh, Accumulate, Reset.

Effect_type identifies DoT/HoT for cleanse/resist interactions (burn, poison, regen, etc.). Max_stacks limits concurrent instances. Tick accumulator ensures deterministic timing: when accumulator >= tick_interval, process tick and subtract tick_interval.

Channel effects are periodic with tick_interval, allow_movement flag, post_complete_effect, post_interrupt_effect. Interrupt on stun, silence, death, or manual interrupt.
