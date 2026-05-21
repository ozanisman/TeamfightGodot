# Spatial Grid

Performance optimization for targeting and AOE queries.

World divided into 8x8 grid buckets (SPATIAL_GRID_DIM). Each bucket stores unit indices. Broad-phase enabled when alive team count >= 4 (SPATIAL_BROAD_PHASE_TEAM_THRESHOLD). Spatial stamping marks units in relevant buckets per query, enabling O(1) lookup instead of O(n) brute force.

Used by: _for_each_unit_in_circle (AOE damage/heal), _for_each_unit_in_shape (universal AOE), obscurance calculation (frontline blocking), cluster density calculation. Aux grids for enemy/player frontline targeting with signature-based invalidation.

Reduces targeting complexity from O(n²) to O(n) for large team sizes. Standard 5v5 can enter spatial path under current benchmark contract.
