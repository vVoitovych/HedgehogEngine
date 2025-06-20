#pragma once
#include "Vector.hpp"

namespace HM
{
    // Row major
    class  Matrix4x4
    {
    public:
        Matrix4x4();
        ~Matrix4x4();

        Matrix4x4(const Matrix4x4&) = default;
        Matrix4x4(Matrix4x4&&) = default;
        Matrix4x4& operator=(const Matrix4x4&) = default;
        Matrix4x4& operator=(Matrix4x4&&) = default;

        Matrix4x4(const Vector4& row1, const Vector4& row2, const Vector4& row3,
            const Vector4& row4);
        Matrix4x4(std::initializer_list<float> list);

        Vector4& operator[](size_t i);
        const Vector4& operator[](size_t i) const;

        static_assert(sizeof(Vector4) == (sizeof(float) * 4), "GetBuffer assumes contiguous Vector4's form contiguous array of floats");
         float* GetBuffer();
         const float* GetBuffer() const;

         Vector4* begin();
         Vector4* end();
         const Vector4* begin() const;
         const Vector4* end() const;

         Matrix4x4 operator-();

         Matrix4x4& operator*=(float other);
         Matrix4x4& operator/=(float other);
         Matrix4x4 operator*(float other) const;
         Matrix4x4 operator/(float other) const;

         Matrix4x4 operator*(const Matrix4x4& other) const;
         Vector4 operator*(const Vector4& other) const;

         bool operator==(const Matrix4x4& other) const;
         bool operator!=(const Matrix4x4& other) const;

         Matrix4x4 Inverse(bool& isSuccessfull) const;
         Matrix4x4 Inverse() const;
         Matrix4x4 Transpose() const;

         static Matrix4x4 GetIdentity();
         static Matrix4x4 GetZero();
         static Matrix4x4 GetTranslation(float x, float y, float z);
         static Matrix4x4 GetTranslation(const Vector4& xyz);
         static Matrix4x4 GetScale(float uniformScale);
         static Matrix4x4 GetScale(float x, float y, float z);
         static Matrix4x4 GetRotationX(float angle);
         static Matrix4x4 GetRotationY(float angle);
         static Matrix4x4 GetRotationZ(float angle);

         static Matrix4x4 LookAt(const Vector4& eye, const Vector4& center, const Vector4& up);
         static Matrix4x4 Perspective(float fov, float aspect, float zNear, float zFar);
         static Matrix4x4 Ortho(float left, float right, float top, float bottom, float zNear, float zFar);

         static Matrix4x4 CalculateView(const Vector4& right, const Vector4& up,
            const Vector4& forward,
            const Vector4& position);
         static Matrix4x4 CalculateProjPerspective(float aspectRatio, float fovY, float nearZ, float farZ);
         static Matrix4x4 CalculateProjOrtographic(float nearZ, float farZ,
            float width, float height);

        // Matrix that transforms UV (-1..1 range) to texcoords(0..1 range) and backwards
         static Matrix4x4 GetUVToTexCoord();
         static Matrix4x4 GetTexCoordToUV();

         static Matrix4x4 CalculateCubeMapView(size_t cubeMapFace,
            const Vector4& position);

    private:
        Vector4 m_data[4];
    };

     Vector4 operator*(const Vector4& first, const Matrix4x4& second);

}