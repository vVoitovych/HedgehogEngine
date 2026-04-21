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
