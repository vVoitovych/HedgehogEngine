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
		HM::Vector3 pos;
		HM::Vector3 color;
		HM::Vector2 texCoord;
		HM::Vector3 normal;

	public:
		static VkVertexInputBindingDescription GetBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 4>  GetAttributeDescription();

	};
}

namespace std
{
	template<>
	struct hash<HM::Vector2>
	{
		size_t operator()(HM::Vector2 const& vector) const
		{
			return (
				(hash<float>()(vector.x()) ^
				(hash<float>()(vector.y()) << 1))
				);
		}
	};

	template<>
	struct hash<HM::Vector3>
	{
		size_t operator()(HM::Vector3 const& vector) const
		{
			return (
				(hash<float>()(vector.x()) ^
				(hash<float>()(vector.y()) << 1) ^
				(hash<float>()(vector.z())))
				);
		}
	};

	template<>
	struct hash<Context::VertexDescription>
	{
		size_t operator()(Context::VertexDescription const& vertex) const
		{
			return (
				(hash<HM::Vector3>()(vertex.pos) ^ (hash<HM::Vector3>()(vertex.color) << 1)) >> 1) ^
				(hash<HM::Vector2>()(vertex.texCoord) << 1) ^
				(hash<HM::Vector3>()(vertex.normal));
		}
	};
}


