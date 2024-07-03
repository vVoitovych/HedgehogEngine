#include "Shader.hpp"

#include "Wrappeers/Device/Device.hpp"

#include "ContentLoader/CommonFunctions.hpp"

#include "Logger/Logger.hpp"

#include <fstream>

namespace Renderer
{
	std::vector<char> ReadFile(const std::string& filename)
	{
		std::string fullName = ContentLoader::GetShadersDirectory() + filename;
		LOGINFO(fullName);

		std::ifstream file(fullName, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			throw std::runtime_error("failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}

	VkShaderModule CreateShaderModule(const VkDevice device, std::vector<char>& code)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create shadeer module!");
		}
		return shaderModule;
	}

	// shader functionality
	Shader::Shader(const Device& device, const std::string& fileName)
		: mShaderModule(nullptr)
		, mShadereStageCreateInfo{}

	{
		auto shaderCode = ReadFile(fileName);
		mShaderModule = CreateShaderModule(device.GetNativeDevice(), shaderCode);
	}

	Shader::~Shader()
	{
	}

	void Shader::Cleanup(const Device& device)
	{
		vkDestroyShaderModule(device.GetNativeDevice(), mShaderModule, nullptr);
	}

	VkPipelineShaderStageCreateInfo Shader::GetCreateInfo()
	{
		return mShadereStageCreateInfo;
	}

}



