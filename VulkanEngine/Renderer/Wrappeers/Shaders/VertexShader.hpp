#pragma once

#include "Shader.hpp"

#include <memory>

namespace Renderer
{
	class Device;

	class VertexShader : public Shader
	{
	public:
		VertexShader(const std::unique_ptr<Device>& device, const std::string& fileName);
		~VertexShader() override;


	};

}



