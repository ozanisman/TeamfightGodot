# CI Stats Feature Status

The confidence interval (CI) feature in the stats dashboard appears to be non-functional.

## Current State

- The "DATA MODE" button toggles between "Mode: CI" and "Mode: Regular"
- The button is disabled when `_loader.ci_stats.is_empty()` (stats_dashboard.gd:1109)
- CI data is expected to come from the stats CSV bundle at `res://stats_output`
- The tooltip has two code paths: one for CI-enabled (with confidence intervals) and one for regular mode

## Issue

Users have never seen CI intervals in tooltips, suggesting either:
1. The CSV aggregator doesn't generate CI data
2. The CI data isn't being loaded properly by `StatsDashboardLoaderScript`
3. The CSV files don't include the required `ci` field

## Investigation Needed

- Check if `stats_csv_aggregator.gd` generates CI intervals
- Verify CSV file format includes `ci` field
- Test `StatsDashboardLoaderScript.ci_stats` loading logic
