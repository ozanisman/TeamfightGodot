# Targeting AI

Scoring-based enemy and ally selection with role-specific strategies.

Each unit maintains a `current_target_id` and retargets periodically (default 0.5s interval). Target selection scores enemies on: distance (weighted by role), HP (lower HP preferred by assassins/mages), threat (recent damage dealers), obscurance (frontline blocking), flanking (positioning relative to team center), bodyguard (protecting carries), and execute threshold (low HP bonus).

Role-specific strategies use different weightings: tanks prioritize frontline and peel, assassins prefer low HP backliners, marksmen kite from danger, supports peel for allies. `UnitStrategy` defines weights for distance, HP, ally distance, ally HP, ally threat, role priorities, stickiness, kiting preference, etc.

Ally targeting (for support abilities) scores on: distance, HP (lower HP prioritized), threat (under pressure), and role priority (supports prioritize other supports/carries).

Target switching uses a margin threshold (default 0.75) to prevent thrashing. Target stickiness bonus reduces switching frequency.
