@echo off
REM CNCS Build Wrapper - calls PowerShell build script
REM Usage: build.bat [Debug|Release] [clean] [tests] [verbose]

set CONFIGURATION=Release
set CLEAN=
set TESTS=
set VERBOSE=

:parse_args
if "%~1"=="" goto :run_build
if /i "%~1"=="debug" set CONFIGURATION=Debug & shift & goto :parse_args
if /i "%~1"=="release" set CONFIGURATION=Release & shift & goto :parse_args
if /i "%~1"=="clean" set CLEAN=-Clean & shift & goto :parse_args
if /i "%~1"=="tests" set TESTS=-RunTests & shift & goto :parse_args
if /i "%~1"=="verbose" set VERBOSE=-Verbose & shift & goto :parse_args
echo Unknown argument: %~1
exit /b 1

:run_build
powershell -ExecutionPolicy Bypass -File "%~dp0build.ps1" -Configuration %CONFIGURATION% %CLEAN% %TESTS% %VERBOSE%
exit /b %ERRORLEVEL%