#pragma once

namespace Scene
{
	enum class GeometryType
	{
		Opaque,
		Transparent
	};

	class RenderComponent
	{
	public:
		bool mIsVisible;

		GeometryType mGeometryType;

		size_t mColorTexture;
		size_t mColorSampler;

	};
}



