#pragma once
#include "Vector.hpp"
#include "HedgehogMathAPI.hpp"

namespace HM
{
    // Row major
    class Matrix4x4
    {
    public:
        HEDGEHOG_MATH_API Matrix4x4();
        HEDGEHOG_MATH_API ~Matrix4x4();

        Matrix4x4(const Matrix4x4&) = default;
        Matrix4x4(Matrix4x4&&) = default;
        Matrix4x4& operator=(const Matrix4x4&) = default;
        Matrix4x4& operator=(Matrix4x4&&) = default;

        HEDGEHOG_MATH_API Matrix4x4(const Vector4& row1, const Vector4& row2, const Vector4& row3,
            const Vector4& row4);
        HEDGEHOG_MATH_API Matrix4x4(std::initializer_list<float> list);

        HEDGEHOG_MATH_API Vector4& operator[](size_t i);
        HEDGEHOG_MATH_API const Vector4& operator[](size_t i) const;

        static_assert(sizeof(Vector4) == (sizeof(float) * 4), "GetBuffer assumes contiguous Vector4's form contiguous array of floats");
        HEDGEHOG_MATH_API float* GetBuffer();
        HEDGEHOG_MATH_API const float* GetBuffer() const;

        HEDGEHOG_MATH_API Vector4* begin();
        HEDGEHOG_MATH_API Vector4* end();
        HEDGEHOG_MATH_API const Vector4* begin() const;
        HEDGEHOG_MATH_API const Vector4* end() const;

        HEDGEHOG_MATH_API Matrix4x4 operator-();

        HEDGEHOG_MATH_API Matrix4x4& operator*=(float other);
        HEDGEHOG_MATH_API Matrix4x4& operator/=(float other);
        HEDGEHOG_MATH_API Matrix4x4 operator*(float other) const;
        HEDGEHOG_MATH_API Matrix4x4 operator/(float other) const;

        HEDGEHOG_MATH_API Matrix4x4 operator*(const Matrix4x4& other) const;
        HEDGEHOG_MATH_API Vector4 operator*(const Vector4& other) const;

        HEDGEHOG_MATH_API bool operator==(const Matrix4x4& other) const;
        HEDGEHOG_MATH_API bool operator!=(const Matrix4x4& other) const;

        HEDGEHOG_MATH_API Matrix4x4 Inverse(bool& isSuccessfull) const;
        HEDGEHOG_MATH_API Matrix4x4 Inverse() const;
        HEDGEHOG_MATH_API Matrix4x4 Transpose() const;

        HEDGEHOG_MATH_API static Matrix4x4 GetIdentity();
        HEDGEHOG_MATH_API static Matrix4x4 GetZero();
        HEDGEHOG_MATH_API static Matrix4x4 GetTranslation(float x, float y, float z);
        HEDGEHOG_MATH_API static Matrix4x4 GetTranslation(const Vector4& xyz);
        HEDGEHOG_MATH_API static Matrix4x4 GetScale(float uniformScale);
        HEDGEHOG_MATH_API static Matrix4x4 GetScale(float x, float y, float z);
        HEDGEHOG_MATH_API static Matrix4x4 GetRotationX(float angle);
        HEDGEHOG_MATH_API static Matrix4x4 GetRotationY(float angle);
        HEDGEHOG_MATH_API static Matrix4x4 GetRotationZ(float angle);

        HEDGEHOG_MATH_API static Matrix4x4 LookAt(const Vector4& eye, const Vector4& center, const Vector4& up);
        HEDGEHOG_MATH_API static Matrix4x4 Perspective(float fov, float aspect, float zNear, float zFar);
        HEDGEHOG_MATH_API static Matrix4x4 Ortho(float left, float right, float top, float bottom, float zNear, float zFar);

        HEDGEHOG_MATH_API static Matrix4x4 CalculateView(const Vector4& right, const Vector4& up,
            const Vector4& forward,
            const Vector4& position);
        HEDGEHOG_MATH_API static Matrix4x4 CalculateProjPerspective(float aspectRatio, float fovY, float nearZ, float farZ);
        HEDGEHOG_MATH_API static Matrix4x4 CalculateProjOrtographic(float nearZ, float farZ,
            float width, float height);

        // Matrix that transforms UV (-1..1 range) to texcoords(0..1 range) and backwards
        HEDGEHOG_MATH_API static Matrix4x4 GetUVToTexCoord();
        HEDGEHOG_MATH_API static Matrix4x4 GetTexCoordToUV();

        HEDGEHOG_MATH_API static Matrix4x4 CalculateCubeMapView(size_t cubeMapFace,
            const Vector4& position);

    private:
        Vector4 m_data[4];
    };

    HEDGEHOG_MATH_API Vector4 operator*(const Vector4& first, const Matrix4x4& second);

}
