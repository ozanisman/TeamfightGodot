# Golden Fixture Behavior Coverage

Maps each entry in [`match_fixtures.json`](match_fixtures.json) to effect-context and combat paths from [`wiki/concepts/effect_system.md`](../../wiki/concepts/effect_system.md). Parity compares full match summaries (float tolerance); **summary signals** are the fields most likely to regress when behavior changes.

Regenerate summaries after input or champion changes:

```powershell
.\run_godot.ps1 -- --rewrite-fixture-summaries=res://fixtures/goldens/match_fixtures.json
```

Tune a single fixture with combat trace:

```powershell
.\run_godot.ps1 -- --fixture-file=res://fixtures/goldens/match_fixtures.json --debug-fixture-name=<name>
```

## Coverage matrix

| Fixture | Champions | Effect paths | Wiki section | Summary signals | Notes |
|---------|-----------|--------------|--------------|-----------------|-------|
| `duel_swordsman_vs_guardian` | swordsman, guardian | general melee + abilities | — | `damage_dealt_*`, `abilities` | `general_combat` |
| `duel_archer_vs_guardian` | archer, guardian | autos, volley, rain channel (incidental) | Cumulative damage | `damage_dealt_ability`, `damage_dealt_ultimate` | `general_combat` |
| `support_clash` | guardian, cleric, swordsman, archer | mixed roles | — | team totals | `general_combat` |
| `backline_skirmish` | artillery, guardian, rogue, wizard | ranged + melee skirmish | — | team totals | `general_combat` |
| `mixed_5v5` | 5v5 roster | full comp regression | — | all unit_stats | `general_combat` / perf |
| `effect_control_wall` | disarmer, windcaller, colossus, … | CC-heavy comp | — | `stuns`, `hard_cc_seconds` | `general_combat` |
| `effect_projectile_rush` | ninja, archer, artillery, wizard, sniper, … | many projectile kits (incidental) | Projectile deferred | `damage_dealt_ability` | not isolated defer test |
| `duel_wraith_vs_monk` | wraith, monk | duel regression | — | duel stats | `general_combat` |
| `summoner_support_skirmish` | warlock, siren, necromancer, mistcaller | summons + support | — | team totals | `general_combat` |
| `warlock_chaos_rift_channel` | warlock, guardian | channel tick `multi_effect` [aoe + `damage_based_heal`]; post-complete `use_accumulated_damage` | Channel accumulated damage | `damage_dealt_ability`, `healing_done_ability` | channel **complete** path |
| `archer_volley` | archer, guardian | `multi_target` repeat projectiles, **no** defer | Projectile deferred damage | `damage_dealt_ability` | parallel volley (repeat only) |
| `fixture_probe_defer_heal` | fixture_probe, guardian | `multi_target` [projectile, `damage_based_heal`] defer + scratch resume | Projectile deferred damage | `healing_done_ability`, `damage_dealt_ability` | isolated defer heal |
| `fixture_probe_multi_effect_defer` | fixture_probe, guardian | `multi_effect` [defer `multi_target`, damage sibling] | Projectile deferred damage | `healing_done_ability`, `damage_dealt_ultimate` | multi_effect sibling pause |
| `archer_rain_of_arrows_channel` | archer, guardian | channel + `multi_target` repeat projectiles per tick | Channel + multi_target | `damage_dealt_ultimate` | Rain of Arrows |
| `valkyrie_whirlwind_shield` | valkyrie, guardian | sync `multi_effect` [aoe, `damage_based_shield`] | Cumulative damage | `shielding_done_ultimate`, `damage_dealt_ultimate` | Whirlwind |
| `valkyrie_sweeping_strikes` | valkyrie, guardian | `post_attack` `multi_effect` [aoe, `damage_based_heal`] | Cumulative damage | `healing_done_passive`, `damage_dealt_passive` | Sweeping Strikes passive |
| `warlock_chaos_rift_interrupt` | warlock, guardian | channel **interrupt** finisher (`use_accumulated_damage` at 100%) | Channel accumulated damage | `damage_dealt_ability`, `healing_done_ability` | guardian Aegis Crash stun mid-channel (seed 2727, x=2.0/2.25) |

## Gap / manual behaviors (not golden)

These paths are hard to stabilize in summary hash tests. Use `--debug-fixture-name` and trace (`cast_start`, `projectile`, `death`) to verify after native changes.

| Behavior | Suggested setup | What to verify |
|----------|-----------------|----------------|
| Target dies before deferred projectile impact | `fixture_probe_defer_heal`; burst guardian during volley | `deferred_multi_target_active` clears; no stuck outstanding count |
| Sudden death mid-projectile batch | any 5v5 near kill threshold | outstanding decrements via abandon; chain resumes or clears |
| Killer defer preserved on kill | probe vs low-HP dummy; killer has open defer | killer defer chain continues after kill (lifecycle) |
| Channel tick skipped while defer pending | hypothetical channel + `[projectile, damage_based_heal]` tick | tick skipped; no overwrite of defer state |
| Scratch skip when deferred target dies | multi-target defer; kill deferred target before impact | resume uses fresh scratch for next target ID |

## `fixture_probe` champion

Defined only in [`champion_catalog.gd`](../../scripts/simulation/champion_catalog.gd). **Do not add to 5v5 comp fixtures.** Used exclusively by `fixture_probe_*` goldens.
