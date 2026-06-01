# Champion System

Units defined by stats, abilities, ultimates, and passives.

ChampionSpec contains: ChampionStats (max_hp, attack_damage, attack_range, attack_speed, move_speed, armor, magic_resist, tenacity, life_steal, mana_cost, mana_per_attack, ability_cd, projectile_speed, projectile_radius, respawn_time, role, passive_id), ability (EffectSpec), ultimate (EffectSpec), and passive_ids array.

ChampionCatalog builds champion data from GDScript constants or JSON schema. Thread-local caching with mutex-protected frozen specs for worker reuse. Array pooling for effect building optimization.

Roles: tank, fighter, assassin, marksman, mage, support. Role configs define stat modifiers and passive hooks (on_tick, post_take_damage). Balance patches can override stats, abilities, ultimates, or passive_ids per archetype/role.

Kit system allows ability/ultimate/passive swaps for balance testing.
