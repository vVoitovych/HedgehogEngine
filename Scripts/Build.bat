@echo off
REM ============================================
REM Build HedgehogEngine.sln (x64)
REM Usage: Build.bat [Debug^|Release]   (default: Debug)
REM ============================================
setlocal

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug"
if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" (
    echo Usage: %~nx0 [Debug^|Release]
    exit /b 1
)

REM Locate MSBuild via vswhere so the script works on any VS 2022 install.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] vswhere.exe not found. Is Visual Studio 2022 installed?
    exit /b 1
)

set "MSBUILD="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
    echo [ERROR] MSBuild.exe not found via vswhere.
    exit /b 1
)

pushd "%~dp0.."

if not exist HedgehogEngine.sln (
    echo [ERROR] HedgehogEngine.sln not found. Run Scripts\SetupWindows.bat first.
    popd
    exit /b 1
)

echo === Building HedgehogEngine.sln ^(%CONFIG% x64^) ===
"%MSBUILD%" HedgehogEngine.sln /p:Configuration=%CONFIG% /p:Platform=x64 /m /v:m
set "BUILD_RESULT=%ERRORLEVEL%"

popd

if not "%BUILD_RESULT%"=="0" (
    echo [ERROR] Build failed.
    exit /b %BUILD_RESULT%
)
echo Build succeeded.
exit /b 0
