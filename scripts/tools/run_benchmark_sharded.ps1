# Runs N parallel Godot processes over disjoint seed ranges. Aggregate m/s = TotalBatchCount / wall-clock.
param(
    [int]$TotalBatchCount = 2000,
    [int]$ShardCount = 4,
    [int]$TeamSize = 5,
    [switch]$BenchSkipSummaries = $true,
    [int]$WorkersPerShard = 1
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$worker = Join-Path $PSScriptRoot "run_benchmark_shard_worker.ps1"
if (-not (Test-Path $worker)) {
    throw "run_benchmark_shard_worker.ps1 not found at $worker"
}

$shardCount = [Math]::Max(1, $ShardCount)
$chunk = [int][Math]::Ceiling([double]$TotalBatchCount / [double]$shardCount)

$benchArg = if ($BenchSkipSummaries) { @("-BenchSkipSummaries") } else { @() }

$t0 = [DateTime]::UtcNow
$children = @()
for ($s = 0; $s -lt $shardCount; $s++) {
    $start = $s * $chunk
    if ($start -ge $TotalBatchCount) {
        break
    }
    $count = [Math]::Min($chunk, $TotalBatchCount - $start)
    $logPath = Join-Path $projectRoot "logs\bench_shard_$s.log"

    $argList = @(
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $worker,
        "-ProjectRoot", $projectRoot,
        "-BatchCount", $count,
        "-BaseSeed", $start,
        "-TeamSize", $TeamSize,
        "-Workers", $WorkersPerShard
    ) + $benchArg

    $proc = Start-Process -FilePath "powershell.exe" -ArgumentList $argList -PassThru -WindowStyle Hidden -RedirectStandardOutput $logPath -RedirectStandardError "$logPath.err"
    $children += @{ Proc = $proc; Log = $logPath }
}

foreach ($c in $children) {
    $c.Proc.WaitForExit()
    $exitCode = 0
    try {
        $exitCode = [int]$c.Proc.ExitCode
    }
    catch {
        $exitCode = -1
    }
    $logText = if (Test-Path $c.Log) { Get-Content -Path $c.Log -Raw } else { "" }
    $okJson = $logText -match '"matches_per_sec"\s*:'
    if ($exitCode -ne 0 -and -not $okJson) {
        if (Test-Path $c.Log) {
            Get-Content -Path $c.Log -Tail 40 | Write-Host
        }
        throw "Shard exited with code $exitCode (no benchmark JSON). Log: $($c.Log)"
    }
    Get-Content -Path $c.Log | Write-Output
    $errPath = "$($c.Log).err"
    if ((Test-Path $errPath) -and ((Get-Item $errPath).Length -gt 0)) {
        Get-Content -Path $errPath | Write-Host
    }
}

$elapsed = ([DateTime]::UtcNow - $t0).TotalSeconds
$mps = [double]$TotalBatchCount / [Math]::Max($elapsed, 0.000001)
Write-Output ("aggregate_duration_sec`t{0}" -f $elapsed)
Write-Output ("aggregate_matches_per_sec`t{0}" -f $mps)
Write-Output ("shards`t{0}" -f $children.Count)
