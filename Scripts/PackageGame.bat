@echo off
REM ============================================
REM Package a project as a playable game folder: Build\<ProjectName>\
REM Usage: PackageGame.bat [project folder]   (default: Projects\FeatureTest)
REM
REM Builds Release, cooks the project's referenced assets with the Cooker, copies
REM Game.exe and the DLLs it loads (no editor, test or ImGui binaries, no .pdb)
REM and LICENSE.txt with THIRD_PARTY_NOTICES.md,
REM then copies the package to a temporary folder and runs Game.exe --frames 120
REM there as a check. Exits nonzero if any step or the check fails. The package
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

echo === Cooking assets ===
"%BIN%\Cooker.exe" --project "%PROJECT%" --out "%OUT%" --binaries "%BIN%" --engine "%ROOT%"
if errorlevel 1 (
    echo [ERROR] The cook failed.
    goto :fail
)

echo === Copying the game executable ===
for %%F in (Game.exe glfw.dll Logger.dll FileSystem.dll HedgehogMath.dll HedgehogCommon.dll ECS.dll EcsSerialization.dll ContentLoader.dll HedgehogSettings.dll HedgehogWindow.dll HedgehogAudio.dll HedgehogEngine.dll) do (
    copy /y "%BIN%\%%F" "%OUT%\" >nul
    if errorlevel 1 (
        echo [ERROR] Could not copy %%F.
        goto :fail
    )
)

REM The engine's licence and the third-party notices must travel with every game.
echo === Copying the licence notices ===
for %%F in (LICENSE.txt THIRD_PARTY_NOTICES.md) do (
    copy /y "%ROOT%\%%F" "%OUT%\" >nul
    if errorlevel 1 (
        echo [ERROR] Could not copy %%F.
        goto :fail
    )
)

REM Nothing of the editor, the tests or the sources may ship. One dir call: a plain
REM for loop would expand the wildcards against the current folder instead.
set "FORBIDDEN=0"
for /f "delims=" %%f in ('dir /s /b /a-d "%OUT%\Editor.exe" "%OUT%\Cooker.exe" "%OUT%\*Test.exe" "%OUT%\*.pdb" "%OUT%\*.ilk" "%OUT%\*.lib" "%OUT%\DialogueWindows.dll" "%OUT%\*imgui*" "%OUT%\*.vert" "%OUT%\*.frag" "%OUT%\*.comp" "%OUT%\*.glsl" 2^>nul') do (
    echo [ERROR] The package must not contain %%f
    set "FORBIDDEN=1"
)
if "%FORBIDDEN%"=="1" goto :fail

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
