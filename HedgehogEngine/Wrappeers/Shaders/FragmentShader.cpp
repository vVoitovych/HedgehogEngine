#include "FragmentShader.hpp"

namespace Renderer
{
	FragmentShader::FragmentShader(const Device& device, const std::string& fileName)
		:Shader(device, fileName)
	{
		mShadereStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		mShadereStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		mShadereStageCreateInfo.module = mShaderModule;
		mShadereStageCreateInfo.pName = "main";
	}

	FragmentShader::~FragmentShader()
	{
	}

}


