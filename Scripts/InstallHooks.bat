@echo off
REM ============================================
REM Install the repository git hooks
REM Points core.hooksPath at .githooks so the ticket-key hooks are active.
REM ============================================
setlocal

pushd "%~dp0.."

git rev-parse --git-dir >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Not inside a git repository.
    popd
    exit /b 1
)

git config core.hooksPath .githooks
if errorlevel 1 (
    echo [ERROR] Failed to set core.hooksPath.
    popd
    exit /b 1
)

echo Git hooks installed: core.hooksPath = .githooks
echo   prepare-commit-msg  prefills HE-nnn from the branch name
echo   commit-msg          rejects subjects with no ticket key
popd
