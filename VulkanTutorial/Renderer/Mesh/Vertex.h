#pragma once
#include "../Common/pch.h"

namespace Renderer
{
	struct Vertex
	{
		glm::vec2 pos;
		glm::vec3 color;


		static VkVertexInputBindingDescription GetBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 2>  GetAttributeDescription();

		static std::vector<Vertex> GetSimpleTriangle();

	};


}


