#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <memory>

namespace Renderer
{
	class Device;

	class Shader
	{
	public:
		Shader(const std::unique_ptr<Device>& device, const std::string& fileName);
		virtual ~Shader();

		Shader(const Shader&) = default;
		Shader(Shader&&) = default;
		Shader& operator=(const Shader&) = default;
		Shader& operator=(Shader&&) = default;

		void Cleanup(const std::unique_ptr<Device>& device);
		VkPipelineShaderStageCreateInfo GetCreateInfo();

	protected:
		VkShaderModule mShaderModule;
		VkPipelineShaderStageCreateInfo mShadereStageCreateInfo;
	};
}

