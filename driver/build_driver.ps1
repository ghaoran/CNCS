<# 
.SYNOPSIS
    CNCS Kernel Driver Build Script
.DESCRIPTION
    Builds the kernel driver using MSBuild (requires WDK + Visual Studio 2022).
.NOTES
    Requires: Windows Driver Kit (WDK) 10.0.22621+, Visual Studio 2022 with C++ Desktop workload
#>

param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    
    [switch]$Clean,
    
    [switch]$Sign,
    
    [string]$CertThumbprint,
    
    [string]$CertPath,
    
    [string]$CertPassword,
    
    [switch]$TestSign
)

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$ProjectFile = "$ScriptDir\CNCS_drv.vcxproj"

if (-not (Test-Path $ProjectFile)) {
    Write-Error "Driver project not found: $ProjectFile"
    exit 1
}

# Check for WDK
# 自动探测最新可用 WDK build 目录（与驱动工程一致，不锁定特定版本号）
$WdkBuildRoot = "${env:ProgramFiles(x86)}\Windows Kits\10\build"
$WdkPath = Get-ChildItem $WdkBuildRoot -Directory -ErrorAction SilentlyContinue |
             Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName

if (-not $WdkPath -or -not (Test-Path $WdkPath)) {
    Write-Warning "WDK not found under $WdkBuildRoot"
    Write-Warning "Driver build may fail without WDK installed"
}

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "CNCS Driver Build" -ForegroundColor Cyan
Write-Host "  Configuration: $Configuration" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

if ($Clean) {
    Write-Host "Cleaning..." -ForegroundColor Yellow
    & msbuild "$ProjectFile" /t:Clean /p:Configuration=$Configuration /p:Platform=x64
}

Write-Host "Building driver..." -ForegroundColor Green
$BuildResult = & msbuild "$ProjectFile" /t:Build /p:Configuration=$Configuration /p:Platform=x64 /v:minimal

if ($LASTEXITCODE -ne 0) {
    Write-Error "Driver build failed"
    exit $LASTEXITCODE
}

$OutputDriver = "$ScriptDir\x64\$Configuration\CNCS_drv.sys"
if (Test-Path $OutputDriver) {
    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host "Driver Build Successful!" -ForegroundColor Green
    Write-Host "Output: $OutputDriver" -ForegroundColor Cyan
    Write-Host "==========================================" -ForegroundColor Cyan
    
    if ($Sign) {
        Write-Host "Signing driver..." -ForegroundColor Green
        $SignArgs = @("-DriverPath", $OutputDriver)
        if ($TestSign) {
            $SignArgs += "-TestMode"
        } else {
            if ($CertThumbprint) { $SignArgs += "-CertThumbprint", $CertThumbprint }
            if ($CertPath) { $SignArgs += "-CertPath", $CertPath }
            if ($CertPassword) { $SignArgs += "-CertPassword", $CertPassword }
        }
        & powershell -ExecutionPolicy Bypass -File "$ScriptDir\sign_driver.ps1" @SignArgs
    }
} else {
    Write-Warning "Build completed but driver not found at expected location"
}

exit 0