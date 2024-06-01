#pragma once

#include "Shader.hpp"

#include <memory>

namespace Hedgehog
{
	namespace Wrappers
	{
		class Device;

		class FragmentShader : public Shader
		{
		public:
			FragmentShader(const Device& device, const std::string& fileName);
			~FragmentShader() override;


		};
	}
}


