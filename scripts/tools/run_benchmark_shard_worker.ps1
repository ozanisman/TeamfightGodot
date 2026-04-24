# Invoked by run_benchmark_sharded.ps1 (one process per shard). Do not run directly unless debugging.
param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectRoot,
    [Parameter(Mandatory = $true)]
    [int]$BatchCount,
    [Parameter(Mandatory = $true)]
    [int]$BaseSeed,
    [int]$TeamSize = 5,
    [int]$Workers = 1,
    [switch]$BenchSkipSummaries = $false
)

$ErrorActionPreference = "Stop"
Set-Location $ProjectRoot
$runGodot = Join-Path $ProjectRoot "run_godot.ps1"
$userArgs = @(
    "--",
    "--check-benchmark",
    "--batch-count=$BatchCount",
    "--base-seed=$BaseSeed",
    "--team-size=$TeamSize",
    "--workers=$Workers"
)
if ($BenchSkipSummaries) {
    $userArgs += "--bench-skip-summaries"
}
& $runGodot @userArgs
exit $LASTEXITCODE
