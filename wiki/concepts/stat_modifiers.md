# Stat Modifiers

Temporary and permanent stat changes with stacking infrastructure.

Stats support additive and multiplicative modifiers: effective = (base + additive) * multiplicative. Duration types: temp (expires), perm (lasts until death), match_duration (lasts entire match).

Stack system: StackEntry tracks current_stacks, max_stacks, base_value, duration, stack_behavior (refresh, accumulate, reset). Expiration queue manages time-based expiration. Thread-local pooling for performance.

Stack consumption: consume_stacks_damage uses stacks to boost damage then clears. set_stacks sets to specific count (to_max or fallback). Stat modifiers apply via function pointer table per stat type.

Stat modifier opcodes: stat_modifier, consume_stacks_damage, consume_stacks_heal, consume_stacks_shield, set_stacks.
