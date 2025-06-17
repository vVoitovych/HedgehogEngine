#include "doctest/doctest/doctest.h"

#include "HedgehogMath/Matrix.hpp"

TEST_CASE("Identity matrix")
{
    HM::Matrix4x4 mat = HM::Matrix4x4::GetIdentity();
    bool result = true;
    for (size_t i = 0; i < 4; ++i)
        for (size_t j = 0; j < 4; ++j)
        {
            if (i == j && mat[i][j] != 1.0f)
                result = false;
            if (i != j && mat[i][j] != 0.0f)
                result = false;
        }
    CHECK(result);
}


