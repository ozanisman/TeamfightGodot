# Native Draft AI Baseline

## Date/Phase
Phase 29: Partial Role Composition Scoring Baseline
Date: June 14, 2026

## Current Scoring Features Enabled

### Draft AI Scoring Components
- **Base Power:** Champion's individual winrate
- **Ally Synergy:** Pairwise synergy with existing allies
- **Enemy Counter Value:** How well candidate counters enemy team
- **Counter Risk:** How much enemy team counters candidate
- **Role Fit:** Penalty for duplicate roles, bonus for needed roles
- **Composition Fit:** Partial role composition scoring (Phase 29)

### Phase-Aware Weights
- Pick scoring weights vary by draft phase (early/mid/late)
- Ban scoring uses differential denial based on draft order
- Early ban fallback component for negligible denial scenarios

### Data Sources
- `combat_stats.csv`: Base winrates
- `matchup_with.csv`: Ally synergy (champion A with champion B)
- `matchup_vs.csv`: Enemy counter value (champion A vs champion B)
- `role_combinations.csv`: Role composition fingerprints (full 5-role comps)
- `champion_kit.csv`: Champion role assignments

## Full Draft Validation Result

**Status:** ✅ PASS

### Validation Results
- **Invalid drafts:** 0
- **Duplicate picks:** None
- **Duplicate bans:** None
- **Pick/ban overlap:** None
- **Team sizes:** Both 5
- **Total bans:** 10

### Draft Sequence
All 20 steps completed successfully:
- Steps 0-5: Bans (6 total)
- Steps 6-11: Picks (6 total)
- Steps 12-15: Bans (4 total)
- Steps 16-19: Picks (4 total)

## 50x25 Side-Symmetric Ablation Table

**Trials:** 25 per matchup
**Simulations per draft:** 25

### Complete Ablation Results

| Matchup | Blue Strategy | Red Strategy | Blue Win Rate | Red Win Rate | Invalid Drafts |
|---------|--------------|--------------|---------------|--------------|----------------|
| 1 | native_full | random_full | 89.4% | 10.6% | 0 |
| 2 | random_full | native_full | 10.9% | 89.1% | 0 |
| 3 | native_full | native_picks_random_bans | 55.4% | 44.6% | 0 |
| 4 | native_picks_random_bans | native_full | 28.3% | 71.7% | 0 |
| 5 | random_picks_native_bans | random_full | 42.4% | 57.6% | 0 |
| 6 | random_full | random_picks_native_bans | 50.9% | 49.1% | 0 |

### Key Baseline Metrics

**native_full vs random_full:**
- native_full as Blue: 89.4%
- native_full as Red: 89.1%
- **Symmetry:** Excellent (0.3% difference)

**native_full vs native_picks_random_bans:**
- native_full as Blue: 55.4%
- native_full as Red: 71.7%
- **Side asymmetry:** 16.3% (may be draft-order advantage)

**random_picks_native_bans vs random_full:**
- native_bans as Blue: 42.4%
- native_bans as Red: 49.1%
- **Side asymmetry:** 6.7% (minimal)

**Overall invalid drafts:** 0 across all 6 matchups

## Partial Comp Diagnostic Table

### Test 1: Pick Recommendations with Varying Ally Counts

| Test | Allies + Candidate | comp_fit | comp_samples | comp_match_count | comp_fingerprint |
|------|-------------------|----------|-------------|-----------------|------------------|
| 1.1 | 0 allies + candidate | 0.0177 | 138,096 | 125 | tank |
| 1.2 | 1 ally + candidate | 0.0427 | 89,631 | 56 | mage + tank |
| 1.3 | 2 allies + candidate | 0.0492 | 52,740 | 21 | fighter + mage + tank |
| 1.4 | 3 allies + candidate | 0.0925 | 12,211 | 6 | fighter + fighter + mage + tank |
| 1.5 | 4 allies + candidate | 0.1455 | 1,574 | 1 | fighter + fighter + mage + tank + tank |

### Test 2: Ban Recommendations with Varying Enemy Counts

| Test | Enemies + Candidate | enemy_comp_fit | enemy_comp_samples | enemy_comp_match_count | enemy_comp_fingerprint |
|------|---------------------|----------------|-------------------|------------------------|------------------------|
| 2.1 | 0 enemies + candidate | 0.0196 | 138,101 | 125 | mage |
| 2.2 | 1 enemy + candidate | 0.0304 | 89,353 | 56 | fighter + mage |
| 2.3 | 2 enemies + candidate | 0.0627 | 26,314 | 21 | fighter + fighter + mage |
| 2.4 | 3 enemies + candidate | 0.0925 | 12,211 | 6 | fighter + fighter + mage + tank |

### Test 3: Exact 5-Role Composition

| Test | comp_fit | comp_samples | comp_match_count | comp_fingerprint |
|------|----------|-------------|-----------------|------------------|
| 3.1 (4 allies + candidate) | 0.1455 | 1,574 | 1 | fighter + fighter + mage + tank + tank |

### Test 4: Unknown Role Case

| Test | comp_fit | comp_samples | comp_match_count | comp_fingerprint |
|------|----------|-------------|-----------------|------------------|
| 4.1 (empty allies) | 0.0177 | 138,096 | 125 | tank |

**Note:** Empty ally list still returns non-neutral because the candidate has a role. This is expected behavior.

### Test 5: Duplicate Role Case

| Test | Champions | comp_fit | comp_samples | comp_match_count | comp_fingerprint |
|------|-----------|----------|-------------|-----------------|------------------|
| 5.1 | colossus + wizard | 0.0427 | 89,631 | 56 | mage + tank |

**Note:** Test 1.4 confirms multiset matching: "fighter + fighter + mage + tank" with match_count = 6.

## Known Caveats

1. **Side asymmetry in native_full vs native_picks_random_bans:**
   - native_full as Blue: 55.4%
   - native_full as Red: 71.7%
   - Difference: 16.3%
   - This may be legitimate draft-order advantage (Red gets last pick), but should be monitored

2. **No pre-partial baseline:**
   - Baseline established in Phase 29 (partial comp scoring)
   - No direct comparison to pre-partial state available
   - Future changes should be compared against this baseline

3. **Unknown-only role behavior:**
   - Confirmed by code path to return neutral
   - Should get a direct diagnostic row in future for completeness

4. **Diagnostic script headless mode:**
   - Required SceneTree inheritance instead of Node
   - Fixed in Phase 29 validation

## Implementation Details

### Partial Role Composition Scoring (Phase 29)

**Function:** `partial_role_combination_value_for(const std::vector<StringName> &roles, int &out_match_count)`

**Algorithm:**
1. Filter unknown/empty roles
2. Return neutral if no valid roles
3. Use exact lookup for 5+ roles (unchanged behavior)
4. For partial sets (< 5 roles):
   - Count role frequencies in partial set (multiset)
   - Scan all full 5-role compositions in database
   - Check if partial multiset is subset of full multiset (with counts)
   - Aggregate wins/losses/samples from matching full comps
5. Apply Bayesian smoothing to aggregated stats
6. Return neutral if no matching full comps

**Multiset Matching Example:**
```
Partial: tank + tank
Full comp 1: fighter + marksman + mage + support + tank → NO (only 1 tank)
Full comp 2: fighter + assassin + marksman + support + tank → NO (only 1 tank)
Full comp 3: tank + tank + marksman + mage + support → YES (2+ tanks)
```

**Performance:**
- Simple linear scan of all full comps
- No caching implemented (not needed for current scale)
- Manual fingerprint parsing to avoid type conversion issues

## Recommendations for Future Phases

1. **Archive this baseline:** Use these results as reference for future feature additions
2. **Monitor side asymmetry:** If 55.4% vs 71.7% asymmetry is concerning, investigate draft-order mechanics
3. **Add unknown role diagnostic:** Create direct test for unknown-only role case
4. **Consider caching:** If profiling shows need, add caching by partial fingerprint

## Experimental Lookahead Status

See [native_draft_ai_lookahead_experiment.md](native_draft_ai_lookahead_experiment.md) for full details.
Summary: lookahead variants (Phases 34–37) were quarantined due to unacceptable side bias (>20%).

## Draft Order Bias Baseline

See [draft_order_bias_audit.md](draft_order_bias_audit.md) for full details.
Summary: current snake draft order retained; ~19.6% Blue-side structural advantage is expected and manageable.
