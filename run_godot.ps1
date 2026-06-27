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
$exportChampionSchema = $Arguments -contains "--export-champion-schema"
$checkNativeLoad = $Arguments -contains "--check-native-load"
$checkNativeSimulationTests = $Arguments -contains "--check-native-simulation-tests"
$checkDeterminism = $Arguments -contains "--check-determinism"
$checkBenchmark = $Arguments -contains "--check-benchmark"
$checkBenchmarkSharded = $Arguments -contains "--check-benchmark-sharded"
$checkBalancePatches = $Arguments -contains "--check-balance-patches"
$checkStatsDashboard = $Arguments -contains "--check-stats-dashboard"
$checkMainMenu = $Arguments -contains "--check-main-menu"
$checkDraftUi = $Arguments -contains "--check-draft-ui"
$checkStatsAggregator = $Arguments -contains "--check-stats-aggregator"
$checkStatsCsvDeterminism = $Arguments -contains "--check-stats-csv-determinism"
$generateStats = $Arguments -contains "--generate-stats"
$measureDraftCeiling = $Arguments -contains "--measure-draft-ceiling"
$verifyPairwiseSignal = $Arguments -contains "--verify-pairwise-signal"
$generateDraftProbeSignals = $Arguments -contains "--generate-draft-probe-signals"
$validatePickRecommendations = $Arguments -contains "--validate-pick-recommendations"
$generateDraftAwareTrainingData = $Arguments -contains "--generate-draft-aware-training-data"
$verifyDraftAwareSignal = $Arguments -contains "--verify-draft-aware-signal"
$validateRolloutConvergence = $Arguments -contains "--validate-rollout-convergence"
$abTestDraftStrategies = $Arguments -contains "--ab-test-draft-strategies"
$checkMatchTelemetry = $Arguments -contains "--check-match-telemetry"
$checkLargeProjectileDamage = $Arguments -contains "--check-large-projectile-damage"
$checkProjectilePayloads = $Arguments -contains "--check-projectile-payloads"
$validateNativeStrategy = $Arguments -contains "--validate-native-strategy"
$validateFullDraft = $Arguments -contains "--validate-full-draft"
$fullDraftABTest = $Arguments -contains "--full-draft-ab-test"
$fullDraftAblationTest = $Arguments -contains "--full-draft-ablation-test"
$fullDraftBanDiagnostic = $Arguments -contains "--full-draft-ban-diagnostic"
$testPartialCompScoring = $Arguments -contains "--test-partial-comp-scoring"
$checkFixtureFile = $false
foreach ($argument in $Arguments) {
	if ($argument -like "--fixture-file=*") {
		$checkFixtureFile = $true
		break
	}
}

# Helper functions for argument parsing
function Get-ArgString([string]$Prefix, [string]$DefaultValue) {
	foreach ($a in $Arguments) {
		if ($a -like "$Prefix*") {
			return $a.Substring($Prefix.Length)
		}
	}
	return $DefaultValue
}
function Get-ArgInt([string]$Prefix, [int]$DefaultValue) {
	foreach ($a in $Arguments) {
		if ($a -like "$Prefix*") {
			return [int]($a.Substring($Prefix.Length))
		}
	}
	return $DefaultValue
}
function Convert-ProjectPath([string]$PathValue) {
	if ($PathValue.StartsWith("res://")) {
		$relative = $PathValue.Substring("res://".Length).Replace("/", "\")
		return Join-Path $projectRoot $relative
	}
	if ([System.IO.Path]::IsPathRooted($PathValue)) {
		return $PathValue
	}
	return Join-Path $projectRoot $PathValue
}
function Get-ValidDepthCount([string]$DepthsValue) {
	$count = 0
	foreach ($part in $DepthsValue.Split(",")) {
		$depth = 0
		if ([int]::TryParse($part.Trim(), [ref]$depth) -and $depth -ge 1 -and $depth -le 5) {
			$count += 1
		}
	}
	if ($count -le 0) {
		return 5
	}
	return $count
}
function Assert-DurableOutput([string]$OutputPath, [int]$MinBytes, [int]$MinLines, [int]$ExactLines) {
	if (-not (Test-Path -LiteralPath $OutputPath)) {
		Write-Error "Expected output was not persisted after Godot exited: $OutputPath"
		exit 1
	}
	$item = Get-Item -LiteralPath $OutputPath
	if ($item.Length -lt $MinBytes) {
		Write-Error "Expected output is too small after Godot exited: $OutputPath ($($item.Length) bytes)"
		exit 1
	}
	if ($MinLines -gt 0 -or $ExactLines -gt 0) {
		$lineCount = (Get-Content -LiteralPath $OutputPath -ErrorAction Stop | Measure-Object -Line).Lines
		if ($MinLines -gt 0 -and $lineCount -lt $MinLines) {
			Write-Error "Expected output has too few lines after Godot exited: $OutputPath ($lineCount lines)"
			exit 1
		}
		if ($ExactLines -gt 0 -and $lineCount -ne $ExactLines) {
			Write-Error "Expected output has wrong line count after Godot exited: $OutputPath ($lineCount lines, expected $ExactLines)"
			exit 1
		}
	}
	Write-Host "Durable output verified: $OutputPath ($($item.Length) bytes)"
}
if ($checkOnly) {
	$timeoutSeconds = 15
}
elseif ($checkStatsDashboard) {
	$timeoutSeconds = 30
}
elseif ($checkMainMenu) {
	$timeoutSeconds = 30
}
elseif ($checkDraftUi) {
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
elseif ($checkProjectilePayloads) {
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
elseif ($abTestDraftStrategies) {
	$timeoutSeconds = 7200
}
elseif ($measureDraftCeiling) {
	$timeoutSeconds = 900
}
elseif ($verifyPairwiseSignal) {
	$timeoutSeconds = 300
}
elseif ($generateDraftProbeSignals) {
	$timeoutSeconds = 900
}
elseif ($validatePickRecommendations) {
	$timeoutSeconds = 900
}
elseif ($generateDraftAwareTrainingData) {
	$timeoutSeconds = 900
}
elseif ($verifyDraftAwareSignal) {
	$timeoutSeconds = 300
}
elseif ($validateRolloutConvergence) {
	$timeoutSeconds = 600
}
elseif ($validateNativeStrategy) {
	$timeoutSeconds = 30
}
elseif ($validateFullDraft) {
	$timeoutSeconds = 60
}
elseif ($fullDraftABTest) {
	$timeoutSeconds = 600
}
elseif ($fullDraftAblationTest) {
	$timeoutSeconds = 600
}
elseif ($fullDraftBanDiagnostic) {
	$timeoutSeconds = 300
}
elseif ($testPartialCompScoring) {
	$timeoutSeconds = 60
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
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkProjectilePayloads) {
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
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkMainMenu) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkDraftUi) {
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
elseif ($env:RUN_GODOT_TIMEOUT_SECONDS -and $fullDraftABTest) {
	[int]$timeoutSeconds = $env:RUN_GODOT_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_TIMEOUT_SECONDS -and $fullDraftAblationTest) {
	[int]$timeoutSeconds = $env:RUN_GODOT_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_TIMEOUT_SECONDS -and $fullDraftBanDiagnostic) {
	[int]$timeoutSeconds = $env:RUN_GODOT_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_TIMEOUT_SECONDS -and $testPartialCompScoring) {
	[int]$timeoutSeconds = $env:RUN_GODOT_TIMEOUT_SECONDS
}

# Ensure global class cache exists so headless script compilation can resolve class_name types.
$godotCacheFile = Join-Path $projectRoot ".godot\global_script_class_cache.cfg"
if (-not (Test-Path $godotCacheFile)) {
	Write-Host "Building Godot global class cache..."
	& $godotExe --path $projectRoot --headless --editor --quit
	if ($LASTEXITCODE -ne 0) {
		Write-Error "Godot editor cache generation failed with exit code $LASTEXITCODE"
		exit $LASTEXITCODE
	}
}

$godotArgs = @("--path", $projectRoot, "--log-file", $logFile)
$isMainMenu = $Arguments -contains "--main-menu"
$isSimulationViewer = $Arguments -contains "--simulation-viewer"
$isInteractiveGui = $isMainMenu -or $isSimulationViewer
if (-not $isInteractiveGui) {
	$godotArgs += @("--headless")
}
if ($isInteractiveGui) {
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
elseif ($exportChampionSchema) {
	$godotArgs += @("--script", "res://scripts/tools/export_champion_schema.gd")
}
elseif ($checkNativeLoad) {
	$godotArgs += @("--script", "res://scripts/tools/check_native_load.gd")
}
elseif ($checkNativeSimulationTests) {
	$godotArgs += @("--script", "res://scripts/tools/check_native_simulation_tests.gd")
}
elseif ($checkMatchTelemetry) {
	$godotArgs += @("--script", "res://scripts/tools/check_match_telemetry.gd")
}
elseif ($checkLargeProjectileDamage) {
	$godotArgs += @("--script", "res://scripts/tools/check_large_projectile_damage.gd")
}
elseif ($checkProjectilePayloads) {
	$godotArgs += @("--script", "res://scripts/tools/check_projectile_payloads.gd")
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
elseif ($checkMainMenu) {
	$godotArgs += @("--script", "res://scripts/tools/check_main_menu_load.gd")
}
elseif ($checkDraftUi) {
	$godotArgs += @("--script", "res://scripts/tools/check_draft_ui_load.gd")
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
elseif ($measureDraftCeiling) {
	$godotArgs += @("--script", "res://scripts/tools/measure_draft_ceiling.gd")
}
elseif ($verifyPairwiseSignal) {
	$godotArgs += @("--script", "res://scripts/tools/verify_pairwise_signal.gd")
}
elseif ($generateDraftProbeSignals) {
	$godotArgs += @("--script", "res://scripts/tools/generate_draft_probe_signals.gd")
}
elseif ($validatePickRecommendations) {
	$godotArgs += @("--script", "res://scripts/tools/validate_pick_recommendations.gd")
}
elseif ($generateDraftAwareTrainingData) {
	$godotArgs += @("--script", "res://scripts/tools/generate_draft_aware_training_data.gd")
	$depthsArg = Get-ArgString "--depths=" "1,2,3,4,5"
	$godotArgs += "--depths=$depthsArg"
	$Arguments = $Arguments | Where-Object { $_ -notlike "--depths=*" }
	$rolloutsArg = Get-ArgString "--rollouts-per-state=" "50"
	$godotArgs += "--rollouts-per-state=$rolloutsArg"
	$Arguments = $Arguments | Where-Object { $_ -notlike "--rollouts-per-state=*" }
}
elseif ($verifyDraftAwareSignal) {
	$godotArgs += @("--script", "res://scripts/tools/verify_draft_aware_signal.gd")
}
elseif ($validateRolloutConvergence) {
	$godotArgs += @("--script", "res://scripts/tools/validate_rollout_convergence.gd")
	$rolloutCountsArg = Get-ArgString "--rollout-counts=" "10,25,50,100,200,500"
	$godotArgs += "--rollout-counts=$rolloutCountsArg"
	$Arguments = $Arguments | Where-Object { $_ -notlike "--rollout-counts=*" }
}
elseif ($validateNativeStrategy) {
	$godotArgs += @("--script", "res://scripts/tools/file_based_validation.gd")
}
elseif ($validateFullDraft) {
	$godotArgs += @("--script", "res://scripts/tools/full_draft_validation.gd")
}
elseif ($fullDraftABTest) {
	$godotArgs += @("--script", "res://scripts/tools/full_draft_ab_test.gd")
}
elseif ($fullDraftAblationTest) {
	$godotArgs += @("--script", "res://scripts/tools/full_draft_ablation_test.gd")
}
elseif ($fullDraftBanDiagnostic) {
	$godotArgs += @("--script", "res://scripts/tools/full_draft_ban_diagnostic.gd")
}
elseif ($testPartialCompScoring) {
	$godotArgs += @("--script", "res://scripts/tools/test_partial_comp_scoring.gd")
}
elseif ($abTestDraftStrategies) {
	$godotArgs += @("--script", "res://scripts/tools/ab_test_draft_strategies.gd")
}
elseif (-not $checkOnly -and -not $exportChampionSchema -and -not $checkNativeLoad -and -not $checkNativeSimulationTests -and -not $checkMatchTelemetry -and -not $checkMainMenu -and -not $checkDraftUi -and -not $isInteractiveGui) {
	$godotArgs += @("--script", "res://scripts/tools/headless_bootstrap.gd")
}
if ($Arguments.Count -gt 0) {
	$godotArgs += "--"
	$godotArgs += $Arguments
}
# Avoid false failures: check-only tails this file and aborts on any "Parse Error" line from a prior run.
if (($checkOnly -or $checkNativeSimulationTests -or $checkStatsDashboard -or $checkMainMenu -or $checkDraftUi -or $checkStatsAggregator -or $checkStatsCsvDeterminism -or $generateDraftAwareTrainingData -or $verifyDraftAwareSignal) -and (Test-Path $logFile)) {
	Clear-Content -Path $logFile
}
$process = Start-Process -FilePath $godotExe -ArgumentList $godotArgs -PassThru -NoNewWindow

try {
	if ($checkOnly -or $checkNativeSimulationTests -or $checkStatsDashboard -or $checkMainMenu -or $checkDraftUi -or $checkStatsAggregator -or $checkStatsCsvDeterminism) {
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
	if ($checkNativeLoad -or $checkNativeSimulationTests -or $checkMatchTelemetry -or $checkLargeProjectileDamage -or $checkProjectilePayloads -or $checkDeterminism -or $checkBenchmark -or $checkBalancePatches -or $checkFixtureFile -or $checkStatsDashboard -or $checkMainMenu -or $checkDraftUi -or $checkStatsCsvDeterminism -or $generateDraftAwareTrainingData -or $verifyDraftAwareSignal) {
		if (Test-Path $logFile) {
			$tail = Get-Content -Path $logFile -Tail 200 -ErrorAction SilentlyContinue
			$failurePattern = "SCRIPT ERROR:|Parse Error:|Compilation failed|Failed to load script|CrashHandlerException|Program crashed with signal|GDExtension load failed|Native simulation backend unavailable|Failed to open fixture file|Failed to open JSON file|Fixture .*mismatch|Fixture parity failed|Replay determinism failed|balance_patch_suite: FAILED|native_simulation_tests: (?!OK)|check_match_telemetry: .*invalid|check_match_telemetry: missing|check_match_telemetry: bad|check_large_projectile_damage: (?!OK)|check_projectile_payloads: (?!OK)|check_stats_csv_determinism: (?!OK)|StatsDashboardLoader: (cannot open|missing CSV header|column count mismatch|no data rows)|StatsDashboardLoader fixture load failed|stats_dashboard: .*failed|main_menu: .*missing|draft_ui: .*missing|check_main_menu_load: FAILED|check_draft_ui_load: FAILED"
			if ($tail -match $failurePattern) {
				Write-Error "Godot check failed. See $logFile."
				exit 1
			}
		}
	}
	if ($generateDraftAwareTrainingData) {
		$outputPath = Convert-ProjectPath (Get-ArgString "--output=" "res://model_stats/draft_aware_training_250k/draft_aware_training.csv")
		$statesPerDepth = [Math]::Max(1, (Get-ArgInt "--states-per-depth=" 25))
		$depthCount = Get-ValidDepthCount (Get-ArgString "--depths=" "1,2,3,4,5")
		Assert-DurableOutput $outputPath 2 0 (($statesPerDepth * $depthCount) + 1)
	}
	elseif ($verifyDraftAwareSignal) {
		$outputPath = Convert-ProjectPath (Get-ArgString "--output=" "res://stats_output/draft_aware_model.csv")
		Assert-DurableOutput $outputPath 2 2 0
	}
	elseif ($validateNativeStrategy) {
		$outputPath = Convert-ProjectPath "res://draft_ai_validation_report.txt"
		Assert-DurableOutput $outputPath 100 10 0
	}
	elseif ($validateFullDraft) {
		$outputPath = Convert-ProjectPath "res://draft_validation_report.txt"
		Assert-DurableOutput $outputPath 100 10 0
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
