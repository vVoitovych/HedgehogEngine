#include "VertexShader.hpp"

namespace Hedgehog
{
	namespace Wrappers
	{
		VertexShader::VertexShader(const Device& device, const std::string& fileName)
			:Shader(device, fileName)
		{
			mShadereStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			mShadereStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
			mShadereStageCreateInfo.module = mShaderModule;
			mShadereStageCreateInfo.pName = "main";
		}

		VertexShader::~VertexShader()
		{
		}
	}
}


