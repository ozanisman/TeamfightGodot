# Channel System

Sustained effects with interruptible state machine.

Channel effects have duration, tick_interval, allow_movement flag, target_mode (fixed/dynamic/self), sub_effect (applied each tick), post_complete_effect (on success), post_interrupt_effect (on failure). Channel state tracked in UnitStateCold: is_channeling, channel_remaining_duration, channel_tick_accumulator, channel_accumulated_damage, target_id.

Each tick: _process_channel_tick applies sub_effect, accumulates damage, increments tick_count. Interrupt conditions via _should_interrupt_channel: stun, silence, death, or manual interrupt. On complete: _complete_channel applies post_complete_effect. On interrupt: _interrupt_channel applies post_interrupt_effect. allow_movement determines if unit can move while channeling.

Used for sustained abilities (e.g., warlock's Chaos Rift). Channels can be interrupted by CC effects.
