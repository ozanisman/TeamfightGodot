# Stat Modifiers

Temporary and permanent stat changes with stacking infrastructure.

Stats support additive and multiplicative modifiers: effective = (base + additive) * multiplicative. Verified in get_effective_* functions. Duration types: temp (expires), perm (lasts until death), match_duration (lasts entire match).

Stack system: StackEntry tracks current_stacks, max_stacks, base_value, duration, stack_behavior (Refresh, Accumulate, Reset). StackBehavior enum defined in C++. Expiration queue manages time-based expiration. Thread-local pooling for performance.

Stack consumption: consume_stacks_damage uses stacks to boost damage then clears. set_stacks sets to specific count (to_max or fallback). Stat modifiers apply via function pointer table per stat type (_apply_stat_* functions).

Stat modifier opcodes: stat_modifier, consume_stacks_damage, consume_stacks_heal, consume_stacks_shield, set_stacks.
