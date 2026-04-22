param(
	[Parameter(ValueFromRemainingArguments = $true)]
	[string[]]$Arguments
)

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$godotExe = "C:\Godot\godot.exe"
$logFile = Join-Path $projectRoot "godot.log"
$timeoutSeconds = 120
if ($env:RUN_GODOT_TIMEOUT_SECONDS) {
	[int]$timeoutSeconds = $env:RUN_GODOT_TIMEOUT_SECONDS
}
elseif ($Arguments -contains "--check-only") {
	$timeoutSeconds = 30
}

$godotArgs = @("--headless", "--path", $projectRoot, "--log-file", $logFile) + $Arguments
$process = Start-Process -FilePath $godotExe -ArgumentList $godotArgs -PassThru -NoNewWindow

try {
	Wait-Process -Id $process.Id -Timeout $timeoutSeconds
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
