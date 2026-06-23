# Role Configs

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → `sim_catalog`, `champion_catalog.gd`

Role-specific stat modifiers and passive hooks loaded from `champion_schema.json` at catalog build time.

The native catalog loader (`sim_catalog.cpp`) reads the `role_configs` dictionary from the schema. For each champion, it looks up the role config matching the champion's `role` and applies:
- `stat_mods`: additive stat modifiers merged into base stats.
- `passive_on_tick`: an effect pushed into the `on_tick` passive slot.
- `passive_post_take_damage`: an effect pushed into the `post_take_damage` passive slot.

Roles: tank, fighter, assassin, marksman, mage, support.

Stat modifiers apply to all champions of that role. Tank gains mana from damage taken. Mage has passive mana regen. Stat mod types: tenacity/life_steal/ability_cd are additive; attack_speed/move_speed are multiplicative.

Passive hooks trigger on role-specific events: on_tick (regeneration, mana gain), post_take_damage (ally defense redirect).
