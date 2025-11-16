@echo off
REM ============================================
REM Update all git submodules (recursive + remote)
REM ============================================

echo Initializing submodules...
git submodule init
if errorlevel 1 (
    echo Failed to initialize submodules.
    exit /b 1
)

echo Updating submodules recursively and pulling remote changes...
git submodule update --recursive --remote
if errorlevel 1 (
    echo Failed to update submodules.
    exit /b 1
)

echo Submodules updated successfully.

REM ============================================
REM Generate Visual Studio 2022 solution using Premake
REM ============================================

REM Define path to Premake executable
set PREMAKE_EXE=..\Vendor\Binaries\Premake\Windows\premake5.exe

REM Run Premake with the specified build script
"%PREMAKE_EXE%" --file=..\Build.lua vs2022

REM Check for errors
if errorlevel 1 (
    echo Premake generation failed.
    exit /b 1
)

echo Premake project files generated successfully.