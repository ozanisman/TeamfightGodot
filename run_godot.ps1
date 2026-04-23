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
if ($checkOnly) {
	$timeoutSeconds = 15
}
if ($env:RUN_GODOT_CHECK_TIMEOUT_SECONDS -and $checkOnly) {
	[int]$timeoutSeconds = $env:RUN_GODOT_CHECK_TIMEOUT_SECONDS
}
elseif ($env:RUN_GODOT_TIMEOUT_SECONDS -and -not $checkOnly) {
	[int]$timeoutSeconds = $env:RUN_GODOT_TIMEOUT_SECONDS
}

$godotArgs = @("--headless", "--path", $projectRoot, "--log-file", $logFile)
if ($checkOnly) {
	$godotArgs += @("--script", "res://scripts/tools/check_only.gd")
}
if ($Arguments.Count -gt 0) {
	$godotArgs += "--"
	$godotArgs += $Arguments
}
$process = Start-Process -FilePath $godotExe -ArgumentList $godotArgs -PassThru -NoNewWindow

try {
	if ($checkOnly) {
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
		Wait-Process -Id $process.Id -Timeout $timeoutSeconds
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
