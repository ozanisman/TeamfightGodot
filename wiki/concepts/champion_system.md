# Champion System

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → `sim_catalog`, `champion_catalog.gd`

Units defined by stats, abilities, ultimates, and passives.

ChampionSpec contains: ChampionStats (max_hp, attack_damage, attack_range, attack_speed, move_speed, armor, magic_resist, tenacity, life_steal, mana_cost, mana_per_attack, ability_cd, projectile_speed, projectile_radius, respawn_time, role, passive_id), ability (EffectSpec), ultimate (EffectSpec), and passive_ids array. Each EffectSpec carries a `cast_range` (double): `-1` = use attack range, `0` = no range gate, `> 0` = gate at that distance. See [targeting_ai.md](targeting_ai.md) for cast-range gating details.

ChampionCatalog builds champion data from `CHAMPION_DATA` in [`champion_catalog.gd`](../../scripts/simulation/champion_catalog.gd) (source of truth). `fixtures/goldens/champion_schema.json` is a generated export consumed by native and GDScript schema tools — do not edit JSON directly. See [DATA_SOURCE.md](../../fixtures/goldens/DATA_SOURCE.md). Thread-local caching with mutex-protected frozen specs for worker reuse. Array pooling for effect building optimization.

Roles: tank, fighter, assassin, marksman, mage, support. Role configs define stat modifiers and passive hooks (on_tick, post_take_damage). Champion passives register effects under trigger kinds: on_attack, on_defense, on_ally_defense, on_tick, post_attack, post_take_damage, on_ability, on_ultimate, post_heal, on_takedown, on_knockback (per target knocked back, runs on the source of the knockback), and on_knockback_action (per action that caused any knockback, runs on the source of the action). Balance patches can override stats, abilities, ultimates, or passive_ids per archetype/role.

Kit system allows ability/ultimate/passive swaps for balance testing.
