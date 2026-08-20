@echo off
REM CNCS Driver Build Wrapper - calls PowerShell build script
REM Usage: build_driver.bat [Debug|Release] [clean] [sign] [testsign] [thumbprint=...] [certpath=...] [certpass=...]

set CONFIGURATION=Release
set CLEAN=
set SIGN=
set TESTSIGN=
set THUMBPRINT=
set CERTPATH=
set CERTPASS=

:parse_args
if "%~1"=="" goto :run_build
if /i "%~1"=="debug" set CONFIGURATION=Debug & shift & goto :parse_args
if /i "%~1"=="release" set CONFIGURATION=Release & shift & goto :parse_args
if /i "%~1"=="clean" set CLEAN=-Clean & shift & goto :parse_args
if /i "%~1"=="sign" set SIGN=-Sign & shift & goto :parse_args
if /i "%~1"=="testsign" set TESTSIGN=-TestSign & shift & goto :parse_args
if /i "%~1"=="thumbprint=*" set THUMBPRINT=%~1 & shift & goto :parse_args
if /i "%~1"=="certpath=*" set CERTPATH=%~1 & shift & goto :parse_args
if /i "%~1"=="certpass=*" set CERTPASS=%~1 & shift & goto :parse_args
echo Unknown argument: %~1
exit /b 1

:run_build
powershell -ExecutionPolicy Bypass -File "%~dp0build_driver.ps1" -Configuration %CONFIGURATION% %CLEAN% %SIGN% %TESTSIGN% %THUMBPRINT% %CERTPATH% %CERTPASS%
exit /b %ERRORLEVEL%