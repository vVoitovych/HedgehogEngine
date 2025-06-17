#pragma once

#include "HedgehogMath/Matrix.hpp"

namespace Renderer
{
	struct ShadowmapPassPushConstants
	{
		HM::Matrix4x4 objToWorld;
	};

}





