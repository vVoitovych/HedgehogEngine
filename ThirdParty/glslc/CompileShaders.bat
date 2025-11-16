@echo off
setlocal enabledelayedexpansion

set SHADERC=%~dp0glslc.exe

if "%~1"=="" (
    echo [ERROR] Missing shader root folder argument.
    echo Usage: %~nx0 path\to\shaders
    exit /b 1
)

set "SHADER_SRC=%~1"

echo === Compiling shaders in folder %SHADER_SRC% ===

for /R "%SHADER_SRC%" %%f in (*.vert *.frag *.comp) do (
    set "SRC=%%f"
    set "NAME=%%~nxf"
    echo Compiling %%f
    "%SHADERC%" -I "%SHADER_SRC%" "%%f" -o "%%f.spv"
    if errorlevel 1 (
        echo [ERROR] Shader compile failed: %%f
        exit /b 1
    )
)

echo === All shaders compiled successfully ===
exit /b 0
