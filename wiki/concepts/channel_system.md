# Channel System

Sustained effects with interruptible state machine.

Channel effects have duration, tick_interval, allow_movement flag, target_mode (fixed/dynamic), sub_effect (applied each tick), post_complete_effect (on success), post_interrupt_effect (on failure). Channel state tracked in UnitStateCold: is_channeling, remaining_duration, tick_accumulator, accumulated_damage, target_id.

Each tick: process_channel_tick applies sub_effect, accumulates damage, increments tick_count. Interrupt conditions: stun, silence, death, or manual interrupt. On complete: apply post_complete_effect. On interrupt: apply post_interrupt_effect. allow_movement determines if unit can move while channeling.

Used for sustained abilities (e.g., warlock's Chaos Rift). Channels can be interrupted by CC effects.
