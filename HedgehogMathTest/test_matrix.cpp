#include "doctest/doctest/doctest.h"

#include "test_common.hpp"

#include <cmath>

TEST_CASE("Identity matrix")
{
    HM::Matrix4x4 mat = HM::Matrix4x4::GetIdentity();
    
    for (size_t i = 0; i < 4; ++i)
        for (size_t j = 0; j < 4; ++j)
            CHECK(mat[i][j] == (i == j ? 1.0f : 0.0f));
}

TEST_CASE("Matrix4x4 Translation") 
{
    HM::Matrix4x4 T = HM::Matrix4x4::GetTranslation(3.0f, 4.0f, 5.0f);
    HM::Vector4 v(1.0f, 2.0f, 3.0f, 1.0f);
    HM::Vector4 result = T * v;    
    CHECK(result == HM::Vector4(4.0f, 6.0f, 8.0f, 1.0f));
}

TEST_CASE("Matrix4x4 Scale") 
{
    HM::Matrix4x4 S = HM::Matrix4x4::GetScale(2.0f, 3.0f, 4.0f);
    HM::Vector4 v(1.0f, 1.0f, 1.0f, 1.0f);
    HM::Vector4 result = S * v;
    CHECK(result == HM::Vector4(2.0f, 3.0f, 4.0f, 1.0f));
}

TEST_CASE("Matrix4x4 RotationX") 
{
    HM::Matrix4x4 R = HM::Matrix4x4::GetRotationX(3.1415926f / 2.0f); // 90 degrees
    HM::Vector4 v(0.0f, 1.0f, 0.0f, 1.0f);
    HM::Vector4 result = R * v;
    CHECK(HedgehogTest::NearlyEqual(result, HM::Vector4(0.0f, 0.0f, 1.0f, 1.0f)));
}

TEST_CASE("Matrix4x4 RotationY") 
{
    HM::Matrix4x4 R = HM::Matrix4x4::GetRotationY(3.1415926f / 2.0f);
    HM::Vector4 v(0.0f, 0.0f, 1.0f, 1.0f);
    HM::Vector4 result = R * v;
    CHECK(HedgehogTest::NearlyEqual(result, HM::Vector4(1.0f, 0.0f, 0.0f, 1.0f)));
}

TEST_CASE("Matrix4x4 RotationZ") 
{
    HM::Matrix4x4 R = HM::Matrix4x4::GetRotationZ(3.1415926f / 2.0f);
    HM::Vector4 v(1.0f, 0.0f, 0.0f, 1.0f);
    HM::Vector4 result = R * v;
    CHECK(HedgehogTest::NearlyEqual(result, HM::Vector4(00.f, 1.0f, 0.0f, 1.0f)));
}

TEST_CASE("Matrix4x4 multiplication (composition)") 
{
    HM::Matrix4x4 S = HM::Matrix4x4::GetScale(2.0f, 2.0f, 2.0f);
    HM::Matrix4x4 T = HM::Matrix4x4::GetTranslation(1.0f, 2.0f, 3.0f);
    HM::Matrix4x4 M = S * T;

    HM::Vector4 v(1.0f, 1.0f, 1.0f, 1.0f);
    HM::Vector4 result = M * v;
    CHECK(result == HM::Vector4(3.0f, 4.0f, 5.0f, 1.0f));
}

TEST_CASE("Matrix4x4 transpose") 
{
    HM::Matrix4x4 M;
    M[0][1] = 5.0f;
    HM::Matrix4x4 T = M.Transpose();

    CHECK(M[0][1] == T[1][0]);
}


TEST_CASE("Matrix4x4 inverse of rotation") 
{
    HM::Matrix4x4 I = HM::Matrix4x4::GetIdentity();

    HM::Matrix4x4 M = HM::Matrix4x4::GetRotationX(2.0f);
    HM::Matrix4x4 inv = M.Inverse();

    HM::Matrix4x4 res = M * inv;
    CHECK(HedgehogTest::NearlyEqual(res, I));
}

TEST_CASE("Matrix4x4 inverse of translation") 
{
    HM::Matrix4x4 T = HM::Matrix4x4::GetTranslation(3.0f, -4.0f, 5.0f);
    HM::Matrix4x4 Tinv = T.Inverse();
    HM::Matrix4x4 identity = T * Tinv;

    CHECK(HedgehogTest::NearlyEqual(identity, HM::Matrix4x4::GetIdentity()));
}

TEST_CASE("Matrix4x4 inverse of scale") 
{
    HM::Matrix4x4 S = HM::Matrix4x4::GetScale(2.0f, 3.0f, 4.0f);
    HM::Matrix4x4 Sinv = S.Inverse();
    HM::Matrix4x4 identity = S * Sinv;

    CHECK(HedgehogTest::NearlyEqual(identity, HM::Matrix4x4::GetIdentity()));
}

TEST_CASE("Matrix4x4 vector transform (composite)") 
{
    HM::Matrix4x4 M = HM::Matrix4x4::GetScale(2.0f, 2.0f, 2.0f) * HM::Matrix4x4::GetTranslation(1.0f, 2.0f, 3.0f);
    HM::Vector4 v(1.0f, 1.0f, 1.0f, 1.0f);
    HM::Vector4 result = M * v;

    CHECK(result == HM::Vector4(3.0f, 4.0f, 5.0f, 1.0f));
}

TEST_CASE("Matrix4x4 LookAt view matrix") 
{
    HM::Vector4 eye(0.0f, 0.0f, 0.0f, 1.0f);
    HM::Vector4 center(0.0f, 0.0f, -1.0f, 1.0f);
    HM::Vector4 up(0.0f, 1.0f, 0.0f, 0.0f);

    HM::Matrix4x4 view = HM::Matrix4x4::LookAt(eye, center, up);
    HM::Vector4 forward = view * HM::Vector4(0.0f, 0.0f, -1.0f, 0.0f);
    HM::Vector4 upTransformed = view * HM::Vector4(0.0f, 1.0f, 0.0f, 0.0f);

    // Check that forward points along -Z
    CHECK(HedgehogTest::NearlyEqual(forward, HM::Vector4(0.0f, 0.0f, -1.0f, 0.0f)));

    // Check up vector remains unchanged
    CHECK(HedgehogTest::NearlyEqual(upTransformed, HM::Vector4(0.0f, 1.0f, 0.0f, 0.0f)));

    // Eye transformed should now be at origin
    HM::Vector4 transformedEye = view * eye;
    CHECK(HedgehogTest::NearlyEqual(transformedEye, HM::Vector4(0.0f, 0.0f, 0.0f, 1.0f)));
}

TEST_CASE("Matrix4x4 Perspective projection") 
{
    float fov = 90.0f * (3.1415926f / 180.0f); // radians
    float aspect = 1.0f;
    float zNear = 1.0f;
    float zFar = 10.0f;

    HM::Matrix4x4 P = HM::Matrix4x4::Perspective(fov, aspect, zNear, zFar);

    HM::Vector4 nearPoint(0.0f, 0.0f, -zNear, 1.0f);
    HM::Vector4 farPoint(0.0f, 0.0f, -zFar, 1.0f);

    HM::Vector4 projectedNear = P * nearPoint;
    HM::Vector4 projectedFar = P * farPoint;

    projectedNear /= projectedNear.w();
    projectedFar /= projectedFar.w();

    CHECK(HedgehogTest::NearlyEqual(projectedNear.z(), 0.0f, 1e-3f));
    CHECK(HedgehogTest::NearlyEqual(projectedFar.z(), 1.0f, 1e-3f));
}

TEST_CASE("Matrix4x4 Ortho projection") 
{
    float left = -1.0f;
    float right = 1.0f;
    float top = 1.0f;
    float bottom = -1.0f;
    float zNear = 0.0f;
    float zFar = 10.0f;

    HM::Matrix4x4 O = HM::Matrix4x4::Ortho(left, right, top, bottom, zNear, zFar);

    HM::Vector4 center(0.0f, 0.0f, -(zNear + zFar) / 2.0f, 1.0f);
    HM::Vector4 edge(1.0f, 1.0f, -zNear, 1.0f);
    HM::Vector4 outside(2.0f, 0.0f, -zNear, 1.0f);

    HM::Vector4 result = O * center;
    result /= result.w();
    CHECK(HedgehogTest::NearlyEqual(result.x(), 0.0f));
    CHECK(HedgehogTest::NearlyEqual(result.y(), 0.0f));
    CHECK(HedgehogTest::NearlyEqual(result.z(), 0.0f)); 

    result = O * edge;
    result /= result.w();
    CHECK(HedgehogTest::NearlyEqual(result.x(), 1.0f));
    CHECK(HedgehogTest::NearlyEqual(result.y(), 1.0f));

    result = O * outside;
    result /= result.w();
    CHECK(result.x() > 1.0f); 
}


