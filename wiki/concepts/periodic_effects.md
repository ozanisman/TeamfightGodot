# Periodic Effects

Damage over time and heal over time with configurable stacking.

DoT/HoT effects tick at intervals (default 1s) for duration. Damage/heal per tick based on: attack_damage_ratio, max_hp_ratio, flat_amount. Stacking modes: refresh (reset duration), extend (add duration), stack_damage (add damage per tick), separate (independent instances).

Effect_type identifies DoT/HoT for cleanse/resist interactions (burn, poison, regen, etc.). Max_stacks limits concurrent instances. Tick accumulator ensures deterministic timing.

Channel effects are periodic with tick_interval, allow_movement flag, post_complete_effect, post_interrupt_effect. Interrupt on stun, silence, death, or manual interrupt.
