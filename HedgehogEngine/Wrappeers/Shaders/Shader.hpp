#pragma once

#include <vulkan/vulkan.h>
#include <string>

namespace Wrappers
{
	class Device;

	class Shader
	{
	public:
		Shader(const Device& device, const std::string& fileName);
		virtual ~Shader();

		Shader(const Shader&) = default;
		Shader(Shader&&) = default;
		Shader& operator=(const Shader&) = default;
		Shader& operator=(Shader&&) = default;

		void Cleanup(const Device& device);
		VkPipelineShaderStageCreateInfo GetCreateInfo();

	protected:
		VkShaderModule mShaderModule;
		VkPipelineShaderStageCreateInfo mShadereStageCreateInfo;
	};
}

