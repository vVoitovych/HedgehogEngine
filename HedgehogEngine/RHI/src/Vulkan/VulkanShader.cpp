#include "VulkanShader.hpp"

#include "VulkanDevice.hpp"

#include "Logger/api/Logger.hpp"

#include <cassert>

namespace RHI
{

VulkanShader::VulkanShader(VulkanDevice& device, std::span<const std::byte> spirv, ShaderStage stage)
    : m_Device(device), m_Stage(stage)
{
    if (spirv.size() % 4 != 0)
    {
        LOGERROR("VulkanShader: SPIR-V size (", spirv.size(), " bytes) is not a multiple of 4");
        return;
    }

    VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = spirv.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(spirv.data());

    VkResult result = vkCreateShaderModule(m_Device.GetHandle(), &createInfo, nullptr, &m_Module);
    assert(result == VK_SUCCESS && "Failed to create VkShaderModule.");
}

VulkanShader::~VulkanShader()
{
    if (m_Module != VK_NULL_HANDLE)
        vkDestroyShaderModule(m_Device.GetHandle(), m_Module, nullptr);
}

} // namespace RHI
