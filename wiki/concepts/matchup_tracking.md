# Matchup Tracking

> **Native implementation:** [native_agent_guide.md](../notes/native_agent_guide.md) → `matchup_tracker.gd` (GDScript tooling)

Winrate analysis for champion vs champion and champion with champion relationships.

MatchupTracker records: vs_opponent (wins, losses, winrate), with_ally (wins, losses, winrate). Data structure: champion_id → {vs_other_id, with_other_id}. Initialized for all champion pairs from ChampionCatalog.

Process match result: winners get vs wins vs all losers, with wins vs all other winners. Losers get vs losses vs all winners, with losses vs all other losers. Merge operation combines data from multiple trackers.

Used by stats dashboard for balance analysis. Winrate = wins / (wins + losses). Reset clears all data.
