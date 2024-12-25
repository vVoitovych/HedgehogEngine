#pragma once

#include <vector>
#include <string>

namespace ShaderCompiler
{
	enum class ShaderType
	{
		Vertex,
		Fragment, 
		Compute
	};

	std::vector<uint32_t> ReadAndCompileShader(const std::string& file, ShaderType type);

}



