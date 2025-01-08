param([string]$buildPreset="release", [string]$configPreset="default", [string]$packConfig, [string]$packPreset, [string]$toolset)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

Write-Warning "Using buildPreset='$buildPreset' and configPreset='$configPreset'"

$projectRoot = Resolve-Path "$PSScriptRoot/.."

if (!$packPreset -and !$packConfig) {
    $packConfig=$buildPreset
}
$toolsetParam=""
if ($toolset) {
    $toolsetParam="-T $toolset"
}

Set-Location $projectRoot
Write-Warning "Starting cmake"
cmake -B build -S "$projectRoot" --preset $configPreset --fresh $toolsetParam

Write-Warning "Starting cmake --build with preset '$($buildPreset)'"
Invoke-Expression "cmake --build '$projectRoot/build' --preset '$($buildPreset)'"

Set-Location "$projectRoot/build"

if ($packPreset) {
    Write-Warning "Starting cpack with preset '$($packPreset)'"
    cpack --preset "$($packPreset)"
} elseif ($packConfig) {
    Write-Warning "Starting cpack with config '$($packConfig)'"
    cpack -C "$($packConfig)"
}

Set-Location $projectRoot
