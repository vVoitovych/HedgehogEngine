#include "doctest/doctest/doctest.h"

#include "test_common.hpp"

TEST_CASE("Vector2 construction and access") {
    HM::Vector2 v(1.0f, 2.0f);
    CHECK(v.x() == 1.0f);
    CHECK(v.y() == 2.0f);
}

TEST_CASE("Vector3 construction and access") {
    HM::Vector3 v(1.0f, 2.0f, 3.0f);
    CHECK(v.x() == 1.0f);
    CHECK(v.y() == 2.0f);
    CHECK(v.z() == 3.0f);
}

TEST_CASE("Vector4 construction and access") {
    HM::Vector4 v(1.0f, 2.0f, 3.0f, 4.0f);
    CHECK(v.x() == 1.0f);
    CHECK(v.y() == 2.0f);
    CHECK(v.z() == 3.0f);
    CHECK(v.w() == 4.0f);
}

TEST_CASE("Vector2 addition works")
{
    HM::Vector2 a(1.0f, 2.0f);
    HM::Vector2 b(5.0f, 6.0f);
    HM::Vector2 result = a + b;
    HM::Vector2 predictedResult(6.0f, 8.0f);
    CHECK(result == predictedResult);
}

TEST_CASE("Vector3 addition works")
{
    HM::Vector3 a(1.0f, 2.0f, 3.0f);
    HM::Vector3 b(5.0f, 6.0f, 7.0f);
    HM::Vector3 result = a + b;
    HM::Vector3 predictedResult(6.0f, 8.0f, 10.0f);
    CHECK(result == predictedResult);
}

TEST_CASE("Vector4 addition works")
{
    HM::Vector4 a(1.0f, 2.0f, 3.0f, 4.0f);
    HM::Vector4 b(5.0f, 6.0f, 7.0f, 8.0f);
    HM::Vector4 result = a + b;
    HM::Vector4 predictedResult(6.0f, 8.0f, 10.0f, 12.0f);
    CHECK(result == predictedResult);
}

TEST_CASE("Vector4 addition works") 
{
    HM::Vector4 a(1.0f, 2.0f, 3.0f, 4.0f);
    HM::Vector4 b(5.0f, 6.0f, 7.0f, 8.0f);
    HM::Vector4 result = a + b;
    HM::Vector4 predictedResult(6.0f, 8.0f, 10.0f, 12.0f);
    CHECK(result == predictedResult);
}

TEST_CASE("Vector2 substraction works")
{
    HM::Vector2 a(5.0f, 6.0f);
    HM::Vector2 b(1.0f, 2.0f);
    HM::Vector2 result = a - b;
    HM::Vector2 predictedResult(4.0f, 4.0f);
    CHECK(result == predictedResult);
}

TEST_CASE("Vector3 substraction works")
{
    HM::Vector3 a(5.0f, 6.0f, 7.0f);
    HM::Vector3 b(1.0f, 2.0f, 3.0f);
    HM::Vector3 result = a - b;
    HM::Vector3 predictedResult(4.0f, 4.0f, 4.0f);
    CHECK(result == predictedResult);
}

TEST_CASE("Vector4 substraction works")
{
    HM::Vector4 a(5.0f, 6.0f, 7.0f, 8.0f);
    HM::Vector4 b(1.0f, 2.0f, 3.0f, 4.0f);
    HM::Vector4 result = a - b;
    HM::Vector4 predictedResult(4.0f, 4.0f, 4.0f, 4.0f);
    CHECK(result == predictedResult);
}

TEST_CASE("Vector2 multiplication on scalar works")
{
    HM::Vector2 a(5.0f, 6.0f);
    HM::Vector2 result = a * 2.0f;
    HM::Vector2 predictedResult(10.0f, 12.0f);
    CHECK(result == predictedResult);
}

TEST_CASE("Vector3 multiplication on scalar works")
{
    HM::Vector3 a(5.0f, 6.0f, 7.0f);
    HM::Vector3 result = a * 2.0f;
    HM::Vector3 predictedResult(10.0f, 12.0f, 14.0f);
    CHECK(result == predictedResult);
}

TEST_CASE("Vector4 multiplication on scalar works")
{
    HM::Vector4 a(5.0f, 6.0f, 7.0f, 8.0f);
    HM::Vector4 result = a * 2.0f;
    HM::Vector4 predictedResult(10.0f, 12.0f, 14.0f, 16.0f);
    CHECK(result == predictedResult);
}

TEST_CASE("Vector2 division on scalar works")
{
    HM::Vector2 a(5.0f, 6.0f);
    HM::Vector2 result = a / 2.0f;
    HM::Vector2 predictedResult(2.5f, 3.0f);
    CHECK(result == predictedResult);
}

TEST_CASE("Vector3 division on scalar works")
{
    HM::Vector3 a(5.0f, 6.0f, 7.0f);
    HM::Vector3 result = a / 2.0f;
    HM::Vector3 predictedResult(2.5f, 3.0f, 3.5f);
    CHECK(result == predictedResult);
}

TEST_CASE("Vector4 division on scalar works")
{
    HM::Vector4 a(5.0f, 6.0f, 7.0f, 8.0f);
    HM::Vector4 result = a / 2.0f;
    HM::Vector4 predictedResult(2.5f, 3.0f, 3.5f, 4.0f);
    CHECK(result == predictedResult);
}

TEST_CASE("Vector2 quality works")
{
    HM::Vector2 a(5.0f, 6.0f);
    HM::Vector2 b(5.0f, 6.0f);
    HM::Vector2 c(2.5f, 6.0f);
    CHECK(a == b);
    CHECK(a != c);
}

TEST_CASE("Vector3 quality works")
{
    HM::Vector3 a(5.0f, 6.0f, 7.0f);
    HM::Vector3 b(5.0f, 6.0f, 7.0f);
    HM::Vector3 c(2.5f, 6.0f, 7.0f);
    CHECK(a == b);
    CHECK(a != c);
}

TEST_CASE("Vector4 quality works")
{
    HM::Vector4 a(5.0f, 6.0f, 7.0f, 8.0f);
    HM::Vector4 b(5.0f, 6.0f, 7.0f, 8.0f);
    HM::Vector4 c(2.5f, 6.0f, 7.0f, 8.0f);
    CHECK(a == b);
    CHECK(a != c);
}

TEST_CASE("Vector3 dot product works")
{
    HM::Vector3 a(1.0f, 0.0f, 0.0f);
    HM::Vector3 b(0.0f, 1.0f, 0.0f);
    HM::Vector3 c(1.0f, 2.0f, 3.0f);
    CHECK(HM::Dot(a, b) == 0.0f);
    CHECK(HM::Dot(b, c) == 2.0f);
}

TEST_CASE("Vector3 cross product works")
{
    HM::Vector3 a(1.0f, 0.0f, 0.0f);
    HM::Vector3 b(0.0f, 1.0f, 0.0f);
    HM::Vector3 c(0.0f, 0.0f, 1.0f);
    HM::Vector3 minusC = c * (-1.0f);
    CHECK(HM::Cross(a, b) == c);
    CHECK(HM::Cross(b, a) == minusC);
}

TEST_CASE("Vector3 length and normalization") 
{
    HM::Vector3 a(3.0f, 4.0f, 0.0f);

    CHECK(HedgehogTest::NearlyEqual(a.LengthSlow(), 5.0f));
    CHECK(HedgehogTest::NearlyEqual(a.LengthSqr(), 25.0f));

    HM::Vector3 n = a.Normalize();
    CHECK(HedgehogTest::NearlyEqual(n.LengthSlow(), 1.0f));
    CHECK(HedgehogTest::NearlyEqual(n, HM::Vector3(0.6f, 0.8f, 0.0f)));
}

TEST_CASE("Vector3 distance") {
    HM::Vector3 a(1.0f, 0.0f, 0.0f);
    HM::Vector3 b(4.0f, 0.0f, 0.0f);

    CHECK(HedgehogTest::NearlyEqual((a - b).LengthSlow(), 3.0f));
}


