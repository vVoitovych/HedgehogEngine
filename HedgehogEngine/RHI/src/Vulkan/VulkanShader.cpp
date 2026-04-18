#include "VulkanShader.hpp"

#include "VulkanDevice.hpp"

#include "Logger/api/Logger.hpp"

#include <Windows.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>

namespace
{
// Mirrors ContentLoader::GetRootDirectory(): exe path up 4 levels.
std::string ResolveAssetPath(const std::string& relativePath)
{
    char buffer[MAX_PATH];
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    std::filesystem::path root = std::filesystem::path(buffer)
        .parent_path().parent_path().parent_path().parent_path().parent_path();
    return root.string() + relativePath;
}
} // namespace

namespace RHI
{

VulkanShader::VulkanShader(VulkanDevice& device, const std::string& filePath, ShaderStage stage)
    : m_Device(device), m_Stage(stage)
{
    const std::string fullPath = ResolveAssetPath(filePath);
    std::ifstream file(fullPath, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        LOGERROR("VulkanShader: cannot open SPIR-V file: ", fullPath);
        assert(false && "Shader file not found.");
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    assert(fileSize % 4 == 0 && "SPIR-V file size must be a multiple of 4 bytes.");

    std::vector<char> code(fileSize);
    file.seekg(0);
    file.read(code.data(), static_cast<std::streamsize>(fileSize));

    VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkResult result = vkCreateShaderModule(m_Device.GetHandle(), &createInfo, nullptr, &m_Module);
    assert(result == VK_SUCCESS && "Failed to create VkShaderModule.");
}

VulkanShader::~VulkanShader()
{
    if (m_Module != VK_NULL_HANDLE)
        vkDestroyShaderModule(m_Device.GetHandle(), m_Module, nullptr);
}

} // namespace RHI
