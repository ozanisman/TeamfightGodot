param(
	[Parameter(ValueFromRemainingArguments = $true)]
	[string[]]$Arguments
)

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$godotExe = "C:\Godot\godot.exe"
$logsDir = Join-Path $projectRoot "logs"
$logFile = Join-Path $logsDir "godot.log"
$null = New-Item -ItemType Directory -Force -Path $logsDir
$timeoutSeconds = 120
$checkOnly = $Arguments -contains "--check-only"
$checkNativeLoad = $Arguments -contains "--check-native-load"
$checkDeterminism = $Arguments -contains "--check-determinism"
$checkBenchmark = $Arguments -contains "--check-benchmark"
$checkBenchmarkSharded = $Arguments -contains "--check-benchmark-sharded"
$checkBalancePatches = $Arguments -contains "--check-balance-patches"
$checkStatsDashboard = $Arguments -contains "--check-stats-dashboard"
$checkStatsAggregator = $Arguments -contains "--check-stats-aggregator"
$checkStatsCsvDeterminism = $Arguments -contains "--check-stats-csv-determinism"
$generateStats = $Arguments -contains "--generate-stats"
$checkMatchTelemetry = $Arguments -contains "--check-match-telemetry"
$checkLargeProjectileDamage = $Arguments -contains "--check-large-projectile-damage"
$checkFixtureFile = $false
foreach ($argument in $Arguments) {
	if ($argument -like "--fixture-file=*") {
		$checkFixtureFile = $true
		break
	}
}
if ($checkOnly) {
	$timeoutSeconds = 15
}
elseif ($checkStatsDashboard) {
	$timeoutSeconds = 30
}
elseif ($checkNativeLoad) {
	$timeoutSeconds = 15
}
elseif ($checkMatchTelemetry) {
	$timeoutSeconds = 15
}
elseif ($checkLargeProjectileDamage) {
	$timeoutSeconds = 15
}
elseif ($checkDeterminism) {
	$timeoutSeconds = 30
}
elseif ($checkBenchmark) {
	$timeoutSeconds = 180
}
elseif ($checkBenchmarkSharded) {
	$timeoutSeconds = 300
}
elseif ($checkStatsCsvDeterminism) {
	$timeoutSeconds = 240
}
if ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkOnly) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkNativeLoad) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkMatchTelemetry) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkLargeProjectileDamage) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkDeterminism) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkBenchmark) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkBenchmarkSharded) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkStatsDashboard) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkStatsAggregator) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkStatsCsvDeterminism) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_GENERATE_STATS_TIMEOUT_SECONDS -and $generateStats) {
	[int]$timeoutSeconds = $env:RUN_GODOT_GENERATE_STATS_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_TIMEOUT_SECONDS -and -not $checkOnly) {
	[int]$timeoutSeconds = $env:RUN_GODOT_TIMEOUT_SECONDS
}

$godotArgs = @("--path", $projectRoot, "--log-file", $logFile)
$isSimulationViewer = $Arguments -contains "--simulation-viewer"
if (-not $isSimulationViewer) {
	$godotArgs += @("--headless")
}
if ($isSimulationViewer) {
	$godotArgs += @("--maximized")
}
if ($checkOnly) {
	$compileStructScript = Join-Path $projectRoot "scripts\tools\check_sim_effects_compile_structure.py"
	if (Test-Path $compileStructScript) {
		& python $compileStructScript
		if ($LASTEXITCODE -ne 0) {
			exit $LASTEXITCODE
		}
	}
	$godotArgs += @("--script", "res://scripts/tools/check_gdscript_preload.gd")
}
elseif ($checkNativeLoad) {
	$godotArgs += @("--script", "res://scripts/tools/check_native_load.gd")
}
elseif ($checkMatchTelemetry) {
	$godotArgs += @("--script", "res://scripts/tools/check_match_telemetry.gd")
}
elseif ($checkLargeProjectileDamage) {
	$godotArgs += @("--script", "res://scripts/tools/check_large_projectile_damage.gd")
}
elseif ($checkDeterminism) {
	$godotArgs += @("--script", "res://scripts/tools/check_determinism.gd")
}
elseif ($checkBenchmark) {
	$godotArgs += @("--script", "res://scripts/tools/check_benchmark.gd")
}
elseif ($checkBenchmarkSharded) {
	# Run benchmark sharded across processes (PowerShell driver).
	function Get-ArgInt([string]$Prefix, [int]$DefaultValue) {
		foreach ($a in $Arguments) {
			if ($a -like "$Prefix*") {
				return [int]($a.Substring($Prefix.Length))
			}
		}
		return $DefaultValue
	}
	function Has-Flag([string]$Flag) {
		return $Arguments -contains $Flag
	}
	$batchCount = [Math]::Max(1, (Get-ArgInt "--batch-count=" 2000))
	$teamSize = [Math]::Max(1, (Get-ArgInt "--team-size=" 5))
	$shardCount = [Math]::Max(1, (Get-ArgInt "--shards=" (Get-ArgInt "--workers=" (Get-ArgInt "--max-workers=" 8))))
	$workersPerShard = [Math]::Max(1, (Get-ArgInt "--workers-per-shard=" 1))
	$benchSkip = (Has-Flag "--bench-skip-summaries")

	$driver = Join-Path $projectRoot "scripts\tools\run_benchmark_sharded.ps1"
	if (-not (Test-Path $driver)) {
		Write-Error "Sharded benchmark driver not found at $driver"
		exit 1
	}
	# Sharded mode is intended for the bench-skip throughput gate; the driver defaults BenchSkipSummaries on.
	& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $driver -TotalBatchCount $batchCount -ShardCount $shardCount -TeamSize $teamSize -WorkersPerShard $workersPerShard
	exit $LASTEXITCODE
}
elseif ($checkBalancePatches) {
	$godotArgs += @("--script", "res://scripts/tools/check_balance_patches.gd")
}
elseif ($checkStatsDashboard) {
	$godotArgs += @("--script", "res://scripts/tools/check_stats_dashboard_load.gd")
}
elseif ($checkStatsAggregator) {
	$godotArgs += @("--script", "res://scripts/tools/check_stats_aggregator_roundtrip.gd")
}
elseif ($checkStatsCsvDeterminism) {
	$godotArgs += @("--script", "res://scripts/tools/check_stats_csv_determinism.gd")
}
elseif ($generateStats) {
	$godotArgs += @("--script", "res://scripts/tools/generate_simulation_stats.gd")
}
elseif (-not $checkOnly -and -not $checkNativeLoad -and -not $checkMatchTelemetry -and -not $isSimulationViewer) {
	$godotArgs += @("--script", "res://scripts/tools/headless_bootstrap.gd")
}
if ($Arguments.Count -gt 0) {
	$godotArgs += "--"
	$godotArgs += $Arguments
}
# Avoid false failures: check-only tails this file and aborts on any "Parse Error" line from a prior run.
if (($checkOnly -or $checkStatsDashboard -or $checkStatsAggregator -or $checkStatsCsvDeterminism) -and (Test-Path $logFile)) {
	Clear-Content -Path $logFile
}
$process = Start-Process -FilePath $godotExe -ArgumentList $godotArgs -PassThru -NoNewWindow

try {
	if ($checkOnly -or $checkStatsDashboard -or $checkStatsAggregator -or $checkStatsCsvDeterminism) {
		$deadline = (Get-Date).AddSeconds($timeoutSeconds)
		while ((Get-Date) -lt $deadline) {
			if ($process.HasExited) {
				break
			}

			if (Test-Path $logFile) {
				$tail = Get-Content -Path $logFile -Tail 80 -ErrorAction SilentlyContinue
				if ($tail -match "Parse Error:|Compilation failed|Failed to load script") {
					Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
					Write-Error "Godot check failed. See $logFile."
					exit 1
				}
			}

			Start-Sleep -Milliseconds 250
		}

		if (-not $process.HasExited) {
			Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
			Write-Error "Godot check timed out after $timeoutSeconds seconds."
			exit 124
		}
	}
	else {
		$deadline = (Get-Date).AddSeconds($timeoutSeconds)
		while ((Get-Date) -lt $deadline) {
			if ($process.HasExited) {
				break
			}

			Start-Sleep -Milliseconds 250
		}

		if (-not $process.HasExited) {
			Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
			Write-Error "Godot timed out after $timeoutSeconds seconds."
			exit 124
		}
	}

	$process.Refresh()
	if ($checkNativeLoad -or $checkMatchTelemetry -or $checkLargeProjectileDamage -or $checkDeterminism -or $checkBenchmark -or $checkBalancePatches -or $checkFixtureFile -or $checkStatsCsvDeterminism) {
		if (Test-Path $logFile) {
			$tail = Get-Content -Path $logFile -Tail 200 -ErrorAction SilentlyContinue
			$failurePattern = "SCRIPT ERROR:|Parse Error:|Compilation failed|Failed to load script|GDExtension load failed|Native simulation backend unavailable|Failed to open fixture file|Failed to open JSON file|Fixture .*mismatch|Fixture parity failed|Replay determinism failed|balance_patch_suite: FAILED|check_match_telemetry: .*invalid|check_match_telemetry: missing|check_match_telemetry: bad|check_large_projectile_damage: (?!OK)|check_stats_csv_determinism: (?!OK)"
			if ($tail -match $failurePattern) {
				Write-Error "Godot check failed. See $logFile."
				exit 1
			}
		}
	}
	exit $process.ExitCode
}
catch [System.TimeoutException] {
	try {
		Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
	}
	catch {
	}
	Write-Error "Godot timed out after $timeoutSeconds seconds."
	exit 124
}
