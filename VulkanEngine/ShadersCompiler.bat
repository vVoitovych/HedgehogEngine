
set VULKAN_SDK_PATH=C:\VulkanSDK\1.3.243.0\Bin
set SHADER_SOURCE_DIR=Shaders

set DEBUG_SHADER_OUTPUT_DIR=..\x64\Debug\CompiledShaders
set RELEASE_SHADER_OUTPUT_DIR=..\x64\Release\CompiledShaders

echo "Debug shader compilation"

mkdir "%DEBUG_SHADER_OUTPUT_DIR%\%SHADER_SOURCE_DIR%" 2>nul

for %%f in ("%SHADER_SOURCE_DIR%\*.vert") do (
    %VULKAN_SDK_PATH%\glslc.exe "%%f" -o "%DEBUG_SHADER_OUTPUT_DIR%\%%f.spv"
)

for %%f in ("%SHADER_SOURCE_DIR%\*.frag") do (
    %VULKAN_SDK_PATH%\glslc.exe "%%f" -o "%DEBUG_SHADER_OUTPUT_DIR%\%%f.spv"
)

echo "Release shader compilation "

mkdir "%RELEASE_SHADER_OUTPUT_DIR%\%SHADER_SOURCE_DIR%" 2>nul

for %%f in ("%SHADER_SOURCE_DIR%\*.vert") do (
    %VULKAN_SDK_PATH%\glslc.exe "%%f" -o "%RELEASE_SHADER_OUTPUT_DIR%\%%f.spv"
)

for %%f in ("%SHADER_SOURCE_DIR%\*.frag") do (
    %VULKAN_SDK_PATH%\glslc.exe "%%f" -o "%RELEASE_SHADER_OUTPUT_DIR%\%%f.spv"
)

pause
