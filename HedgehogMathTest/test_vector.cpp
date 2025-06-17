#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest/doctest.h"

#include "HedgehogMath/Vector.hpp"

TEST_CASE("Vector3 addition works") 
{
    HM::Vector3 a(1.0f, 2.0f, 3.0f);
    HM::Vector3 b(4.0f, 5.0f, 6.0f);
    HM::Vector3 result = a + b;
    HM::Vector3 predictedResult(5.0f, 7.0f, 9.0f);
    CHECK(result == predictedResult);
}



