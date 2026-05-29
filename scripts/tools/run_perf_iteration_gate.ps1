# Sequential benchmark gate for perf iteration (5 runs default, compare medians vs baseline).
param(
	[string]$StepName = "baseline",
	[int]$Workers = 1,
	[int]$Runs = 5,
	[int]$BatchCount = 2000,
	[int]$TeamSize = 5,
	[string]$BaselineJson = "",
	[switch]$CaptureBaseline,
	[switch]$SkipCompare
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runGodot = Join-Path $projectRoot "run_godot.ps1"
$godotLog = Join-Path $projectRoot "logs\godot.log"
$outDir = Join-Path $projectRoot "logs\perf_gate"
$null = New-Item -ItemType Directory -Force -Path $outDir

if ([string]::IsNullOrWhiteSpace($BaselineJson)) {
	$BaselineJson = Join-Path $projectRoot "scripts\tools\perf_gate_baseline.json"
}

function Get-Median([double[]]$Values) {
	if ($Values.Count -eq 0) { return 0.0 }
	$sorted = @($Values | Sort-Object)
	$mid = [int][Math]::Floor($sorted.Count / 2)
	if ($sorted.Count % 2 -eq 1) {
		return [double]$sorted[$mid]
	}
	return ([double]$sorted[$mid - 1] + [double]$sorted[$mid]) / 2.0
}

function Parse-BenchJson([string]$Text) {
	$anchor = $Text.IndexOf('"matches_per_sec"')
	if ($anchor -lt 0) {
		throw "No benchmark JSON (matches_per_sec) found in output."
	}
	$start = $Text.LastIndexOf("{", $anchor)
	if ($start -lt 0) {
		throw "No JSON object start found before matches_per_sec."
	}
	$depth = 0
	for ($i = $start; $i -lt $Text.Length; $i++) {
		$c = $Text[$i]
		if ($c -eq "{") {
			$depth++
		} elseif ($c -eq "}") {
			$depth--
			if ($depth -eq 0) {
				return $Text.Substring($start, $i - $start + 1) | ConvertFrom-Json
			}
		}
	}
	throw "Unbalanced JSON in benchmark output."
}

function Stop-StrayGodot {
	Get-Process -Name "Godot*" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}

$rates = New-Object System.Collections.Generic.List[double]
$durations = New-Object System.Collections.Generic.List[double]
$runRecords = @()

Write-Host "perf_gate: step=$StepName workers=$Workers runs=$Runs batch=$BatchCount"

for ($i = 1; $i -le $Runs; $i++) {
	Stop-StrayGodot
	Start-Sleep -Seconds 1
	$logPath = Join-Path $outDir "${StepName}_w${Workers}_run${i}.log"
	Write-Host "  run $i/$Runs ..."
	if (Test-Path $godotLog) {
		Clear-Content -Path $godotLog -ErrorAction SilentlyContinue
	}
	Push-Location $projectRoot
	try {
		& $runGodot -- --check-benchmark "--batch-count=$BatchCount" "--team-size=$TeamSize" --bench-skip-summaries "--workers=$Workers" | Out-Null
		$exitCode = $LASTEXITCODE
	} finally {
		Pop-Location
	}
	$logText = ""
	if (Test-Path $godotLog) {
		$logText = Get-Content -Path $godotLog -Raw -ErrorAction SilentlyContinue
	}
	Set-Content -Path $logPath -Value $logText -Encoding UTF8
	if ($exitCode -ne 0) {
		Write-Error "Benchmark run $i failed (exit $exitCode). See $logPath"
	}
	$result = Parse-BenchJson $logText
	$mps = [double]$result.matches_per_sec
	$dur = [double]$result.duration_sec
	$rates.Add($mps) | Out-Null
	$durations.Add($dur) | Out-Null
	$record = [ordered]@{
		step = $StepName
		workers = $Workers
		run = $i
		matches_per_sec = $mps
		duration_sec = $dur
		batch_count = [int]$result.batch_count
		timestamp = (Get-Date).ToString("o")
	}
	$jsonOut = Join-Path $outDir "${StepName}_w${Workers}_run${i}.json"
	$record | ConvertTo-Json | Set-Content -Path $jsonOut -Encoding UTF8
	$runRecords += $record
	Write-Host ("    {0:N2} m/s  ({1:N2}s)" -f $mps, $dur)
}

$medianMps = Get-Median @($rates.ToArray())
$minMps = ($rates | Measure-Object -Minimum).Minimum
$maxMps = ($rates | Measure-Object -Maximum).Maximum

$summary = [ordered]@{
	step = $StepName
	workers = $Workers
	runs = $Runs
	batch_count = $BatchCount
	team_size = $TeamSize
	median_matches_per_sec = $medianMps
	min_matches_per_sec = $minMps
	max_matches_per_sec = $maxMps
	runs_detail = $runRecords
	timestamp = (Get-Date).ToString("o")
}
$summaryPath = Join-Path $outDir "${StepName}_w${Workers}_summary.json"
$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $summaryPath -Encoding UTF8

Write-Host ("summary: median={0:N2} min={1:N2} max={2:N2} m/s" -f $medianMps, $minMps, $maxMps)

if ($CaptureBaseline) {
	$baseline = @{
		note = "Quiet host baseline; update after each accepted optimization step."
		captured = (Get-Date).ToString("o")
		git_sha = (git -C $projectRoot rev-parse --short HEAD 2>$null)
		w1 = @{ median_matches_per_sec = 0.0; min = 0.0; max = 0.0 }
		w8 = @{ median_matches_per_sec = 0.0; min = 0.0; max = 0.0 }
	}
	if (Test-Path $BaselineJson) {
		$baseline = Get-Content $BaselineJson -Raw | ConvertFrom-Json
	}
	if ($Workers -eq 1) {
		$baseline.w1 = @{
			median_matches_per_sec = $medianMps
			min = $minMps
			max = $maxMps
			step = $StepName
		}
	} elseif ($Workers -eq 8) {
		$baseline.w8 = @{
			median_matches_per_sec = $medianMps
			min = $minMps
			max = $maxMps
			step = $StepName
		}
	}
	$baseline | ConvertTo-Json -Depth 6 | Set-Content -Path $BaselineJson -Encoding UTF8
	Write-Host "Updated baseline ($BaselineJson) workers=$Workers"
	Stop-StrayGodot
	exit 0
}

if ($SkipCompare) {
	Stop-StrayGodot
	exit 0
}

if (-not (Test-Path $BaselineJson)) {
	Write-Error "Baseline file not found: $BaselineJson (run with -CaptureBaseline first)"
}

$base = Get-Content $BaselineJson -Raw | ConvertFrom-Json
$baseMedian = 0.0
$baseStep = "baseline"
if ($Workers -eq 1) {
	$baseMedian = [double]$base.w1.median_matches_per_sec
	$baseStep = [string]$base.w1.step
} else {
	$baseMedian = [double]$base.w8.median_matches_per_sec
	$baseStep = [string]$base.w8.step
}

$delta = $medianMps - $baseMedian
Write-Host ("compare vs baseline step '{0}': {1:N2} -> {2:N2} ({3:+0.00;-0.00} m/s)" -f $baseStep, $baseMedian, $medianMps, $delta)

if ($medianMps -lt $baseMedian) {
	$reportPath = Join-Path $outDir "${StepName}_REGRESSION.md"
	@(
		"# Perf gate regression: $StepName",
		"",
		"**Stopped:** median workers=$Workers dropped ($baseMedian to $medianMps m/s).",
		"**Baseline step:** $baseStep",
		"**Git:** $(git -C $projectRoot rev-parse HEAD 2>$null)",
		"",
		"| Run | m/s | duration_sec |",
		"|-----|-----|--------------|"
	) + ($runRecords | ForEach-Object { "| $($_.run) | $([math]::Round($_.matches_per_sec, 2)) | $([math]::Round($_.duration_sec, 2)) |" }) + @(
		"",
		"Change kept in tree. Revert only with explicit approval.",
		"Re-run confirmatory gate on a quiet host before discarding the optimization."
	) | Set-Content -Path $reportPath -Encoding UTF8
	Write-Host "REGRESSION: wrote $reportPath"
	Stop-StrayGodot
	exit 1
}

Stop-StrayGodot
exit 0
