param(
	[ValidateSet("Debug", "Release")]
	[string]$Config = "Release",
	[string]$GodotCppDir = $env:GODOT_CPP_DIR,
	[string]$GodotCppLib = $env:GODOT_CPP_LIB,
	[string]$BuildDir = "$(Join-Path $PSScriptRoot "build")",
	[string]$Generator = "Visual Studio 17 2022",
	[string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($GodotCppDir)) {
	throw "Set GODOT_CPP_DIR or pass -GodotCppDir to point at a godot-cpp checkout."
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
	throw "cmake was not found on PATH."
}

$GodotCppDir = (Resolve-Path $GodotCppDir).Path

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

if ([string]::IsNullOrWhiteSpace($GodotCppLib)) {
	$GodotCppLib = Get-ChildItem (Join-Path $GodotCppDir "bin") -Filter "libgodot-cpp*.lib" | Sort-Object Name | Select-Object -First 1 -ExpandProperty FullName
} else {
	$GodotCppLib = (Resolve-Path $GodotCppLib).Path
}

$cmakeExe = (Get-Command cmake).Source

$cmakeArgs = @(
	"-S", $PSScriptRoot,
	"-B", $BuildDir,
	"-G", $Generator,
	"-A", $Platform,
	"-DGODOT_CPP_DIR=$GodotCppDir"
)

if (-not [string]::IsNullOrWhiteSpace($GodotCppLib)) {
	$cmakeArgs += "-DGODOT_CPP_LIB=$GodotCppLib"
}

& $cmakeExe @cmakeArgs
& $cmakeExe --build $BuildDir --config $Config
