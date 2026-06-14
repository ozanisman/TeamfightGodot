# Benchmark History

User-run benchmarks tracking simulation performance across git commits.

## Format
- **Commit**: Git commit hash
- **Matches/sec**: Performance metric
- **Configuration**: Test parameters (batch count, team size, workers)
- **Context**: Additional context about the benchmark (e.g., recent changes, optimizations)

## Entries

### 2026-06-13 - e88a398
- **Commit**: e88a398
- **Matches/sec**: ~450
- **Configuration**: 2000 matches, 8 worker threads, team size 5
- **Context**: Stat cache refactoring (72 calls converted to cache-aware pattern)
