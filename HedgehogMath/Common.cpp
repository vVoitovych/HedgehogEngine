#include "Common.hpp"

#include <cmath>

namespace
{
    constexpr double PI = 3.14159265358979323846;
}

namespace HM
{
    float ToRadians(float angle)
    {
        return static_cast<float>(PI) / 180.0f * angle;
    }

    double ToRadians(double angle)
    {
        return PI / 180.0 * angle;
    }

    float ToDegree(float angle)
    {
        return 180.0f / static_cast<float>(PI) * angle;
    }

    double ToDegree(double angle)
    {
        return 180.0 / PI * angle;
    }
}
