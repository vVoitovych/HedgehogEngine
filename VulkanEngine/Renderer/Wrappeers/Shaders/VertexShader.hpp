#pragma once

#include "Shader.hpp"

namespace Renderer
{
	class Device;

	class VertexShader : public Shader
	{
	public:
		VertexShader(const Device& device, const std::string& fileName);
		~VertexShader() override;


	};

}



