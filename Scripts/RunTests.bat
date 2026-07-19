@echo off
REM ============================================
REM Build the solution, then run every test executable.
REM Usage: RunTests.bat [Debug^|Release]   (default: Debug)
REM Exits nonzero if the build or any test fails.
REM ============================================
setlocal enabledelayedexpansion

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug"

echo === Checking module boundaries ===
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0CheckModuleBoundaries.ps1"
if errorlevel 1 (
    echo [ERROR] Module boundary check failed - build and tests were not run.
    exit /b 1
)

call "%~dp0Build.bat" %CONFIG%
if errorlevel 1 (
    echo [ERROR] Build failed - tests were not run.
    exit /b 1
)

set "BINDIR=%~dp0..\Binaries\windows-x86_64\%CONFIG%"
set FAILED=0

for %%T in (HedgehogMathTest FileSystemTest ECSTest EcsSerializationTest ContentLoaderTest) do (
    echo.
    echo === Running %%T ^(%CONFIG%^) ===
    if exist "%BINDIR%\%%T\%%T.exe" (
        "%BINDIR%\%%T\%%T.exe"
        if errorlevel 1 (
            echo [FAILED] %%T
            set /a FAILED+=1
        )
    ) else (
        echo [FAILED] %%T.exe not found in %BINDIR%\%%T
        set /a FAILED+=1
    )
)

echo.
if !FAILED! gtr 0 (
    echo [ERROR] !FAILED! test executables failed.
    exit /b 1
)
echo All test executables passed.
exit /b 0
