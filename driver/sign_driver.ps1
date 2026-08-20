<# 
.SYNOPSIS
    CNCS Kernel Driver Signing Script
.DESCRIPTION
    Signs the kernel driver (CNCS_drv.sys) with an EV code signing certificate.
    Requires: Windows SDK (signtool), valid EV certificate (.pfx or hardware token).
.NOTES
    For production: Use an EV (Extended Validation) certificate from a trusted CA.
    For testing: Use test signing mode (bcdedit /set testsigning on) with a self-signed cert.
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$DriverPath,
    
    [Parameter(Mandatory=$false)]
    [string]$CertPath,
    
    [Parameter(Mandatory=$false)]
    [string]$CertPassword,
    
    [Parameter(Mandatory=$false)]
    [string]$TimestampServer = "http://timestamp.sectigo.com",
    
    [Parameter(Mandatory=$false)]
    [string]$CertThumbprint,
    
    [switch]$TestMode,
    
    [switch]$VerifyOnly
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $DriverPath)) {
    Write-Error "Driver not found: $DriverPath"
    exit 1
}

$SignTool = Join-Path (${env:ProgramFiles(x86)}) "Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe"
if (-not (Test-Path $SignTool)) {
    # Try to find signtool in PATH
    $SignTool = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if (-not $SignTool) {
        Write-Error "signtool.exe not found. Install Windows SDK."
        exit 1
    }
    $SignTool = $SignTool.Source
}

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "CNCS Driver Signing" -ForegroundColor Cyan
Write-Host "  Driver: $DriverPath" -ForegroundColor Cyan
Write-Host "  SignTool: $SignTool" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

if ($VerifyOnly) {
    Write-Host "Verifying signature..." -ForegroundColor Green
    & $SignTool verify /pa /v "$DriverPath"
    exit $LASTEXITCODE
}

if ($TestMode) {
    Write-Host "Test signing mode (self-signed certificate)..." -ForegroundColor Yellow
    
    # Create test certificate if not exists
    $TestCert = "CNCS_Test_Cert"
    $CertExists = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq "CN=$TestCert" }
    
    if (-not $CertExists) {
        Write-Host "Creating test certificate..." -ForegroundColor Green
        $cert = New-SelfSignedCertificate -Type Custom -Subject "CN=$TestCert" -KeyUsage DigitalSignature -FriendlyName "CNCS Test Certificate" -CertStoreLocation "Cert:\CurrentUser\My" -NotAfter (Get-Date).AddYears(10) -HashAlgorithm SHA256
        $CertThumbprint = $cert.Thumbprint
    } else {
        $CertThumbprint = $CertExists.Thumbprint
        Write-Host "Using existing test certificate: $CertThumbprint" -ForegroundColor Green
    }
    
    # Sign with test certificate
    & $SignTool sign /v /fd SHA256 /sha1 $CertThumbprint /tr "http://timestamp.digicert.com" /td SHA256 "$DriverPath"
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Test signing successful!" -ForegroundColor Green
        Write-Host "NOTE: Enable test signing mode on target machine:" -ForegroundColor Yellow
        Write-Host "  bcdedit /set testsigning on" -ForegroundColor Yellow
        Write-Host "  (Requires reboot)" -ForegroundColor Yellow
    }
    
    exit $LASTEXITCODE
}

# Production signing with EV certificate
if (-not $CertThumbprint -and -not $CertPath) {
    Write-Error "Production signing requires either -CertThumbprint (for hardware token/cert store) or -CertPath (for .pfx file)"
    exit 1
}

$SignArgs = @("sign", "/v", "/fd", "SHA256", "/tr", $TimestampServer, "/td", "SHA256")

if ($CertThumbprint) {
    $SignArgs += "/sha1", $CertThumbprint
} elseif ($CertPath) {
    if (-not (Test-Path $CertPath)) {
        Write-Error "Certificate file not found: $CertPath"
        exit 1
    }
    $SignArgs += "/f", $CertPath
    if ($CertPassword) {
        $SignArgs += "/p", $CertPassword
    }
}

$SignArgs += $DriverPath

Write-Host "Signing with production certificate..." -ForegroundColor Green
& $SignTool @SignArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "Production signing successful!" -ForegroundColor Green
    
    # Verify
    Write-Host "Verifying signature..." -ForegroundColor Green
    & $SignTool verify /pa /v "$DriverPath"
} else {
    Write-Error "Signing failed"
}

exit $LASTEXITCODE