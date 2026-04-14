#include "FragmentShader.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"

#include "ContentLoader/CommonFunctions.hpp"

#include <stdexcept>

namespace Wrappers
{
    FragmentShader::FragmentShader(const Device& device, const std::string& fileName)
        : m_ShaderModule(nullptr)
    {
        std::string code = ContentLoader::ReadFile(fileName);

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<uint32_t*>(code.data());

        if (vkCreateShaderModule(device.GetNativeDevice(), &createInfo, nullptr, &m_ShaderModule) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create shader module!");
        }

        m_ShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        m_ShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        m_ShaderStageCreateInfo.module = m_ShaderModule;
        m_ShaderStageCreateInfo.pName = "main";
    }

    FragmentShader::~FragmentShader()
    {
    }

    void FragmentShader::Cleanup(const Device& device)
    {
        vkDestroyShaderModule(device.GetNativeDevice(), m_ShaderModule, nullptr);
    }

    VkPipelineShaderStageCreateInfo FragmentShader::GetCreateInfo()
    {
        return m_ShaderStageCreateInfo;
    }

}
