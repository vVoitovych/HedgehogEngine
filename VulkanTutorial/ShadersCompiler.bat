
set SHADER_OUTPUT_DIR=CompiledShaders

mkdir "%SHADER_OUTPUT_DIR%" 2>nul

C:\VulkanSDK\1.3.243.0\Bin\glslc.exe Shaders/SimpleShader.vert -o CompiledShaders/SimpleShader.vert.spv
C:\VulkanSDK\1.3.243.0\Bin\glslc.exe Shaders/SimpleShader.frag -o CompiledShaders/SimpleShader.frag.spv
pause
