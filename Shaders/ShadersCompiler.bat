
set SHADER_SOURCE_DIR=Shaders

set DEBUG_SHADER_OUTPUT_DIR=Shaders\CompiledShaders\Debug
set RELEASE_SHADER_OUTPUT_DIR=Shaders\CompiledShaders\Release

echo "Debug shader compilation"

mkdir "%DEBUG_SHADER_OUTPUT_DIR%\%SHADER_SOURCE_DIR%" 2>nul

for %%f in ("%SHADER_SOURCE_DIR%\*.vert") do (
    glslc.exe "%%f" -o "%DEBUG_SHADER_OUTPUT_DIR%\%%f.spv"
)

for %%f in ("%SHADER_SOURCE_DIR%\*.frag") do (
    glslc.exe "%%f" -o "%DEBUG_SHADER_OUTPUT_DIR%\%%f.spv"
)

echo "Release shader compilation "

mkdir "%RELEASE_SHADER_OUTPUT_DIR%\%SHADER_SOURCE_DIR%" 2>nul

for %%f in ("%SHADER_SOURCE_DIR%\*.vert") do (
    glslc.exe "%%f" -o "%RELEASE_SHADER_OUTPUT_DIR%\%%f.spv"
)

for %%f in ("%SHADER_SOURCE_DIR%\*.frag") do (
    glslc.exe "%%f" -o "%RELEASE_SHADER_OUTPUT_DIR%\%%f.spv"
)

pause
