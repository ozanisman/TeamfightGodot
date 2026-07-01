param(
	[int]$trials = 5,
	[int]$sims_per_draft = 2,
	[int]$base_seed = 100000,
	[string]$stats_dir = "res://model_stats/stats_output_100k",
	[string]$godot_exe = "C:\Godot\godot.exe"
)

$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$runId = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
$run1Res = "res://model_stats/native_draft_validation_run1_$runId.csv"
$run2Res = "res://model_stats/native_draft_validation_run2_$runId.csv"
$run1Path = Join-Path $projectRoot "model_stats/native_draft_validation_run1_$runId.csv"
$run2Path = Join-Path $projectRoot "model_stats/native_draft_validation_run2_$runId.csv"

function Invoke-Harness([string]$outputRes) {
	$argList = @(
		"--path", $projectRoot,
		"--headless",
		"--script", "res://scripts/tools/native_draft_validation_harness.gd",
		"--",
		"--trials=$trials",
		"--sims-per-draft=$sims_per_draft",
		"--base-seed=$base_seed",
		"--stats-dir=$stats_dir",
		"--output=$outputRes"
	)
	$process = Start-Process -FilePath $godot_exe -ArgumentList $argList -PassThru -Wait
	if ($process.ExitCode -ne 0) {
		Write-Error "Harness run failed with exit code $($process.ExitCode)"
		exit 1
	}
}

Invoke-Harness $run1Res
Invoke-Harness $run2Res

$hash1 = (Get-FileHash $run1Path -Algorithm SHA256).Hash
$hash2 = (Get-FileHash $run2Path -Algorithm SHA256).Hash

if ($hash1 -eq $hash2) {
	Write-Host "Determinism check passed: outputs are identical."
	Remove-Item $run1Path -ErrorAction SilentlyContinue
	Remove-Item $run2Path -ErrorAction SilentlyContinue
	exit 0
} else {
	Write-Error "Determinism check failed: outputs differ."
	Write-Host "Run 1: $run1Path"
	Write-Host "Run 2: $run2Path"
	exit 1
}
