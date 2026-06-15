# Draft Archetype Tags Design

## Date/Phase
Phase 40: Archetype Tag Design and Data Audit
Date: June 14, 2026

## Purpose

Design a minimal champion archetype/tag system to improve pick/ban recommendations, especially comp identity and threat targeting. This is design + data audit only - no implementation yet.

## Why

Current draft AI uses:
- Base power
- Pair synergy
- Pair counters
- Role fit
- Partial role composition
- Phase-aware/side-aware ban logic

It does not understand higher-level team identity like:
- Dive
- Poke
- Frontline
- Backline
- Summon
- Control
- Burst
- Sustain
- Anti-heal
- Anti-shield
- CC chain

Archetypes may help improve comp detection and ban targeting without lookahead.

## Existing Data Audit

### Champion Definitions

**Primary source:** `fixtures/goldens/champion_schema.json`
- 26 champions defined
- Each champion has: name, role, stats, ability, ultimate, passive_ids, description
- Roles: tank, fighter, assassin, marksman, mage, support
- JSON structure is clean and extensible
- Currently has no archetype tags

**Example structure:**
```json
"archer": {
  "name": "Archer",
  "role": "marksman",
  "stats": { ... },
  "ability": { ... },
  "ultimate": { ... },
  "passive_ids": ["eagle_eye"],
  "description": "A swift ranged attacker who fires volleys of arrows..."
}
```

### Role Statistics

**Source:** `model_stats/_profile_5v5/role_stats.csv`
- Role-level win rates and damage/healing stats
- Roles: assassin, fighter, mage, marksman, support, tank
- Used for role composition scoring
- No archetype-level data

### Champion System

**Documentation:** `wiki/concepts/champion_system.md`
- Describes champion structure (ChampionSpec, ChampionStats, EffectSpec)
- Roles are well-defined and integrated
- Kit system allows ability/ultimate/passive swaps
- No mention of archetypes or tags

### Data Sources Summary

| Source | Format | Contains | Extensible? |
|--------|--------|----------|-------------|
| champion_schema.json | JSON | Full champion defs | Yes |
| role_stats.csv | CSV | Role-level stats | No (aggregated) |
| champion_kits.json | JSON | Kit templates | Yes |

**Recommendation:** Add tags to `champion_schema.json` as it's the primary champion definition and is already extensible.

## Proposed Tag Schema

### Minimal Tag List

Keep it conservative. Focus on high-impact, low-overlap tags:

**Positioning:**
- `frontline` - High durability, engages first
- `backline` - Low durability, stays safe

**Engagement:**
- `dive` - High mobility, targets backline
- `poke` - Long-range harass, low commitment

**Damage Pattern:**
- `burst` - High damage in short window
- `sustain` - Consistent damage/healing over time

**Control:**
- `cc` - Crowd control (stun, root, silence, etc.)
- `control` - Broader battlefield control (slow, zone)

**Special:**
- `summon` - Creates minions/pets
- `aoe` - Multi-target damage/effects
- `single_target` - Focused single-target damage
- `protect` - Shields, heals, defensive utility
- `mobility` - Dash, blink, movement abilities

**Counter Tags (deferred):**
- `anti_heal` - Healing reduction
- `anti_shield` - Shield penetration/break

**Total: 12-14 tags** (depending on counter tag inclusion)

### Tag Rationale

**Positioning tags** (frontline, backline):
- Critical for comp formation
- Maps to existing roles (tank/fighter → frontline, marksman/mage → backline)
- Easy to infer from stats (HP, armor, range)

**Engagement tags** (dive, poke):
- Distinguishes aggressive vs passive playstyles
- Important for comp synergy (dive needs follow-up, poke needs peel)
- Maps to abilities (dash → dive, long-range → poke)

**Damage pattern tags** (burst, sustain):
- Distinguishes spike damage vs attrition
- Important for timing and counterplay
- Maps to abilities (high ratio → burst, HoT → sustain)

**Control tags** (cc, control):
- Critical for threat assessment
- Distinguishes hard CC vs soft control
- Maps to effects (stun/root → cc, slow → control)

**Special tags** (summon, aoe, single_target, protect, mobility):
- Summon is mechanically distinct (minion management)
- AOE vs single-target is a major axis
- Protect identifies support/utility
- Mobility identifies playmaking potential

**Counter tags (deferred):**
- Anti-heal/anti-shield are niche
- Can be added later if needed
- Don't want to overfit early

## Proposed Storage Format

### Recommendation: Add to champion_schema.json

Add a `tags` array to each champion entry:

```json
"archer": {
  "name": "Archer",
  "role": "marksman",
  "tags": ["backline", "poke", "aoe"],
  "stats": { ... },
  "ability": { ... },
  "ultimate": { ... },
  "passive_ids": ["eagle_eye"],
  "description": "..."
}
```

### Rationale

**Pros:**
- Single source of truth
- Already extensible
- No new file to maintain
- Tags travel with champion definition
- Easy to validate (tags are part of schema)

**Cons:**
- Modifies existing schema (but schema is already designed for extension)
- Tags are static (but this is appropriate for archetypes)

**Alternative rejected:** Separate CSV file
- Would require separate loading logic
- Adds synchronization burden
- No clear benefit over JSON extension

### Schema Extension

Add to `champion_schema.json`:
```json
{
  "archer": {
    "tags": ["backline", "poke", "aoe"],
    ...
  }
}
```

Update schema validation to:
- Ensure tags is an array
- Ensure 1-5 tags per champion
- Ensure all tags are from approved list
- Allow empty array for uncertain champions

## Champion-to-Tag Mapping Draft

### Mapping Strategy

**Rules:**
- Max 3-5 tags per champion
- Prioritize most distinctive features
- If unsure, mark as `needs_review`
- Use role as a hint but don't duplicate (e.g., tank → frontline, not tank)

### Draft Mapping

| Champion | Role | Tags | Notes |
|----------|------|------|-------|
| archer | marksman | backline, poke, aoe | Multi-target volley, long-range |
| artillery | marksman | backline, poke, aoe, single_target | Very long-range, splash damage |
| berserker | fighter | frontline, burst, sustain | Lifesteal, HP sacrifice, high damage |
| cleric | support | backline, protect, sustain | Pure healer, no attacks |
| colossus | tank | frontline, cc, aoe | Taunt, AOE stun, high durability |
| disarmer | fighter | frontline, cc, single_target | Disarm, attack speed boost |
| earthbender | tank | frontline, cc, aoe, control | Root, AOE root, stacking defense |
| frost_mage | mage | backline, control, aoe | Slow, AOE slow/blizzard |
| guardian | tank | frontline, protect, cc | Shield allies, damage redirection |
| mirror_knight | tank | frontline, protect, reflect | Reflect damage, AOE reflect |
| mistcaller | support | backline, protect, sustain | HoT, AOE HoT, temp HP |
| monk | fighter | frontline, cc, single_target | Silence, stun, high attack speed |
| necromancer | mage | backline, summon, aoe | Summon skeletons/ghouls |
| ninja | assassin | dive, burst, single_target, mobility | Dash, execute, high mobility |
| oracle | support | backline, protect, control | Purify, cleanse, silence |
| paladin | tank | frontline, protect, sustain | Heal, shield, high durability |
| rogue | assassin | dive, burst, single_target, mobility | High damage, stealth/dash |
| silencer | mage | backline, cc, control | Silence, AOE silence |
| siren | support | backline, control, aoe | Charm, AOE charm |
| sniper | marksman | backline, poke, single_target | Very long-range, high damage |
| swordsman | fighter | frontline, burst, single_target | Simple high-damage melee |
| valkyrie | support | backline, protect, mobility | Shield, dash, revive |
| warlock | mage | backline, sustain, aoe | DoT, AOE DoT, lifesteal |
| windcaller | mage | backline, control, aoe | Knockback, AOE knockback |
| wizard | mage | backline, burst, aoe | High damage, AOE damage |
| wraith | assassin | dive, sustain, single_target | Lifesteal, phase/dash |

### Uncertain Mappings

| Champion | Uncertainty | Reason |
|----------|-------------|--------|
| artillery | needs_review | Is it poke or burst? High damage but slow |
| earthbender | needs_review | Is it cc or control? Root is hard CC but also zone control |
| mirror_knight | needs_review | Is protect the right tag? Reflect is offensive |
| mistcaller | needs_review | Is protect the right tag? HoT is sustain |
| rogue | needs_review | Is it dive? No dash in current kit |
| warlock | needs_review | Is it sustain or burst? DoT is sustain but high damage |

### Tag Distribution

**Positioning:**
- Frontline: 8 (colossus, disarmer, earthbender, guardian, monk, paladin, berserker, swordsman)
- Backline: 18 (archer, artillery, frost_mage, mistcaller, necromancer, oracle, silencer, siren, sniper, warlock, windcaller, wizard, wraith, cleric, mirror_knight, valkyrie, rogue, ninja)

**Engagement:**
- Dive: 3 (ninja, rogue, wraith)
- Poke: 3 (archer, artillery, sniper)

**Damage Pattern:**
- Burst: 5 (berserker, ninja, swordsman, wizard, rogue)
- Sustain: 5 (berserker, mistcaller, paladin, warlock, wraith)

**Control:**
- CC: 5 (colossus, disarmer, earthbender, monk, oracle)
- Control: 5 (frost_mage, earthbender, silencer, siren, windcaller)

**Special:**
- Summon: 1 (necromancer)
- AOE: 10 (archer, artillery, colossus, earthbender, frost_mage, necromancer, siren, warlock, windcaller, wizard)
- Single_target: 9 (artillery, disarmer, monk, ninja, rogue, sniper, swordsman, wraith, berserker)
- Protect: 6 (cleric, guardian, mirror_knight, mistcaller, oracle, paladin, valkyrie)
- Mobility: 3 (ninja, valkyrie, rogue)

**Distribution issues:**
- Backline is overrepresented (18 vs 8 frontline) - may need adjustment
- AOE is common (10 champions) - may be too broad
- Summon is rare (1 champion) - may be too niche
- Dive is rare (3 champions) - may need more champions

## Future Scoring Ideas

### Pick Scoring

**Reward coherent ally tag clusters:**
- Bonus for complementary tags (e.g., frontline + backline, dive + follow-up)
- Bonus for tag diversity (avoid overstacking same tag)
- Bonus for completing archetypes (e.g., 2+ dive, 2+ protect)

**Reward missing comp identity:**
- Bonus for tags we don't have yet
- Higher bonus for critical missing tags (e.g., frontline if none)
- Lower bonus for redundant tags (e.g., 3rd backline)

**Avoid overstacking fragile patterns:**
- Penalty for too many dive without frontline
- Penalty for too many poke without peel
- Penalty for too many squishy without protect

### Ban Scoring

**Ban champions that complete enemy archetypes:**
- If enemy has 2 dive, ban the 3rd dive
- If enemy has 1 summon, ban the 2nd summon
- If enemy has no frontline, ban remaining frontliners

**Ban counters to our archetype:**
- If we are dive, ban anti-dive (high CC, protect)
- If we are poke, ban gap-closers (mobility, dive)
- If we are sustain, ban anti-heal (if added)

**Ban high-value enablers when enemy has partial cluster:**
- If enemy has 1 protect, ban the 2nd protect
- If enemy has 1 frontline, ban the 2nd frontline
- If enemy has partial dive, ban the enabler (mobility)

### Tag Synergy Matrix

**Positive synergies:**
- Frontline + Backline (balanced formation)
- Dive + Protect (dive needs peel)
- Poke + Frontline (poke needs frontline)
- Sustain + Protect (sustain needs protection)
- AOE + Control (AOE needs setup)

**Negative synergies:**
- Dive + Dive (overstacking, no follow-up)
- Poke + Poke (no frontline, vulnerable)
- Backline + Backline (no frontline, vulnerable)
- Burst + Burst (inconsistent damage timing)

## Validation Plan

### Phase 1: Tag Validation

**1. Print each champion's tags:**
- Iterate through all champions in champion_schema.json
- Print champion name, role, tags
- Verify tags are from approved list
- Verify 1-5 tags per champion

**2. Check every champion has 1-5 tags:**
- Count tags per champion
- Flag champions with 0 tags (missing)
- Flag champions with >5 tags (over-tagged)
- Flag champions with `needs_review` tag

**3. Check no unknown tags:**
- Collect all unique tags across all champions
- Compare against approved tag list
- Flag any unknown tags

**4. Check tag distribution is not too sparse/dense:**
- Count champions per tag
- Flag tags with <2 champions (too sparse)
- Flag tags with >15 champions (too dense)
- Aim for 3-10 champions per tag

**5. Manually inspect 5-10 sample recommendations:**
- Run draft AI with tag-based scoring (if implemented)
- Print tag explanations for recommendations
- Manually verify recommendations make sense
- Adjust tags if recommendations are off

### Phase 2: Scoring Validation

**1. Compare tag-based vs baseline recommendations:**
- Run draft with baseline scoring
- Run draft with tag-based scoring
- Compare top 5 recommendations per pick
- Measure overlap and divergence

**2. Validate tag synergy:**
- Check that complementary tags are rewarded
- Check that overstacking is penalized
- Check that missing tags are prioritized

**3. Validate ban targeting:**
- Check that archetype completion is targeted
- Check that counters are targeted
- Check that enablers are targeted

### Phase 3: Production Validation

**1. Monitor draft quality:**
- Track win rates by tag composition
- Track pick rates by tag
- Monitor for tag exploitation

**2. A/B test:**
- Run tag-based vs baseline in parallel
- Compare win rates
- Compare draft diversity

## Open Questions

### Tag Granularity

**Question:** Are 12-14 tags too many or too few?

**Considerations:**
- More tags = more precision but more complexity
- Fewer tags = simpler but less expressive
- Current set is conservative and can be expanded

**Recommendation:** Start with 12-14 tags, expand if needed.

### Tag Inference

**Question:** Should tags be manually assigned or inferred from stats/abilities?

**Considerations:**
- Manual = accurate but labor-intensive
- Inferred = automated but may be inaccurate
- Hybrid = manual with inference validation

**Recommendation:** Manual assignment with inference validation.

### Tag Weights

**Question:** Should tags have different weights in scoring?

**Considerations:**
- Some tags may be more important (e.g., frontline)
- Some tags may be more situational (e.g., summon)
- Weights add complexity

**Recommendation:** Start with equal weights, add weights if needed.

### Tag Evolution

**Question:** How to handle new champions or balance changes?

**Considerations:**
- New champions need tags
- Balance changes may invalidate tags
- Need process for tag updates

**Recommendation:** Tags are part of champion schema, updated with balance patches.

### Counter Tags

**Question:** Should anti-heal/anti-shield tags be added now?

**Considerations:**
- Currently no champions have these mechanics
- May be added in future
- Don't want to overfit

**Recommendation:** Defer until champions have these mechanics.

## Implementation Notes

### Schema Update

Add `tags` field to `champion_schema.json`:
```json
{
  "archer": {
    "tags": ["backline", "poke", "aoe"],
    ...
  }
}
```

### Loading Logic

Update champion loading to:
- Read tags from schema
- Validate tags against approved list
- Store tags in ChampionSpec
- Provide tag access in draft AI

### Scoring Integration

Add tag-based scoring to draft AI:
- Compute tag clusters for ally team
- Compute tag clusters for enemy team
- Apply tag synergy bonuses
- Apply tag diversity bonuses
- Apply archetype completion penalties

### Validation Script

Create validation script to:
- Load champion schema
- Validate tag format
- Validate tag distribution
- Print tag statistics
- Flag uncertain mappings

## References

- `fixtures/goldens/champion_schema.json` - Champion definitions
- `model_stats/_profile_5v5/role_stats.csv` - Role statistics
- `wiki/concepts/champion_system.md` - Champion system documentation
- `wiki/notes/native_draft_ai.md` - Draft AI architecture

---

## Phase 41: Implementation Results

### Date/Phase
Phase 41: Implement Archetype Tags and Validation
Date: June 14, 2026

### Implementation Summary

**Files Modified:**
- `scripts/simulation/champion_catalog.gd` - Added tags array to CHAMPION_DATA for all 26 champions
- `scripts/simulation/champion_spec.gd` - Added tags field to ChampionSpec class and to_dict() method
- `scripts/simulation/champion_catalog.gd` - Updated _build_champion() to parse tags from data
- `scripts/tools/export_champion_schema.gd` - No changes needed (uses to_dict() automatically)
- `fixtures/goldens/champion_schema.json` - Regenerated with tags included
- `scripts/tools/validate_archetype_tags.gd` - Created validation script

**Storage Location:**
- **Source of truth:** `scripts/simulation/champion_catalog.gd` (CHAMPION_DATA constant)
- **Generated artifact:** `fixtures/goldens/champion_schema.json` (auto-generated from source)
- **Tags field:** Added to each champion entry as `tags: [&"tag1", &"tag2", ...]`

### Final Approved Tag List

**14 approved tags:**
- `frontline` - High durability, engages first
- `backline` - Low durability, stays safe
- `dive` - High mobility, targets backline
- `poke` - Long-range harass, low commitment
- `burst` - High damage in short window
- `sustain` - Consistent damage/healing over time
- `cc` - Crowd control (stun, root, silence, etc.)
- `control` - Broader battlefield control (slow, zone)
- `summon` - Creates minions/pets
- `aoe` - Multi-target damage/effects
- `single_target` - Focused single-target damage
- `protect` - Shields, heals, defensive utility
- `mobility` - Dash, blink, movement abilities
- `needs_review` - Placeholder for uncertain mappings

**Deferred tags:**
- `anti_heal` - Healing reduction (no current champion support)
- `anti_shield` - Shield penetration/break (no current champion support)

### Validation Results

**Champion Count:** 26

**Tag Distribution:**
- frontline: 8
- backline: 10
- dive: 2
- poke: 2
- burst: 4
- sustain: 4
- cc: 6
- control: 4
- summon: 1
- aoe: 7
- single_target: 6
- protect: 5
- mobility: 3
- needs_review: 6

**Validation Checks:**
- Champions Missing Tags: 0 (all champions have tags)
- Champions with Unknown Tags: 0 (all tags are from approved list)
- Champions with Too Many Tags (>5): 0 (all champions have 1-5 tags)
- Champions with needs_review: 6

**needs_review Champions:**
- rogue - Uncertain kit (placeholder abilities)
- warlock - Uncertain kit (no ultimate, melee mage)
- artillery - Uncertain archetype (siege unit, slow but high damage)
- earthbender - Uncertain archetype (tank with roots, may be cc or control)
- mirror_knight - Uncertain archetype (reflect damage, may be protect or offensive)
- mistcaller - Uncertain archetype (HoT support, may be protect or sustain)

### Full Champion-to-Tag Mapping

| Champion | Tags |
|----------|------|
| swordsman | frontline, burst, single_target |
| archer | backline, poke, aoe |
| guardian | frontline, protect, cc |
| ninja | dive, burst, single_target, mobility |
| sniper | backline, poke, single_target |
| berserker | frontline, burst, sustain |
| paladin | frontline, protect, sustain |
| rogue | needs_review |
| oracle | backline, protect, cc |
| colossus | frontline, cc, aoe |
| wraith | dive, sustain, single_target, mobility |
| warlock | needs_review |
| wizard | backline, burst, aoe |
| monk | frontline, cc, single_target |
| artillery | needs_review |
| cleric | backline, protect, sustain |
| siren | backline, control, aoe |
| valkyrie | frontline, protect, mobility |
| frost_mage | backline, control, aoe |
| earthbender | needs_review |
| silencer | backline, cc, control |
| disarmer | frontline, cc, single_target |
| windcaller | backline, control, aoe |
| mirror_knight | needs_review |
| mistcaller | needs_review |
| necromancer | backline, summon, aoe |

### Tag Distribution Analysis

**Positioning:**
- Frontline: 8 (31%) - Balanced
- Backline: 10 (38%) - Slightly overrepresented

**Engagement:**
- Dive: 2 (8%) - Underrepresented
- Poke: 2 (8%) - Underrepresented

**Damage Pattern:**
- Burst: 4 (15%) - Balanced
- Sustain: 4 (15%) - Balanced

**Control:**
- CC: 6 (23%) - Balanced
- Control: 4 (15%) - Balanced

**Special:**
- Summon: 1 (4%) - Very rare (only necromancer)
- AOE: 7 (27%) - Common
- Single_target: 6 (23%) - Balanced
- Protect: 5 (19%) - Balanced
- Mobility: 3 (12%) - Underrepresented

**Distribution Notes:**
- Backline is slightly overrepresented (38% vs 31% frontline) - acceptable
- Dive and poke are underrepresented (8% each) - may need more champions or tag adjustments
- Summon is very rare (1 champion) - expected as it's a niche mechanic
- Mobility is underrepresented (12%) - may need review
- AOE is common (27%) - acceptable given AOE abilities are common

### Implementation Status

**Completed:**
- Tags added to all 26 champions in source of truth
- ChampionSpec class updated to support tags
- Export script updated to include tags in generated JSON
- champion_schema.json regenerated with tags
- Validation script created and executed
- All validation checks passed

**Pending:**
- needs_review champions require manual review and tag assignment
- Tags are not yet integrated into draft scoring (as per Phase 41 scope)
- No changes to native draft scoring, pick recommendations, ban recommendations, draft order, lookahead, UI behavior, or stats files

### Ready for Loading into Native Draft AI

**Status:** Yes, tags are ready for loading into native draft AI.

**Reasons:**
- All champions have tags (no missing tags)
- All tags are from approved list (no unknown tags)
- All champions have 1-5 tags (no over-tagging)
- Tags are stored in the correct location (source of truth)
- Generated JSON includes tags
- Validation script confirms data integrity

**Caveats:**
- 6 champions have `needs_review` tag and require manual review
- Some tags are underrepresented (dive, poke, mobility, summon)
- Tags are not yet scoring-active (requires separate implementation phase)

### Next Steps

**Immediate:**
- Manual review of needs_review champions to assign definitive tags
- Consider adding more dive/poke/mobility champions or adjusting tags

**Future (separate phase):**
- Integrate tags into native draft scoring
- Implement tag-based pick scoring
- Implement tag-based ban scoring
- A/B test tag-based vs baseline recommendations
- Monitor win rates by tag composition

---

## Phase 44: Reviewed Archetype Tags

### Date/Phase
Phase 44: Review and Finalize Archetype Tags
Date: June 14, 2026

### Scope

Reviewed the six champions previously tagged only as `needs_review`. This phase changed tag data only. Native draft scoring, pick recommendations, ban recommendations, weights, lookahead, draft order, production UI, and stats files were not changed.

### Reviewed Decisions

| Champion | Previous tags | Reviewed tags | Remove needs_review? | Reason |
|----------|---------------|---------------|----------------------|--------|
| rogue | needs_review | burst, single_target | yes | Current kit is a melee assassin with 200% ability damage, 600% ultimate damage, and dodge. It has no dash/blink or target-selection mechanic, so it is not `dive` or `mobility` based on current mechanics. |
| warlock | needs_review | sustain, aoe | yes | Current active kit is a melee channel with repeated AOE damage, damage-based healing, and max-HP growth from self-healing. The ultimate is missing, but the implemented kit is clear enough for sustain/AOE and does not show burst or control. |
| artillery | needs_review | backline, poke, burst, aoe | yes | Current kit has 6.0 range, very low durability/mobility, explosive splash, 150% shell damage with short stun/knockback, and a 330% ultimate shell. It is long-range poke with burst and AOE splash, not primarily single-target. |
| earthbender | needs_review | frontline, cc, control, aoe | yes | Current kit is a tank with stacking defenses, single-target root, and AOE root/damage ultimate. Root is hard CC and the AOE root gives battlefield control. |
| mirror_knight | needs_review | frontline, protect, aoe | yes | Current kit is a high-durability tank with self shield/reflect and an AOE ally reflect ultimate. There is no approved `reflect` tag, so the current defensive ally utility maps to `protect`; the ally-radius ultimate supports `aoe`. |
| mistcaller | needs_review | backline, protect, sustain, aoe | yes | Current kit is a low-durability ranged support with single-target HoT, AOE HoT, overheal-to-temp-HP, and periodic AOE healing. This is sustained protective support with AOE healing. |

### Final Champion-to-Tag Mapping

| Champion | Tags |
|----------|------|
| swordsman | frontline, burst, single_target |
| archer | backline, poke, aoe |
| guardian | frontline, protect, cc |
| ninja | dive, burst, single_target, mobility |
| sniper | backline, poke, single_target |
| berserker | frontline, burst, sustain |
| paladin | frontline, protect, sustain |
| rogue | burst, single_target |
| oracle | backline, protect, cc |
| colossus | frontline, cc, aoe |
| wraith | dive, sustain, single_target, mobility |
| warlock | sustain, aoe |
| wizard | backline, burst, aoe |
| monk | frontline, cc, single_target |
| artillery | backline, poke, burst, aoe |
| cleric | backline, protect, sustain |
| siren | backline, control, aoe |
| valkyrie | frontline, protect, mobility |
| frost_mage | backline, control, aoe |
| earthbender | frontline, cc, control, aoe |
| silencer | backline, cc, control |
| disarmer | frontline, cc, single_target |
| windcaller | backline, control, aoe |
| mirror_knight | frontline, protect, aoe |
| mistcaller | backline, protect, sustain, aoe |
| necromancer | backline, summon, aoe |

### Remaining needs_review Champions

None.

### Tag Distribution After Review

- frontline: 10
- backline: 12
- dive: 2
- poke: 3
- burst: 6
- sustain: 6
- cc: 7
- control: 5
- summon: 1
- aoe: 12
- single_target: 7
- protect: 7
- mobility: 3
- needs_review: 0

### Validation Results

Commands were run through Godot 4.6.2 headless with explicit workspace log files because direct headless runs can fail around `user://logs`.

- `validate_archetype_tags.gd`: PASS. 26 champions, 0 missing tags, 0 unknown tags, 0 champions with more than 5 tags, 0 `needs_review`.
- `validate_native_archetype_tags.gd`: PASS. 26 champions, 26 with tags, 0 missing tags, 0 unknown tags, 0 `needs_review`.
- `validate_native_draft_ai_tags.gd`: PASS for the tag path. Current native draft AI output still includes `candidate_tags`; top pick `colossus` and top ban `wizard` matched schema tags with 0 missing and 0 unknown tags.
- Full draft validation with native strategy: PASS. 5 blue picks, 5 red picks, 10 bans, no duplicate picks, no duplicate bans, no pick/ban overlap, 0 invalid selections.

Native validation emitted existing effective-champion fallback messages for unsupported ability kinds while building recommendation inputs. Those messages did not prevent the tag-path checks or full draft validity checks from passing.

### Readiness

The reviewed tags are ready for a small scoring experiment. Scoring remains unchanged in this phase; tags are still data/debug-only until a separate scoring change is made.

---

## Phase 45: Experimental Archetype Scoring

### Date/Phase
Phase 45: Experimental Archetype Scoring
Date: June 14, 2026

### Status

Archetype scoring is implemented only for the opt-in `native_archetype` strategy. The default `native` strategy remains unchanged.

### Weights

- Pick archetype weight: `0.06`
- Ban archetype weight: `0.04`

### Pair Table

Each pair applies at most once per candidate.

| Pair | Raw score |
|------|-----------|
| frontline + backline | +0.030 |
| frontline + poke | +0.020 |
| frontline + protect | +0.015 |
| cc + aoe | +0.020 |
| control + poke | +0.020 |
| dive + mobility | +0.015 |
| dive + protect | +0.015 |
| sustain + frontline | +0.015 |
| protect + backline | +0.015 |

### Pick Scoring

For `native_archetype`, candidate tags are compared with ally tags using the pair table. Small overstack penalties apply for:

- `overstack_backline`: ally backline count >= 4 and candidate has `backline` (`-0.030`)
- `no_frontline_backline`: ally frontline count == 0 and candidate has `backline` (`-0.020`)
- `overstack_poke`: ally poke count >= 3 and candidate has `poke` (`-0.020`)
- `overstack_dive`: ally dive count >= 3 and candidate has `dive` (`-0.020`)

Raw pick archetype score is clamped to `[-0.08, +0.08]`, then multiplied by `0.06`.

### Ban Scoring

For `native_archetype`, candidate tags are compared with enemy tags using the same pair table. A `+0.020` completion bonus applies when the enemy already has 2+ champions with one of the candidate's major tags:

`dive`, `poke`, `burst`, `sustain`, `cc`, `control`, `protect`, `aoe`

Raw ban archetype score is clamped to `[0.00, +0.08]`, then multiplied by `0.04`.

### Debug Fields

Pick recommendations expose:

- `candidate_tags`
- `archetype_score`
- `archetype_weight`
- `archetype_contribution`
- `archetype_reasons`

Ban recommendations expose:

- `candidate_tags`
- `enemy_archetype_score`
- `enemy_archetype_weight`
- `enemy_archetype_contribution`
- `archetype_reasons`

Example validated debug reasons:

- Pick `colossus`: `frontline+backline`, `frontline+poke`, `cc+aoe`
- Ban `wizard`: `frontline+backline`, `protect+backline`

### Validation Results

- `validate_archetype_tags.gd`: PASS. 26 champions, 0 missing tags, 0 unknown tags, 0 champions with more than 5 tags, 0 `needs_review`.
- `validate_native_archetype_tags.gd`: PASS. 26 champions, 26 with tags, 0 missing tags, 0 unknown tags, 0 `needs_review`.
- `validate_native_draft_ai_tags.gd`: PASS. Current native output still includes `candidate_tags`; `native_archetype` output includes archetype score, weight, contribution, and readable reasons for pick and ban recommendations.
- Full draft validation with `native`: PASS. 0 invalid selections.
- Full draft validation with `native_archetype`: PASS. 0 invalid selections.

Native validation still emits existing effective-champion fallback messages for unsupported ability kinds while building recommendation inputs. Those messages did not prevent tag validation, debug-field validation, or full draft validity checks from passing.

### 25x25 A/B Summary

| Blue strategy | Red strategy | Blue win rate | Red win rate | Invalid drafts |
|---------------|--------------|---------------|--------------|----------------|
| native_archetype | random | 89.9% | 10.1% | 0 |
| random | native_archetype | 17.0% | 83.0% | 0 |
| native_archetype | native | 61.8% | 38.2% | 0 |
| native | native_archetype | 68.2% | 31.8% | 0 |
| native_archetype | native_archetype | 68.0% | 32.0% | 0 |
| native | native | 61.3% | 38.7% | 0 |

`native_archetype` beats random from both sides and stays competitive with `native`. However, self-play side bias increased from the known native 61.3%/38.7% result in this 25x25 run to 68.0%/32.0%, which is about +6.7 percentage points of Blue-side win rate.

### Recommendation

Keep `native_archetype` experimental. Tags and debug output are ready for further scoring experiments, but this version should not be promoted to default because the initial screen shows a side-bias increase slightly above the requested ~5 percentage point guardrail.

---

## Phase 46: Tuned Experimental Archetype Scoring

### Date/Phase
Phase 46: Tune Experimental Archetype Scoring
Date: June 14, 2026

### Scope

Added experimental weight profiles for archetype scoring. The default `native` strategy, draft order, lookahead, tag data, champion stats, champion abilities, and production UI were not changed.

### Weight Profiles

| Profile | Pick weight | Ban weight | Notes |
|---------|-------------|------------|-------|
| `archetype_full` | 0.06 | 0.04 | Same behavior as Phase 45 `native_archetype`. |
| `archetype_light` | 0.03 | 0.02 | Half-strength pick and ban archetype scoring. |
| `archetype_pick_light` | 0.03 | 0.00 | Half-strength pick-only archetype scoring. |
| `archetype_ban_light` | 0.00 | 0.02 | Half-strength ban-only archetype scoring. |

### Contribution Diagnostics

Report: `archetype_scoring_diagnostic_report.md`

| Profile | Avg contribution | Max | Min | Top recommendation changes |
|---------|------------------|-----|-----|----------------------------|
| `archetype_full` | 0.001175 | 0.004800 | -0.001200 | 2 of 20 |
| `archetype_light` | 0.000588 | 0.002400 | -0.000600 | 1 of 20 |
| `archetype_pick_light` | 0.000403 | 0.002400 | -0.000600 | 1 of 20 |
| `archetype_ban_light` | 0.000185 | 0.001600 | 0.000000 | 0 of 20 |

Most common reasons across profiles were `frontline+backline`, `protect+backline`, `cc+aoe`, `frontline+protect`, `frontline+poke`, and `sustain+frontline`. The reasons remained readable.

### 25x25 Screening

| Blue strategy | Red strategy | Blue win rate | Red win rate | Invalid drafts |
|---------------|--------------|---------------|--------------|----------------|
| native | native | 63.4% | 36.6% | 0 |
| archetype_full | archetype_full | 73.4% | 26.6% | 0 |
| archetype_light | archetype_light | 74.6% | 25.4% | 0 |
| archetype_pick_light | archetype_pick_light | 67.8% | 32.2% | 0 |
| archetype_ban_light | archetype_ban_light | 62.1% | 37.9% | 0 |
| archetype_full | native | 60.8% | 39.2% | 0 |
| native | archetype_full | 68.8% | 31.2% | 0 |
| archetype_light | native | 61.1% | 38.9% | 0 |
| native | archetype_light | 71.5% | 28.5% | 0 |
| archetype_pick_light | native | 57.0% | 43.0% | 0 |
| native | archetype_pick_light | 70.2% | 29.8% | 0 |
| archetype_ban_light | native | 62.7% | 37.3% | 0 |
| native | archetype_ban_light | 59.0% | 41.0% | 0 |
| archetype_light | random | 86.4% | 13.6% | 0 |
| random | archetype_light | 21.1% | 78.9% | 0 |

Additional 25x25 random check for the only qualifying profile:

| Blue strategy | Red strategy | Blue win rate | Red win rate | Invalid drafts |
|---------------|--------------|---------------|--------------|----------------|
| archetype_ban_light | random | 89.8% | 10.2% | 0 |
| random | archetype_ban_light | 9.4% | 90.6% | 0 |

### 50x25 Confirmation

`archetype_ban_light` qualified for confirmation because it had 0 invalid drafts, beat random from both sides, remained competitive with native, and did not increase self-play Blue-side bias in the 25x25 screen.

| Blue strategy | Red strategy | Blue win rate | Red win rate | Invalid drafts |
|---------------|--------------|---------------|--------------|----------------|
| native | native | 62.5% | 37.5% | 0 |
| archetype_ban_light | archetype_ban_light | 62.5% | 37.5% | 0 |
| archetype_ban_light | native | 58.3% | 41.7% | 0 |
| native | archetype_ban_light | 61.7% | 38.3% | 0 |
| archetype_ban_light | random | 85.8% | 14.2% | 0 |
| random | archetype_ban_light | 14.4% | 85.6% | 0 |

### Recommendation

Use `archetype_ban_light` as the preferred experimental archetype profile for the next scoring experiment. Keep `native_archetype`/`archetype_full` experimental and do not promote it, because both full and light pick-including profiles worsened self-play Blue-side bias in the 25x25 screen.

Default `native` remains unchanged.
