<# 
.SYNOPSIS
    CNCS Unified Build Script
.DESCRIPTION
    Builds CNCS using CMake + vcpkg. Replaces the old Zig/VS dual build.
.NOTES
    Requires: Visual Studio 2022, CMake 3.25+, vcpkg
#>

param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    
    [switch]$Clean,
    
    [switch]$RunTests,
    
    [string]$CMakeGenerator = 'Visual Studio 17 2022',
    
    [string]$Architecture = 'x64',
    
    [switch]$Verbose
)

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$ProjectRoot = Resolve-Path "$ScriptDir\.."
$BuildDir = "$ProjectRoot\build"
$VcpkgRoot = $env:VCPKG_ROOT

if (-not $VcpkgRoot) {
    Write-Warning "VCPKG_ROOT not set. Attempting to use vcpkg from PATH..."
    $VcpkgExe = Get-Command vcpkg -ErrorAction SilentlyContinue
    if ($VcpkgExe) {
        $VcpkgRoot = Split-Path (Split-Path $VcpkgExe.Source)
        Write-Host "Found vcpkg at: $VcpkgRoot"
    } else {
        Write-Error "vcpkg not found. Please install vcpkg and set VCPKG_ROOT environment variable."
        exit 1
    }
}

$ToolchainFile = "$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"
if (-not (Test-Path $ToolchainFile)) {
    Write-Error "vcpkg toolchain not found at: $ToolchainFile"
    exit 1
}

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "CNCS Build Script" -ForegroundColor Cyan
Write-Host "  Configuration: $Configuration" -ForegroundColor Cyan
Write-Host "  Generator: $CMakeGenerator" -ForegroundColor Cyan
Write-Host "  Architecture: $Architecture" -ForegroundColor Cyan
Write-Host "  vcpkg: $VcpkgRoot" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# Clean if requested
if ($Clean) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
    }
}

# Create build directory
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Configure
Write-Host "Configuring with CMake..." -ForegroundColor Green
$CMakeArgs = @(
    "-S", $ProjectRoot
    "-B", $BuildDir
    "-G", $CMakeGenerator
    "-A", $Architecture
    "-DCMAKE_BUILD_TYPE=$Configuration"
    "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile"
    "-DVCPKG_TARGET_TRIPLET=x64-windows"
)

if ($RunTests) {
    $CMakeArgs += "-DBUILD_TESTS=ON"
}

if ($Verbose) {
    $CMakeArgs += "-DCMAKE_VERBOSE_MAKEFILE=ON"
}

& cmake @CMakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configure failed"
    exit $LASTEXITCODE
}

# Build
Write-Host "Building..." -ForegroundColor Green
$BuildArgs = @(
    "--build", $BuildDir
    "--config", $Configuration
)

if ($Verbose) {
    $BuildArgs += "--verbose"
}

& cmake @BuildArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
    exit $LASTEXITCODE
}

# Run tests if requested
if ($RunTests) {
    Write-Host "Running tests..." -ForegroundColor Green
    & ctest --test-dir $BuildDir --output-on-failure -C $Configuration
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Tests failed"
        exit $LASTEXITCODE
    }
}

$OutputExe = "$BuildDir\bin\$Configuration\CNCS.exe"
if (Test-Path $OutputExe) {
    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host "Build Successful!" -ForegroundColor Green
    Write-Host "Output: $OutputExe" -ForegroundColor Cyan
    Write-Host "==========================================" -ForegroundColor Cyan
} else {
    Write-Warning "Build completed but output executable not found at expected location"
}

exit 0