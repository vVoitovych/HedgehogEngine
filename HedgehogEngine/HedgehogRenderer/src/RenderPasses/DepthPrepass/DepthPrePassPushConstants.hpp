#pragma once

#include "HedgehogMath/api/Matrix.hpp"

namespace Renderer
{
    struct DepthPrePassPushConstants
    {
        HM::Matrix4x4 objToWorld;
    };

}





