#include "FragmentShader.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"

#include "Shaders/ShaderCompiler/ShaderCompiler.hpp"

#include <stdexcept>

namespace Wrappers
{
	FragmentShader::FragmentShader(const Device& device, const std::string& fileName)
		: m_ShaderModule(nullptr)
	{
		std::vector<uint32_t> code = ShaderCompiler::ReadAndCompileShader(fileName, ShaderCompiler::ShaderType::Fragment);

		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size() * sizeof(uint32_t);
		createInfo.pCode = code.data();

		if (vkCreateShaderModule(device.GetNativeDevice(), &createInfo, nullptr, &m_ShaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create shadeer module!");
		}

		m_ShadereStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		m_ShadereStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		m_ShadereStageCreateInfo.module = m_ShaderModule;
		m_ShadereStageCreateInfo.pName = "main";
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
		return m_ShadereStageCreateInfo;
	}

}


