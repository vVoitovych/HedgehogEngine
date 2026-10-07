@echo off
REM ============================================
REM Package a project as a playable game folder: Build\<ProjectName>\
REM Usage: PackageGame.bat [project folder]   (default: Projects\FeatureTest)
REM
REM Builds Release, then has Cooker.exe --package make the folder: the project's
REM referenced assets, its plugins, Game.exe and the DLLs it loads (no editor, test or
REM ImGui binaries, no .pdb) and LICENSE.txt with THIRD_PARTY_NOTICES.md; then copies
REM the package to a temporary folder and runs Game.exe --frames 120 there as a check.
REM Exits nonzero if any step or the check fails. The package
REM needs only the Vulkan runtime and the VC++ redistributable on the target.
REM ============================================
setlocal enabledelayedexpansion

REM A project named on the command line is relative to the caller's folder.
set "PROJECT="
if not "%~1"=="" set "PROJECT=%~f1"

pushd "%~dp0.."
set "ROOT=%CD%"
set "BIN=%ROOT%\Binaries\windows-x86_64\Release"
if not defined PROJECT set "PROJECT=%ROOT%\Projects\FeatureTest"
if not exist "%PROJECT%\Project.yaml" (
    echo [ERROR] %PROJECT% holds no Project.yaml.
    goto :fail
)

REM The project's name (Project.yaml's name:) names the package folder.
set "NAME="
for /f "usebackq tokens=1,* delims=:" %%a in ("%PROJECT%\Project.yaml") do (
    if "%%a"=="name" set "NAME=%%b"
)
for /f "tokens=* delims= " %%n in ("!NAME!") do set "NAME=%%n"
if not defined NAME (
    echo [ERROR] Project.yaml has no name.
    goto :fail
)
set "OUT=%ROOT%\Build\%NAME%"
echo === Packaging '%NAME%' into %OUT% ===

call "%ROOT%\Scripts\Build.bat" Release
if errorlevel 1 goto :fail

echo === Packaging the game ===
REM Cooker.exe --package (CookerCore's PackageGame): the assets of every scene of the project
REM (--all-scenes, as File > Build Game... packages it), its enabled plugins,
REM Game.exe and the runtime DLLs, the licences, and the check that nothing of the editor, the
REM tests or the sources ships.
"%BIN%\Cooker.exe" --project "%PROJECT%" --out "%OUT%" --binaries "%BIN%" --engine "%ROOT%" --package --all-scenes
if errorlevel 1 (
    echo [ERROR] Packaging failed.
    goto :fail
)

echo === Running the package from another folder ===
set "CHECK=%TEMP%\HedgehogPackageCheck_%RANDOM%%RANDOM%"
robocopy "%OUT%" "%CHECK%" /e /nfl /ndl /njh /njs /nc /ns /np >nul
if errorlevel 8 (
    echo [ERROR] Could not copy the package to %CHECK%.
    goto :fail
)
start "" /wait "%CHECK%\Game.exe" --frames 120
set "RUN_RESULT=%ERRORLEVEL%"
rmdir /s /q "%CHECK%"
if not "%RUN_RESULT%"=="0" (
    echo [ERROR] The packaged Game.exe exited with %RUN_RESULT%.
    goto :fail
)

echo Packaged %OUT%
popd
exit /b 0

:fail
popd
exit /b 1
