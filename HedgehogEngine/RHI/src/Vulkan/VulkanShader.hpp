#pragma once

#include "RHI/api/IRHIShader.hpp"

#include <Volk/volk.h>

#include <string>

namespace RHI
{

class VulkanDevice;

class VulkanShader final : public IRHIShader
{
public:
    VulkanShader(VulkanDevice& device, const std::string& filePath, ShaderStage stage);
    ~VulkanShader() override;

    VulkanShader(const VulkanShader&)            = delete;
    VulkanShader& operator=(const VulkanShader&) = delete;
    VulkanShader(VulkanShader&&)                 = delete;
    VulkanShader& operator=(VulkanShader&&)      = delete;

    ShaderStage GetStage() const override { return m_Stage; }

    VkShaderModule GetHandle() const { return m_Module; }

private:
    VulkanDevice& m_Device;
    VkShaderModule m_Module = VK_NULL_HANDLE;
    ShaderStage    m_Stage;
};

} // namespace RHI
