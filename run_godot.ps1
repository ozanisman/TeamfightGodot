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
$checkBalancePatches = $Arguments -contains "--check-balance-patches"
$checkStatsDashboard = $Arguments -contains "--check-stats-dashboard"
$checkStatsAggregator = $Arguments -contains "--check-stats-aggregator"
$generateStats = $Arguments -contains "--generate-stats"
if ($checkOnly) {
	$timeoutSeconds = 15
}
elseif ($checkStatsDashboard) {
	$timeoutSeconds = 30
}
elseif ($checkNativeLoad) {
	$timeoutSeconds = 15
}
elseif ($checkDeterminism) {
	$timeoutSeconds = 30
}
elseif ($checkBenchmark) {
	$timeoutSeconds = 180
}
if ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkOnly) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkNativeLoad) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkDeterminism) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkBenchmark) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkStatsDashboard) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkStatsAggregator) {
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
	$godotArgs += @("--script", "res://scripts/tools/check_only.gd")
}
elseif ($checkNativeLoad) {
	$godotArgs += @("--script", "res://scripts/tools/check_native_load.gd")
}
elseif ($checkDeterminism) {
	$godotArgs += @("--script", "res://scripts/tools/check_determinism.gd")
}
elseif ($checkBenchmark) {
	$godotArgs += @("--script", "res://scripts/tools/check_benchmark.gd")
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
elseif ($generateStats) {
	$godotArgs += @("--script", "res://scripts/tools/generate_simulation_stats.gd")
}
elseif (-not $checkOnly -and -not $checkNativeLoad -and -not $isSimulationViewer) {
	$godotArgs += @("--script", "res://scripts/tools/headless_bootstrap.gd")
}
if ($Arguments.Count -gt 0) {
	$godotArgs += "--"
	$godotArgs += $Arguments
}
# Avoid false failures: check-only tails this file and aborts on any "Parse Error" line from a prior run.
if (($checkOnly -or $checkStatsDashboard -or $checkStatsAggregator) -and (Test-Path $logFile)) {
	Clear-Content -Path $logFile
}
$process = Start-Process -FilePath $godotExe -ArgumentList $godotArgs -PassThru -NoNewWindow

try {
	if ($checkOnly -or $checkStatsDashboard -or $checkStatsAggregator) {
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
