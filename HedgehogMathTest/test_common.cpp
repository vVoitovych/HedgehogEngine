#include "test_common.hpp"

#include <cmath>
namespace HedgehogTest
{
    bool NearlyEqual(float a, float b, float epsilon)
    {
        return std::abs(a - b) < epsilon;
    }

    bool NearlyEqual(const HM::Vector3& a, const HM::Vector3& b, float epsilon)
    {
        return  NearlyEqual(a.x(), b.x(), epsilon) &&
                NearlyEqual(a.y(), b.y(), epsilon) &&
                NearlyEqual(a.z(), b.z(), epsilon);
    }

    bool NearlyEqual(const HM::Vector4& a, const HM::Vector4& b, float epsilon)
    {
        return  NearlyEqual(a.x(), b.x(), epsilon) &&
                NearlyEqual(a.y(), b.y(), epsilon) &&
                NearlyEqual(a.z(), b.z(), epsilon) &&
                NearlyEqual(a.w(), b.w(), epsilon);
    }

    bool NearlyEqual(const HM::Matrix4x4& a, const HM::Matrix4x4& b, float epsilon)
    {
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                if (!NearlyEqual(a[row][col], b[row][col], epsilon))
                    return false;
        return true;
    }
}






