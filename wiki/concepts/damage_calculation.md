# Damage Calculation

Armor and magic resist reduce incoming damage by percentage.

Damage types: physical (reduced by armor), magic (reduced by magic resist), true (ignores all defense). Defense multiplier formula: `1.0 - defense_stat` for armor/magic resist. Negative defense values increase damage taken (defense multiplier > 1.0).

Final damage calculation: `base_damage * attack_modifiers * ability_modifiers * ultimate_modifiers * defense_multiplier * dodge_multiplier`. Auto-dodge (agility passive) can negate auto attacks entirely.

Lifesteal heals for percentage of damage dealt: `heal = damage * life_steal`. Reflect damage returns portion of received damage to attacker: `reflected = damage * reflect_pct`. Damage sources tracked for assist credit (5s window).

Critical damage not implemented. Execute bonuses are targeting preferences (not damage bonuses) when target HP below 25%.
