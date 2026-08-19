<#
.SYNOPSIS
  One-time local vcpkg setup: clones microsoft/vcpkg into engine/vcpkg/ (gitignored,
  not a submodule) and bootstraps the vcpkg.exe tool. CMakeLists.txt auto-detects this
  directory and sets it as the toolchain file.
#>
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$vcpkgDir = Join-Path $root "vcpkg"

if (Test-Path (Join-Path $vcpkgDir "vcpkg.exe")) {
    Write-Output "vcpkg already bootstrapped at $vcpkgDir"
    exit 0
}

if (-not (Test-Path $vcpkgDir)) {
    git clone --depth 1 https://github.com/microsoft/vcpkg.git $vcpkgDir
}

& "$vcpkgDir\bootstrap-vcpkg.bat"

Write-Output ""
Write-Output "vcpkg ready at $vcpkgDir"
Write-Output "Next: cd engine; .\vcpkg\vcpkg.exe install --triplet x64-windows --x-install-root=vcpkg_installed"
