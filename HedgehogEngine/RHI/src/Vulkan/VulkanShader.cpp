#include "VulkanShader.hpp"

#include "VulkanDevice.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "Logger/api/Logger.hpp"

#include <cassert>
#include <cstddef>

namespace RHI
{

VulkanShader::VulkanShader(VulkanDevice& device, const std::string& virtualPath, ShaderStage stage,
                            const FS::FileSystemManager& fileSystem)
    : m_Device(device), m_Stage(stage)
{
    const auto bytes = fileSystem.ReadFile(virtualPath);
    if (!bytes)
    {
        LOGERROR("VulkanShader: cannot read SPIR-V file: ", virtualPath);
        assert(false && "Shader file not found.");
    }

    assert(bytes->size() % 4 == 0 && "SPIR-V file size must be a multiple of 4 bytes.");

    VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = bytes->size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(bytes->data());

    VkResult result = vkCreateShaderModule(m_Device.GetHandle(), &createInfo, nullptr, &m_Module);
    assert(result == VK_SUCCESS && "Failed to create VkShaderModule.");
}

VulkanShader::~VulkanShader()
{
    if (m_Module != VK_NULL_HANDLE)
        vkDestroyShaderModule(m_Device.GetHandle(), m_Module, nullptr);
}

} // namespace RHI
