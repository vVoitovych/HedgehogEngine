#pragma once

#include <vulkan/vulkan.h>
#include <string>

namespace Wrappers
{
	class Device;

	class FragmentShader
	{
	public:
		FragmentShader(const Device& device, const std::string& fileName);
		~FragmentShader();

		FragmentShader(const FragmentShader&) = default;
		FragmentShader(FragmentShader&&) = default;
		FragmentShader& operator=(const FragmentShader&) = default;
		FragmentShader& operator=(FragmentShader&&) = default;

		void Cleanup(const Device& device);
		VkPipelineShaderStageCreateInfo GetCreateInfo();

	private:
		VkShaderModule m_ShaderModule;
		VkPipelineShaderStageCreateInfo m_ShadereStageCreateInfo = {};
	};

}


