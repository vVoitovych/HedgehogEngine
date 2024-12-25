#pragma once

#include <vulkan/vulkan.h>
#include <string>

namespace Wrappers
{
	class Device;

	class VertexShader 
	{
	public:
		VertexShader(const Device& device, const std::string& fileName);
		~VertexShader();

		VertexShader(const VertexShader&) = default;
		VertexShader(VertexShader&&) = default;
		VertexShader& operator=(const VertexShader&) = default;
		VertexShader& operator=(VertexShader&&) = default;

		void Cleanup(const Device& device);
		VkPipelineShaderStageCreateInfo GetCreateInfo();

	private:
		VkShaderModule mShaderModule;
		VkPipelineShaderStageCreateInfo mShadereStageCreateInfo = {};
	};

}



