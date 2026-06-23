# Full perf iteration gate: build, check-only, fixtures, 5x bench w=1 and w=8, compare/update baseline.
param(
	[string]$StepName = "step",
	[switch]$SkipBuild,
	[switch]$InitialBaseline
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$gate = Join-Path $PSScriptRoot "run_perf_iteration_gate.ps1"
$baselineJson = Join-Path $projectRoot "scripts\tools\perf_gate_baseline.json"

if (-not $SkipBuild) {
	Write-Host "=== cmake Release ==="
	cmake --build (Join-Path $projectRoot "native\build") --config Release
	if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "=== check-only ==="
& (Join-Path $projectRoot "run_godot.ps1") -- --check-only
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "=== fixtures 9/9 ==="
& (Join-Path $projectRoot "run_godot.ps1") -- --fixture-file=res://fixtures/goldens/match_fixtures.json
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($InitialBaseline) {
	Write-Host "=== initial baseline capture (no compare) ==="
	& $gate -StepName $StepName -Workers 1 -Runs 5 -CaptureBaseline -SkipCompare
	if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
	& $gate -StepName $StepName -Workers 8 -Runs 5 -CaptureBaseline -SkipCompare
	if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
	Write-Host "Initial baseline captured at $baselineJson"
	exit 0
}

Write-Host "=== bench workers=1 (5 runs) ==="
& $gate -StepName $StepName -Workers 1 -Runs 5
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "=== bench workers=8 (5 runs) ==="
& $gate -StepName $StepName -Workers 8 -Runs 5
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $gate -StepName $StepName -Workers 1 -Runs 5 -CaptureBaseline -SkipCompare
& $gate -StepName $StepName -Workers 8 -Runs 5 -CaptureBaseline -SkipCompare
Write-Host "PASS: $StepName - baseline updated."
exit 0
