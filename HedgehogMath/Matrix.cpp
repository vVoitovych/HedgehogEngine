#include "Matrix.hpp"

#include <algorithm>
#include <cassert>

namespace HM
{
    Matrix4x4::Matrix4x4(std::initializer_list<float> list)
    {
        auto iter = list.begin();
        for (auto& vector : m_data)
        {
            vector = Vector4(iter, iter + 4);
            iter += 4;
        }
    }

    Matrix4x4::Matrix4x4(const Vector4& row1, const Vector4& row2, const Vector4& row3,
        const Vector4& row4)
        : m_data{ row1, row2, row3, row4 }
    {
    }

    Matrix4x4::Matrix4x4()
    {
    }

    Matrix4x4::~Matrix4x4()
    {
    }

    const Vector4* Matrix4x4::end() const
    {
        return m_data + 4;
    }

    Vector4* Matrix4x4::end()
    {
        return m_data + 4;
    }

    const Vector4& Matrix4x4::operator[](size_t i) const
    {
        return m_data[i];
    }

    Vector4& Matrix4x4::operator[](size_t i)
    {
        return m_data[i];
    }

    const float* Matrix4x4::GetBuffer() const
    {
        return m_data[0].GetBuffer();
    }

    float* Matrix4x4::GetBuffer()
    {
        return m_data[0].GetBuffer();
    }

    const Vector4* Matrix4x4::begin() const
    {
        return m_data;
    }

    Vector4* Matrix4x4::begin()
    {
        return m_data;
    }

    Matrix4x4 Matrix4x4::operator-()
    {
        Matrix4x4 result;
        for (size_t i = 0; i < 4; i++)
        {
            result[i] = -m_data[i];
        }
        return result;
    }

    Matrix4x4& Matrix4x4::operator*=(float other)
    {
        for (auto& element : m_data)
        {
            element *= other;
        }
        return *this;
    }

    Matrix4x4& Matrix4x4::operator/=(float other)
    {
        for (auto& element : m_data)
        {
            element /= other;
        }
        return *this;
    }

    Matrix4x4 Matrix4x4::operator*(float other) const
    {
        Matrix4x4 result = *this;
        result *= other;
        return result;
    }

    Matrix4x4 Matrix4x4::operator/(float other) const
    {
        Matrix4x4 result = *this;
        result /= other;
        return result;
    }

    Matrix4x4 Matrix4x4::operator*(const Matrix4x4& other) const
    {
        Matrix4x4 result;

        for (size_t i = 0; i < 4; i++)
        {
            for (size_t j = 0; j < 4; j++)
            {
                result[i] += other[j] * (*this)[i][j];
            }
        }

        return result;
    }

    Vector4 Matrix4x4::operator*(const Vector4& other) const
    {
        Vector4 result;

        for (size_t i = 0; i < 4; i++)
        {
            for (size_t j = 0; j < 4; j++)
            {
                result[i] = other[j] * (*this)[i][j];
            }
        }

        return result;
    }

    bool Matrix4x4::operator==(const Matrix4x4& other) const
    {
        return std::equal(std::begin(m_data), std::end(m_data), std::begin(other.m_data));
    }

    bool Matrix4x4::operator!=(const Matrix4x4& other) const
    {
        return !operator==(other);
    }

    Matrix4x4 Matrix4x4::Inverse(bool& isSuccessfull) const
    {
        Matrix4x4 result;

        result[0][0] = (*this)[1][1] * (*this)[2][2] * (*this)[3][3] -
            (*this)[1][1] * (*this)[2][3] * (*this)[3][2] -
            (*this)[2][1] * (*this)[1][2] * (*this)[3][3] +
            (*this)[2][1] * (*this)[1][3] * (*this)[3][2] +
            (*this)[3][1] * (*this)[1][2] * (*this)[2][3] -
            (*this)[3][1] * (*this)[1][3] * (*this)[2][2];

        result[1][0] = -(*this)[1][0] * (*this)[2][2] * (*this)[3][3] +
            (*this)[1][0] * (*this)[2][3] * (*this)[3][2] +
            (*this)[2][0] * (*this)[1][2] * (*this)[3][3] -
            (*this)[2][0] * (*this)[1][3] * (*this)[3][2] -
            (*this)[3][0] * (*this)[1][2] * (*this)[2][3] +
            (*this)[3][0] * (*this)[1][3] * (*this)[2][2];

        result[2][0] = (*this)[1][0] * (*this)[2][1] * (*this)[3][3] -
            (*this)[1][0] * (*this)[2][3] * (*this)[3][1] -
            (*this)[2][0] * (*this)[1][1] * (*this)[3][3] +
            (*this)[2][0] * (*this)[1][3] * (*this)[3][1] +
            (*this)[3][0] * (*this)[1][1] * (*this)[2][3] -
            (*this)[3][0] * (*this)[1][3] * (*this)[2][1];

        result[3][0] = -(*this)[1][0] * (*this)[2][1] * (*this)[3][2] +
            (*this)[1][0] * (*this)[2][2] * (*this)[3][1] +
            (*this)[2][0] * (*this)[1][1] * (*this)[3][2] -
            (*this)[2][0] * (*this)[1][2] * (*this)[3][1] -
            (*this)[3][0] * (*this)[1][1] * (*this)[2][2] +
            (*this)[3][0] * (*this)[1][2] * (*this)[2][1];

        result[0][1] = -(*this)[0][1] * (*this)[2][2] * (*this)[3][3] +
            (*this)[0][1] * (*this)[2][3] * (*this)[3][2] +
            (*this)[2][1] * (*this)[0][2] * (*this)[3][3] -
            (*this)[2][1] * (*this)[0][3] * (*this)[3][2] -
            (*this)[3][1] * (*this)[0][2] * (*this)[2][3] +
            (*this)[3][1] * (*this)[0][3] * (*this)[2][2];

        result[1][1] = (*this)[0][0] * (*this)[2][2] * (*this)[3][3] -
            (*this)[0][0] * (*this)[2][3] * (*this)[3][2] -
            (*this)[2][0] * (*this)[0][2] * (*this)[3][3] +
            (*this)[2][0] * (*this)[0][3] * (*this)[3][2] +
            (*this)[3][0] * (*this)[0][2] * (*this)[2][3] -
            (*this)[3][0] * (*this)[0][3] * (*this)[2][2];

        result[2][1] = -(*this)[0][0] * (*this)[2][1] * (*this)[3][3] +
            (*this)[0][0] * (*this)[2][3] * (*this)[3][1] +
            (*this)[2][0] * (*this)[0][1] * (*this)[3][3] -
            (*this)[2][0] * (*this)[0][3] * (*this)[3][1] -
            (*this)[3][0] * (*this)[0][1] * (*this)[2][3] +
            (*this)[3][0] * (*this)[0][3] * (*this)[2][1];

        result[3][1] = (*this)[0][0] * (*this)[2][1] * (*this)[3][2] -
            (*this)[0][0] * (*this)[2][2] * (*this)[3][1] -
            (*this)[2][0] * (*this)[0][1] * (*this)[3][2] +
            (*this)[2][0] * (*this)[0][2] * (*this)[3][1] +
            (*this)[3][0] * (*this)[0][1] * (*this)[2][2] -
            (*this)[3][0] * (*this)[0][2] * (*this)[2][1];

        result[0][2] = (*this)[0][1] * (*this)[1][2] * (*this)[3][3] -
            (*this)[0][1] * (*this)[1][3] * (*this)[3][2] -
            (*this)[1][1] * (*this)[0][2] * (*this)[3][3] +
            (*this)[1][1] * (*this)[0][3] * (*this)[3][2] +
            (*this)[3][1] * (*this)[0][2] * (*this)[1][3] -
            (*this)[3][1] * (*this)[0][3] * (*this)[1][2];

        result[1][2] = -(*this)[0][0] * (*this)[1][2] * (*this)[3][3] +
            (*this)[0][0] * (*this)[1][3] * (*this)[3][2] +
            (*this)[1][0] * (*this)[0][2] * (*this)[3][3] -
            (*this)[1][0] * (*this)[0][3] * (*this)[3][2] -
            (*this)[3][0] * (*this)[0][2] * (*this)[1][3] +
            (*this)[3][0] * (*this)[0][3] * (*this)[1][2];

        result[2][2] = (*this)[0][0] * (*this)[1][1] * (*this)[3][3] -
            (*this)[0][0] * (*this)[1][3] * (*this)[3][1] -
            (*this)[1][0] * (*this)[0][1] * (*this)[3][3] +
            (*this)[1][0] * (*this)[0][3] * (*this)[3][1] +
            (*this)[3][0] * (*this)[0][1] * (*this)[1][3] -
            (*this)[3][0] * (*this)[0][3] * (*this)[1][1];

        result[3][2] = -(*this)[0][0] * (*this)[1][1] * (*this)[3][2] +
            (*this)[0][0] * (*this)[1][2] * (*this)[3][1] +
            (*this)[1][0] * (*this)[0][1] * (*this)[3][2] -
            (*this)[1][0] * (*this)[0][2] * (*this)[3][1] -
            (*this)[3][0] * (*this)[0][1] * (*this)[1][2] +
            (*this)[3][0] * (*this)[0][2] * (*this)[1][1];

        result[0][3] = -(*this)[0][1] * (*this)[1][2] * (*this)[2][3] +
            (*this)[0][1] * (*this)[1][3] * (*this)[2][2] +
            (*this)[1][1] * (*this)[0][2] * (*this)[2][3] -
            (*this)[1][1] * (*this)[0][3] * (*this)[2][2] -
            (*this)[2][1] * (*this)[0][2] * (*this)[1][3] +
            (*this)[2][1] * (*this)[0][3] * (*this)[1][2];

        result[1][3] = (*this)[0][0] * (*this)[1][2] * (*this)[2][3] -
            (*this)[0][0] * (*this)[1][3] * (*this)[2][2] -
            (*this)[1][0] * (*this)[0][2] * (*this)[2][3] +
            (*this)[1][0] * (*this)[0][3] * (*this)[2][2] +
            (*this)[2][0] * (*this)[0][2] * (*this)[1][3] -
            (*this)[2][0] * (*this)[0][3] * (*this)[1][2];

        result[2][3] = -(*this)[0][0] * (*this)[1][1] * (*this)[2][3] +
            (*this)[0][0] * (*this)[1][3] * (*this)[2][1] +
            (*this)[1][0] * (*this)[0][1] * (*this)[2][3] -
            (*this)[1][0] * (*this)[0][3] * (*this)[2][1] -
            (*this)[2][0] * (*this)[0][1] * (*this)[1][3] +
            (*this)[2][0] * (*this)[0][3] * (*this)[1][1];

        result[3][3] = (*this)[0][0] * (*this)[1][1] * (*this)[2][2] -
            (*this)[0][0] * (*this)[1][2] * (*this)[2][1] -
            (*this)[1][0] * (*this)[0][1] * (*this)[2][2] +
            (*this)[1][0] * (*this)[0][2] * (*this)[2][1] +
            (*this)[2][0] * (*this)[0][1] * (*this)[1][2] -
            (*this)[2][0] * (*this)[0][2] * (*this)[1][1];

        float det = (*this)[0][0] * result[0][0] + (*this)[0][1] * result[1][0] +
            (*this)[0][2] * result[2][0] + (*this)[0][3] * result[3][0];

        if (::abs(det) < FLT_EPSILON)
        {
            isSuccessfull = false;
            return result;
        }

        isSuccessfull = true;

        result /= det;

        return result;
    }
    Matrix4x4 Matrix4x4::Transpose() const
    {
        return Matrix4x4{
            {(*this)[0][0], (*this)[1][0], (*this)[2][0], (*this)[3][0]},
            {(*this)[0][1], (*this)[1][1], (*this)[2][1], (*this)[3][1]},
            {(*this)[0][2], (*this)[1][2], (*this)[2][2], (*this)[3][2]},
            {(*this)[0][3], (*this)[1][3], (*this)[2][3], (*this)[3][3]},
        };
    }

    Matrix4x4 Matrix4x4::GetIdentity()
    {
        return Matrix4x4{
            {1.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f},
        };
    }

    Matrix4x4 Matrix4x4::GetZero()
    {
        return Matrix4x4{
            {0.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f, 0.0f},
        };
    }

    Matrix4x4 Matrix4x4::GetTranslation(float x, float y, float z)
    {
        return Matrix4x4{
            {1.0f, 0.0f, 0.0f, x},
            {0.0f, 1.0f, 0.0f, y},
            {0.0f, 0.0f, 1.0f, z},
            {0.0f, 0.0f, 0.0f, 1.0f},
        };
    }

    Matrix4x4 Matrix4x4::GetTranslation(const Vector4& xyz)
    {
        return Matrix4x4{
            {1.0f, 0.0f, 0.0f, xyz[0]},
            {0.0f, 1.0f, 0.0f, xyz[1]},
            {0.0f, 0.0f, 1.0f, xyz[2]},
            {0.0f, 0.0f, 0.0f, 1.0f},
        };
    }

    Matrix4x4 Matrix4x4::GetScale(float uniformScale)
    {
        return Matrix4x4{
            {uniformScale, 0.0f, 0.0f, 0.0f},
            {0.0f, uniformScale, 0.0f, 0.0f},
            {0.0f, 0.0f, uniformScale, 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f},
        };
    }

    Matrix4x4 Matrix4x4::GetScale(float x, float y, float z)
    {
        return Matrix4x4{
            {x, 0.0f, 0.0f, 0.0f},
            {0.0f, y, 0.0f, 0.0f},
            {0.0f, 0.0f, z, 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f},
        };
    }

    Matrix4x4 Matrix4x4::GetRotationX(float angle)
    {
        return Matrix4x4{
            {1.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, cosf(angle), -sinf(angle), 0.0f},
            {0.0f, sinf(angle), cosf(angle), 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f},
        };
    }

    Matrix4x4 Matrix4x4::GetRotationY(float angle)
    {
        return Matrix4x4{
            {cosf(angle), 0.0f, sinf(angle), 0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {-sinf(angle), 0.0f, cosf(angle), 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f},
        };
    }

    Matrix4x4 Matrix4x4::GetRotationZ(float angle)
    {
        return Matrix4x4{
            {cosf(angle), -sinf(angle), 0.0f, 0.0f},
            {sinf(angle), cosf(angle), 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f},
        };
    }

    Matrix4x4 Matrix4x4::LookAt(const Vector4& eye, const Vector4& center, const Vector4& up)
    {
        Vector4 dir = (center - eye).Normalize();
        Vector4 up_norm = up.Normalize();
        Vector4 cross = (Cross(dir, up_norm)).Normalize();
        up_norm = Cross(cross, dir);

        Matrix4x4 m = GetIdentity();

        m[0][0] = cross[0];
        m[1][0] = cross[1];
        m[2][0] = cross[2];
        m[0][1] = up_norm[0];
        m[1][1] = up_norm[1];
        m[2][1] = up_norm[2];
        m[0][2] = -dir[0];
        m[1][2] = -dir[1];
        m[2][2] = -dir[2];
        m[3][0] = -Dot(cross, eye);
        m[3][1] = -Dot(up_norm, eye);
        m[3][2] = Dot(dir, eye);

        return m;
    }

    Matrix4x4 Matrix4x4::Perspective(float fov, float aspect, float zNear, float zFar)
    {
        assert(abs(aspect - std::numeric_limits<float>::epsilon()) > static_cast<float>(0));

        const float tanHalfFov = tanf(fov / 2.0f);

        Matrix4x4 result = GetZero();
        result[0][0] = 1.0f / (aspect * tanHalfFov);
        result[1][1] = 1.0f / (tanHalfFov);
        result[2][2] = -zFar / (zFar - zNear);
        result[2][3] = -1.0f;
        result[3][2] = -(zFar * zNear) / (zFar - zNear);
        return result;
    }

    Matrix4x4 Matrix4x4::Ortho(float left, float right, float top, float bottom, float zNear, float zFar)
    {
        Matrix4x4 result = Matrix4x4::GetIdentity();

        result[0][0] = 2.0f / (right - left);
        result[1][1] = 2.0f / (top - bottom);
        result[2][2] = -2.0f / (zFar - zNear);
        result[3][0] = -(right + left) / (right - left);
        result[3][1] = -(top + bottom) / (top - bottom);
        result[3][2] = -(zFar + zNear) / (zFar - zNear);

        return result;
    }

    Matrix4x4 Matrix4x4::CalculateView(const Vector4& right, const Vector4& up,
        const Vector4& forward, const Vector4& position)
    {
        const Vector4 minusPos = -position;

        Matrix4x4 view{
            {right[0], up[0], forward[0], 0.0f},
            {right[1], up[1], forward[1], 0.0f},
            {right[2], up[2], forward[2], 0.0f},
            {Dot(minusPos, right), Dot(minusPos, up), Dot(minusPos, forward), 1.0f},
        };

        return view;
    }

    Matrix4x4 Matrix4x4::CalculateCubeMapView(size_t cubeMapFace, const Vector4& position)
    {
        static const Vector4 right[6] = {
            {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},  {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
        };

        static const Vector4 up[6] = {
            {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f},
            {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        };

        static const Vector4 forward[6] = {
            {1.0f, 0.0f, 0.0f},  {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
            {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},  {0.0f, 0.0f, -1.0f},
        };

        return CalculateView(right[cubeMapFace], up[cubeMapFace], forward[cubeMapFace], position);
    }

    Matrix4x4 Matrix4x4::CalculateProjPerspective(float aspectRatio, float fovY, float nearZ, float farZ)
    {
        float h = 1.0f / tanf(fovY * 0.5f);
        float w = h / aspectRatio;
        float a = farZ / (farZ - nearZ);
        float b = (-nearZ * farZ) / (farZ - nearZ);

        Matrix4x4 projMatrix = {
            {w, 0, 0, 0},
            {0, h, 0, 0},
            {0, 0, a, 1},
            {0, 0, b, 0},
        };

        return projMatrix;
    }

    Matrix4x4 Matrix4x4::CalculateProjOrtographic(float nearZ, float farZ, float width,
        float height)
    {
        float w = 2.0f / width;
        float h = 2.0f / height;
        float a = 1.0f / (farZ - nearZ);
        float b = -a * nearZ;

        Matrix4x4 projMatrix = {
            {w, 0, 0, 0},
            {0, h, 0, 0},
            {0, 0, a, 0},
            {0, 0, b, 1},
        };

        return projMatrix;
    }

    Matrix4x4 Matrix4x4::Translate(const Matrix4x4& mat, const Vector3& vec)
    {
        Matrix4x4 result(mat);
        result[3] = mat[0] * vec[0] + mat[1] * vec[1] + mat[2] * vec[2] + mat[3];
        return result;
    }

    Matrix4x4 Matrix4x4::Rotate(const Matrix4x4& mat, float angle, const Vector3& vec)
    {
        const float a = angle;
        const float c = cosf(a);
        const float s = sinf(a);

        Vector3 axis(vec.Normalize());
        Vector3 temp(axis * (1.0f - c));

        Matrix4x4 rotate;
        rotate[0][0] = c + temp[0] * axis[0];
        rotate[0][1] = temp[0] * axis[1] + s * axis[2];
        rotate[0][2] = temp[0] * axis[2] - s * axis[1];

        rotate[1][0] = temp[1] * axis[0] - s * axis[2];
        rotate[1][1] = c + temp[1] * axis[1];
        rotate[1][2] = temp[1] * axis[2] + s * axis[0];

        rotate[2][0] = temp[2] * axis[0] + s * axis[1];
        rotate[2][1] = temp[2] * axis[1] - s * axis[0];
        rotate[2][2] = c + temp[2] * axis[2];

        Matrix4x4 Result;
        Result[0] = mat[0] * rotate[0][0] + mat[1] * rotate[0][1] + mat[2] * rotate[0][2];
        Result[1] = mat[0] * rotate[1][0] + mat[1] * rotate[1][1] + mat[2] * rotate[1][2];
        Result[2] = mat[0] * rotate[2][0] + mat[1] * rotate[2][1] + mat[2] * rotate[2][2];
        Result[3] = mat[3];
        return Result;
    }

    Matrix4x4 Matrix4x4::Scale(const Matrix4x4& mat, const Vector3& vec)
    {
        Matrix4x4 rotate;
        rotate[0] = mat[0] * vec[0];
        rotate[1] = mat[1] * vec[1];
        rotate[2] = mat[2] * vec[2];
        rotate[3] = mat[3];
        return rotate;
    }

    Matrix4x4 Matrix4x4::GetUVToTexCoord()
    {
        return Matrix4x4{
            {0.5f, 0, 0, 0},
            {0, -0.5f, 0, 0},
            {0, 0, 1, 0},
            {0.5f, 0.5f, 0, 1},
        };
    }

    Matrix4x4 Matrix4x4::GetTexCoordToUV()
    {
        return Matrix4x4{
            {2.0f, 0, 0, 0},
            {0, -2.0f, 0, 0},
            {0, 0, 1, 0},
            {-1.0f, 1.0f, 0, 1},
        };
    }

    Vector4 operator*(const Vector4& first, const Matrix4x4& second)
    {
        Vector4 result;

        for (size_t i = 0; i < 4; i++)
        {
            result += second[i] * first[i];
        }

        return result;
    }

}
