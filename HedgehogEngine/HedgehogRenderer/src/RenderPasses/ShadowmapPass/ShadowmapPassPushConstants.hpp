#pragma once

#include "HedgehogMath/api/Matrix.hpp"

namespace Renderer
{
    struct ShadowmapPassPushConstants
    {
        HM::Matrix4x4 objToWorld;
    };

}





