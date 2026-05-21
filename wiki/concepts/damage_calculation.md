# Damage Calculation

Armor and magic resist reduce incoming damage by percentage.

Damage types: physical (reduced by armor), magic (reduced by magic resist), true (ignores all defense). Defense multiplier formula: `1 / (1 + defense_stat)` for armor/magic resist.

Final damage calculation: `base_damage * attack_modifiers * ability_modifiers * ultimate_modifiers * defense_multiplier * dodge_multiplier * type_multiplier`. Auto-dodge (agility passive) can negate auto attacks entirely.

Lifesteal heals for percentage of damage dealt. Reflect damage returns portion of received damage to attacker. Damage sources tracked for assist credit (5s window).

Critical damage not implemented. Execute bonuses apply when target HP below threshold (default 25%).
