#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <functional>

#include "HedgehogMath/Vector.hpp"

namespace Context
{
	class VertexDescription
	{
	public:
		bool operator==(const VertexDescription& other) const;

	public:
		HM::Vector4 pos;
		HM::Vector2 texCoord;
		HM::Vector4 normal;

	public:
		static VkVertexInputBindingDescription GetBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 3>  GetAttributeDescription();

	};
}
