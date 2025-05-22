#pragma once
#include "Vector.hpp"

namespace HM
{
    struct Plane 
    {
        Vector3 normal;  
        float distance;    

        float GetSignedDistanceToPlane(const Vector3& point) const;
    };

}

