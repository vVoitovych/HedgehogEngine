#include "test_common.hpp"

#include <cmath>
namespace HedgehogTest
{
    bool NearlyEqual(float a, float b)
    {
        constexpr float epsilon = 1e-5f;
        return std::abs(a - b) < epsilon;
    }

    bool NearlyEqual(const HM::Vector3& a, const HM::Vector3& b)
    {
        return  NearlyEqual(a.x(), b.x()) &&
                NearlyEqual(a.y(), b.y()) &&
                NearlyEqual(a.z(), b.z());
    }

    bool NearlyEqual(const HM::Vector4& a, const HM::Vector4& b)
    {
        return  NearlyEqual(a.x(), b.x()) &&
                NearlyEqual(a.y(), b.y()) &&
                NearlyEqual(a.z(), b.z()) &&
                NearlyEqual(a.w(), b.w());
    }

    bool NearlyEqual(const HM::Matrix4x4& a, const HM::Matrix4x4& b)
    {
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                if (!NearlyEqual(a[row][col], b[row][col]))
                    return false;
        return true;
    }
}






