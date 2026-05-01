#pragma once

#include "HedgehogMath/api/Matrix.hpp"

namespace Renderer
{
    struct ForwardPassPushConstants
    {
        HM::Matrix4x4 objToWorld;
    };

}





