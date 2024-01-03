#pragma once

#include "Shader.hpp"

#include <memory>

namespace Renderer
{
	class Device;

	class FragmentShader : public Shader
	{
	public:
		FragmentShader(const std::unique_ptr<Device>& device, const std::string& fileName);
		~FragmentShader() override;


	};

}


