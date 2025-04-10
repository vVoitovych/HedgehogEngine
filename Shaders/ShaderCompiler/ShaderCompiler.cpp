#include "ShaderCompiler.hpp"
#include "ContentLoader/CommonFunctions.hpp"
#include "Logger/Logger.hpp"

#include "ThirdParty/SPIRV-Reflect/SPIRV-Reflect/spirv_reflect.h"

#include <shaderc/shaderc.hpp>

#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <Windows.h>
#include <stdexcept>
#include <filesystem>

namespace ShaderCompiler
{
    std::string ReadFile(const std::string& filepath)
    {
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file: " + filepath);
        }

        size_t fileSize = (size_t)file.tellg();
        std::string buffer(fileSize, '\0');
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        return buffer;
    }

    std::string PreprocessShader(const std::string& source, const std::string& directory, std::unordered_set<std::string>& includedFiles)
    {
        std::istringstream stream(source);
        std::ostringstream processed;
        std::string line;

        while (std::getline(stream, line)) {
            if (line.find("#include") == 0) {
                size_t start = line.find('"');
                size_t end = line.find('"', start + 1);
                if (start == std::string::npos || end == std::string::npos) {
                    throw std::runtime_error("Malformed #include directive: " + line);
                }

                std::string includePath = directory + line.substr(start + 1, end - start - 1);
                if (includedFiles.find(includePath) != includedFiles.end())
                {
                    continue;
                }

                includedFiles.insert(includePath);
                std::string includedSource = ReadFile(includePath);
                processed << PreprocessShader(includedSource, directory, includedFiles);
            }
            else {
                processed << line << "\n";
            }
        }

        return processed.str();
    }

    std::vector<uint32_t> CompileShader(const std::string& source, shaderc_shader_kind kind, const std::string& name)
    {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);

        auto result = compiler.CompileGlslToSpv(source, kind, name.c_str(), options);
        if (result.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            throw std::runtime_error(result.GetErrorMessage());
        }

        return { result.cbegin(), result.cend() };
    }

    shaderc_shader_kind ToNativeKind(ShaderType type)
    {
        switch (type)
        {
        case ShaderCompiler::ShaderType::Vertex:
            return shaderc_vertex_shader;
            break;
        case ShaderCompiler::ShaderType::Fragment:
            return shaderc_fragment_shader;
            break;
        case ShaderCompiler::ShaderType::Compute:
            return shaderc_compute_shader;
            break;
        default:
            throw std::runtime_error("Unsuported shader type");
        }
    }

    void PrintUniforms(const std::vector<uint32_t>& spirvData) 
    {
        SpvReflectShaderModule module;
        SpvReflectResult result = spvReflectCreateShaderModule(spirvData.size() * sizeof(uint32_t), spirvData.data(), &module);

        if (result != SPV_REFLECT_RESULT_SUCCESS) 
        {
            LOGERROR("Failed to create SPIRV reflection module");
            return;
        }

        uint32_t count = 0;
        spvReflectEnumerateDescriptorBindings(&module, &count, nullptr);
        std::vector<SpvReflectDescriptorBinding*> bindings(count);
        spvReflectEnumerateDescriptorBindings(&module, &count, bindings.data());

        for (const auto* binding : bindings) 
        {
            LOGVERBOSE("Name: ", binding->name, ", Binding: ", binding->binding, ", Set: ", binding->set, ", DescriptorType: ", binding->descriptor_type);
        }

        spvReflectDestroyShaderModule(&module);
    }

	std::vector<uint32_t> ReadAndCompileShader(const std::string& file, ShaderType type)
	{
        std::string shaderDirectory = ContentLoader::GetShadersDirectory();
        std::unordered_set<std::string> includedFiles;

        std::string vertexSource = ReadFile(ContentLoader::GetShadersDirectory() + file);
        std::string preprocessedVertexSource = PreprocessShader(vertexSource, shaderDirectory, includedFiles);

        std::vector<uint32_t> result = CompileShader(preprocessedVertexSource, ToNativeKind(type), file);
#ifdef DEBUG
        PrintUniforms(result);
#endif
		return result;
	}
}




