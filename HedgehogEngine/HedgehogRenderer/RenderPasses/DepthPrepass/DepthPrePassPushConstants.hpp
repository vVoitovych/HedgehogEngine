#pragma once

#include "HedgehogMath/Matrix.hpp"

namespace Renderer
{
	struct DepthPrePassPushConstants
	{
		HM::Matrix4x4 objToWorld;
	};

}





