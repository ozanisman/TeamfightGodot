# Role Configs

Role-specific stat modifiers and passive hooks.

RoleConfig defines: stat_modifiers (stat → value), passive_on_tick (effect per tick), passive_post_take_damage (effect after taking damage). Roles: tank, fighter, assassin, marksman, mage, support.

Stat modifiers apply to all champions of role. Tank gains mana from damage taken (TANK_MANA_GAIN_DAMAGE_RATIO). Mage has passive mana regen (MAGE_MANA_REGEN_TICK). Stat mod types defined in SimConstants: tenacity/life_steal/ability_cd are additive, attack_speed/move_speed are multiplicative.

Passive hooks trigger on role-specific events: on_tick (regeneration, mana gain), post_take_damage (ally defense redirect). Role configs loaded from role_kits.json, merged into effective champion data.
