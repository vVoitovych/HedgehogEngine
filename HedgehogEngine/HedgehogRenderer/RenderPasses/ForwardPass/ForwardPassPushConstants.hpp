#pragma once

#include "HedgehogMath/Matrix.hpp"

namespace Renderer
{
	struct ForwardPassPushConstants
	{
		HM::Matrix4x4 objToWorld;
	};

}





