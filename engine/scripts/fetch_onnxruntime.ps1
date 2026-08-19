<#
.SYNOPSIS
  Downloads Microsoft's official prebuilt ONNX Runtime C++ distribution for Windows x64
  and unpacks it into engine/onnxruntime/. Deliberately NOT vcpkg's onnxruntime port --
  see docs/ARCHITECTURE.md for why (that port builds from source and is a common source
  of protobuf version collisions with other vcpkg dependencies).

.PARAMETER Version
  Optional exact ONNX Runtime release tag, e.g. "v1.20.1". Defaults to latest release.
#>
param(
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$destDir = Join-Path $root "onnxruntime"

if (Test-Path (Join-Path $destDir "include\onnxruntime_cxx_api.h")) {
    Write-Output "ONNX Runtime already present at $destDir - skipping download."
    exit 0
}

if ($Version -eq "") {
    Write-Output "Looking up latest ONNX Runtime release..."
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/microsoft/onnxruntime/releases/latest" -UseBasicParsing
    $Version = $release.tag_name
} else {
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/microsoft/onnxruntime/releases/tags/$Version" -UseBasicParsing
}

$versionNumber = $Version.TrimStart("v")
$assetName = "onnxruntime-win-x64-$versionNumber.zip"
$asset = $release.assets | Where-Object { $_.name -eq $assetName } | Select-Object -First 1

if (-not $asset) {
    Write-Error "Could not find asset '$assetName' in release $Version. Available assets: $($release.assets.name -join ', ')"
    exit 1
}

$zipPath = Join-Path $env:TEMP "$assetName"
Write-Output "Downloading $($asset.browser_download_url) ..."
Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $zipPath -UseBasicParsing

Write-Output "Extracting to $destDir ..."
$extractTmp = Join-Path $env:TEMP "onnxruntime-extract-$([guid]::NewGuid())"
Expand-Archive -Path $zipPath -DestinationPath $extractTmp -Force

$innerDir = Get-ChildItem -Path $extractTmp -Directory | Select-Object -First 1
if (Test-Path $destDir) { Remove-Item $destDir -Recurse -Force }
Move-Item $innerDir.FullName $destDir

Remove-Item $zipPath -Force
Remove-Item $extractTmp -Recurse -Force -ErrorAction SilentlyContinue

Write-Output "ONNX Runtime $Version installed at $destDir"
