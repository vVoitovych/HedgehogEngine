#pragma once

#include <cstdint>
#include <initializer_list>
#include <algorithm>

namespace HM
{
    template <size_t componentCount, typename elementType = float>
    class Vector
    {
    public:
        using thisType = Vector<componentCount, elementType>;

        Vector();

        ~Vector() = default;

        Vector(const Vector&) = default;
        Vector(Vector&&) = default;
        Vector& operator=(const Vector&) = default;
        Vector& operator=(Vector&&) = default;

        Vector(std::initializer_list<elementType> data);
        Vector(const elementType* first, const elementType* last);

        template <size_t otherComponentCount>
        Vector(const Vector<otherComponentCount, elementType>& other);

        template <typename otherType>
        Vector(const Vector<componentCount, otherType>& other);

        template <size_t firstComponentCount, typename... Args>
        Vector(const Vector<firstComponentCount, elementType>& first, Args... args);

        template <typename... Args>
        Vector(elementType first, Args... args);

        elementType x() const;
        elementType y() const;
        elementType z() const;
        elementType w() const;

        elementType& x();
        elementType& y();
        elementType& z();
        elementType& w();

        elementType r() const;
        elementType g() const;
        elementType b() const;
        elementType a() const;

        elementType& r();
        elementType& g();
        elementType& b();
        elementType& a();

        elementType& operator[](size_t i);
        const elementType& operator[](size_t i) const;

        elementType* GetBuffer();
        const elementType* GetBuffer() const;

        size_t size() const;

        elementType* begin();
        elementType* end();
        const elementType* begin() const;
        const elementType* end() const;

        elementType LengthSlow() const;
        elementType LengthSqr() const;
        thisType Normalize() const;

        elementType Length3Slow() const;
        elementType Length3Sqr() const;
        thisType Normalize3() const;

        thisType Min(const thisType& other) const;
        thisType Max(const thisType& other) const;

        thisType Select(const thisType& other, const thisType& mask) const;

        thisType operator-() const;

        thisType& operator*=(elementType other);
        thisType& operator/=(elementType other);
        thisType operator*(elementType other) const;
        thisType operator/(elementType other) const;

        thisType& operator*=(const thisType& other);
        thisType& operator/=(const thisType& other);
        thisType& operator+=(const thisType& other);
        thisType& operator-=(const thisType& other);
        thisType operator*(const thisType& other) const;
        thisType operator/(const thisType& other) const;
        thisType operator+(const thisType& other) const;
        thisType operator-(const thisType& other) const;

        bool operator==(const thisType& other) const;
        bool operator!=(const thisType& other) const;

        thisType operator>(const thisType& other) const;
        thisType operator<(const thisType& other) const;
        thisType operator>=(const thisType& other) const;
        thisType operator<=(const thisType& other) const;

    protected:
        elementType m_data[componentCount];

        template <size_t currentIndex, size_t firstComponentCount, typename... Args>
        void InitializeTemplateList(const Vector<firstComponentCount, elementType>& first,
            Args... args);
        template <size_t currentIndex, typename... Args>
        void InitializeTemplateList(elementType first, Args... args);
        template <size_t currentIndex>
        void InitializeTemplateList();
    };

    template <size_t componentCount, typename elementType>
    elementType Dot(const Vector<componentCount, elementType>& first, const Vector<componentCount, elementType>& second)
    {
        elementType result = 0;
        for (size_t i = 0; i < componentCount; i++)
        {
            result += first[i] * second[i];
        }

        return result;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Cross(const Vector<componentCount, elementType>& first, const Vector<componentCount, elementType>& second)
    {
        static_assert(componentCount >= 3);

        return Vector<componentCount, elementType>{ first[1] * second[2] - first[2] * second[1],
                        first[2] * second[0] - first[0] * second[2],
                        first[0] * second[1] - first[1] * second[0] };
    }

    template <size_t componentCount, typename elementType>
    template <typename otherType>
    Vector<componentCount, elementType>::Vector(const Vector<componentCount, otherType>& other)
    {
        for (size_t i = 0; i < componentCount; ++i)
        {
            m_data[i] = other[i];
        }
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType>::Vector()
        : m_data{}
    {
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType>::Vector(std::initializer_list<elementType> data)
        : m_data{}
    {
        std::copy(data.begin(), data.end(), begin());
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType>::Vector(const elementType* first, const elementType* last)
        : m_data{}
    {
        std::copy(first, last, begin());
    }

    template <size_t componentCount, typename elementType>
    template <size_t otherComponentCount>
    Vector<componentCount, elementType>::Vector(
        const Vector<otherComponentCount, elementType>& other)
        : m_data{}
    {
        if constexpr (otherComponentCount > componentCount)
        {
            std::copy(other.begin(), other.begin() + componentCount, begin());
        }
        else
        {
            std::copy(other.begin(), other.end(), begin());
        }
    }

    template <size_t componentCount, typename elementType>
    template <size_t firstComponentCount, typename... Args>
    Vector<componentCount, elementType>::Vector(
        const Vector<firstComponentCount, elementType>& first, Args... args)
        : m_data{}
    {
        InitializeTemplateList<0>(first, args...);
    }

    template <size_t componentCount, typename elementType>
    template <typename... Args>
    Vector<componentCount, elementType>::Vector(elementType first, Args... args)
        : m_data{}
    {
        InitializeTemplateList<0>(first, args...);
    }

    template <size_t componentCount, typename elementType>
    elementType Vector<componentCount, elementType>::x() const
    {
        static_assert(componentCount > 0);
        return m_data[0];
    };
    template <size_t componentCount, typename elementType>
    elementType Vector<componentCount, elementType>::y() const
    {
        static_assert(componentCount > 1);
        return m_data[1];
    };
    template <size_t componentCount, typename elementType>
    elementType Vector<componentCount, elementType>::z() const
    {
        static_assert(componentCount > 2);
        return m_data[2];
    };
    template <size_t componentCount, typename elementType>
    elementType Vector<componentCount, elementType>::w() const
    {
        static_assert(componentCount > 3);
        return m_data[3];
    };

    template <size_t componentCount, typename elementType>
    elementType& Vector<componentCount, elementType>::x()
    {
        static_assert(componentCount > 0);
        return m_data[0];
    };
    template <size_t componentCount, typename elementType>
    elementType& Vector<componentCount, elementType>::y()
    {
        static_assert(componentCount > 1);
        return m_data[1];
    };
    template <size_t componentCount, typename elementType>
    elementType& Vector<componentCount, elementType>::z()
    {
        static_assert(componentCount > 2);
        return m_data[2];
    };
    template <size_t componentCount, typename elementType>
    elementType& Vector<componentCount, elementType>::w()
    {
        static_assert(componentCount > 3);
        return m_data[3];
    };

    template <size_t componentCount, typename elementType>
    elementType Vector<componentCount, elementType>::r() const
    {
        static_assert(componentCount > 0);
        return m_data[0];
    };
    template <size_t componentCount, typename elementType>
    elementType Vector<componentCount, elementType>::g() const
    {
        static_assert(componentCount > 1);
        return m_data[1];
    };
    template <size_t componentCount, typename elementType>
    elementType Vector<componentCount, elementType>::b() const
    {
        static_assert(componentCount > 2);
        return m_data[2];
    };
    template <size_t componentCount, typename elementType>
    elementType Vector<componentCount, elementType>::a() const
    {
        static_assert(componentCount > 3);
        return m_data[3];
    };

    template <size_t componentCount, typename elementType>
    elementType& Vector<componentCount, elementType>::r()
    {
        static_assert(componentCount > 0);
        return m_data[0];
    };
    template <size_t componentCount, typename elementType>
    elementType& Vector<componentCount, elementType>::g()
    {
        static_assert(componentCount > 1);
        return m_data[1];
    };
    template <size_t componentCount, typename elementType>
    elementType& Vector<componentCount, elementType>::b()
    {
        static_assert(componentCount > 2);
        return m_data[2];
    };
    template <size_t componentCount, typename elementType>
    elementType& Vector<componentCount, elementType>::a()
    {
        static_assert(componentCount > 3);
        return m_data[3];
    };

    template <size_t componentCount, typename elementType>
    elementType& Vector<componentCount, elementType>::operator[](size_t i)
    {
        return m_data[i];
    };
    template <size_t componentCount, typename elementType>
    const elementType& Vector<componentCount, elementType>::operator[](size_t i) const
    {
        return m_data[i];
    };

    template <size_t componentCount, typename elementType>
    elementType* Vector<componentCount, elementType>::GetBuffer()
    {
        return m_data;
    }
    template <size_t componentCount, typename elementType>
    const elementType* Vector<componentCount, elementType>::GetBuffer() const
    {
        return m_data;
    }

    template <size_t componentCount, typename elementType>
    elementType* Vector<componentCount, elementType>::begin()
    {
        return m_data;
    }
    template <size_t componentCount, typename elementType>
    elementType* Vector<componentCount, elementType>::end()
    {
        return m_data + componentCount;
    }
    template <size_t componentCount, typename elementType>
    const elementType* Vector<componentCount, elementType>::begin() const
    {
        return m_data;
    }
    template <size_t componentCount, typename elementType>
    const elementType* Vector<componentCount, elementType>::end() const
    {
        return m_data + componentCount;
    }

    template <size_t componentCount, typename elementType>
    size_t Vector<componentCount, elementType>::size() const
    {
        return componentCount;
    }

    template <size_t componentCount, typename elementType>
    elementType Vector<componentCount, elementType>::LengthSlow() const
    {
        return sqrtf(LengthSqr());
    }

    template <size_t componentCount, typename elementType>
    elementType Vector<componentCount, elementType>::LengthSqr() const
    {
        elementType result = 0;
        for (auto& value : m_data)
        {
            result += value * value;
        }

        return result;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Vector<componentCount, elementType>::Normalize() const
    {
        return (*this) / LengthSlow();
    }

    template <size_t componentCount, typename elementType>
    elementType Vector<componentCount, elementType>::Length3Slow() const
    {
        static_assert(componentCount >= 3);
        return sqrtf(Length3Sqr());
    }

    template <size_t componentCount, typename elementType>
    elementType Vector<componentCount, elementType>::Length3Sqr() const
    {
        static_assert(componentCount >= 3);
        elementType result = 0;
        for (size_t i = 0; i < 3; ++i)
        {
            result += m_data[i] * m_data[i];
        }

        return result;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Vector<componentCount, elementType>::Normalize3() const
    {
        static_assert(componentCount >= 3);
        return (*this) / Length3Slow();
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Vector<componentCount, elementType>::Min(
        const thisType& other) const
    {
        Vector<componentCount, elementType> result;
        for (size_t i = 0; i < componentCount; ++i)
        {
            result[i] = std::min((*this)[i], other[i]);
        }
        return result;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Vector<componentCount, elementType>::Max(
        const thisType& other) const
    {
        Vector<componentCount, elementType> result;
        for (size_t i = 0; i < componentCount; ++i)
        {
            result[i] = std::max((*this)[i], other[i]);
        }
        return result;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Vector<componentCount, elementType>::Select(
        const thisType& other, const thisType& mask) const
    {
        Vector<componentCount, elementType> result;
        for (size_t i = 0; i < componentCount; ++i)
        {
            result[i] = mask[i] == 0.0f ? (*this)[i] : other[i];
        }
        return result;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Vector<componentCount, elementType>::operator-() const
    {
        thisType result;
        for (size_t i = 0; i < componentCount; i++)
        {
            result[i] = -(*this)[i];
        }

        return result;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType>& Vector<componentCount, elementType>::operator*=(
        elementType other)
    {
        for (elementType& element : m_data)
        {
            element *= other;
        }

        return *this;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType>& Vector<componentCount, elementType>::operator/=(
        elementType other)
    {
        for (elementType& element : m_data)
        {
            element /= other;
        }

        return *this;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Vector<componentCount, elementType>::operator*(
        elementType other) const
    {
        thisType result = *this;
        result *= other;
        return result;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Vector<componentCount, elementType>::operator/(
        elementType other) const
    {
        thisType result = *this;
        result /= other;
        return result;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType>& Vector<componentCount, elementType>::operator*=(
        const thisType& other)
    {
        for (size_t i = 0; i < componentCount; ++i)
        {
            (*this)[i] *= other[i];
        }
        return *this;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType>& Vector<componentCount, elementType>::operator/=(
        const thisType& other)
    {
        for (size_t i = 0; i < componentCount; ++i)
        {
            (*this)[i] /= other[i];
        }
        return *this;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType>& Vector<componentCount, elementType>::operator+=(
        const thisType& other)
    {
        for (size_t i = 0; i < componentCount; ++i)
        {
            (*this)[i] += other[i];
        }
        return *this;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType>& Vector<componentCount, elementType>::operator-=(
        const thisType& other)
    {
        for (size_t i = 0; i < componentCount; ++i)
        {
            (*this)[i] -= other[i];
        }
        return *this;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Vector<componentCount, elementType>::operator*(
        const thisType& other) const
    {
        thisType result = *this;
        result *= other;
        return result;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Vector<componentCount, elementType>::operator/(
        const thisType& other) const
    {
        thisType result = *this;
        result /= other;
        return result;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Vector<componentCount, elementType>::operator+(
        const thisType& other) const
    {
        thisType result = *this;
        result += other;
        return result;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Vector<componentCount, elementType>::operator-(
        const thisType& other) const
    {
        thisType result = *this;
        result -= other;
        return result;
    }

    template <size_t componentCount, typename elementType>
    bool Vector<componentCount, elementType>::operator==(const thisType& other) const
    {
        return std::equal(std::begin(m_data), std::end(m_data), std::begin(other.m_data));
    }

    template <size_t componentCount, typename elementType>
    bool Vector<componentCount, elementType>::operator!=(const thisType& other) const
    {
        return !operator==(other);
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Vector<componentCount, elementType>::operator>(
        const thisType& other) const
    {
        thisType result;
        for (size_t i = 0; i < componentCount; ++i)
        {
            result[i] = (*this)[i] > other[i] ? 1.0f : 0.0f;
        }
        return result;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Vector<componentCount, elementType>::operator<(
        const thisType& other) const
    {
        thisType result;
        for (size_t i = 0; i < componentCount; ++i)
        {
            result[i] = (*this)[i] < other[i] ? 1.0f : 0.0f;
        }
        return result;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Vector<componentCount, elementType>::operator>=(
        const thisType& other) const
    {
        thisType result;
        for (size_t i = 0; i < componentCount; ++i)
        {
            result[i] = (*this)[i] >= other[i] ? 1.0f : 0.0f;
        }
        return result;
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Vector<componentCount, elementType>::operator<=(
        const thisType& other) const
    {
        thisType result;
        for (size_t i = 0; i < componentCount; ++i)
        {
            result[i] = (*this)[i] <= other[i] ? 1.0f : 0.0f;
        }
        return result;
    }


    template <size_t componentCount, typename elementType>
    template <size_t currentIndex, size_t firstComponentCount, typename... Args>
    void Vector<componentCount, elementType>::InitializeTemplateList(
        const Vector<firstComponentCount, elementType>& first, Args... args)
    {
        static_assert(currentIndex + firstComponentCount <= componentCount);
        std::copy(first.begin(), first.end(), &m_data[currentIndex]);
        InitializeTemplateList<currentIndex + firstComponentCount>(args...);
    }
    template <size_t componentCount, typename elementType>
    template <size_t currentIndex, typename... Args>
    void Vector<componentCount, elementType>::InitializeTemplateList(elementType first,
        Args... args)
    {
        static_assert(currentIndex + 1 <= componentCount);
        m_data[currentIndex] = first;
        InitializeTemplateList<currentIndex + 1>(args...);
    }
    template <size_t componentCount, typename elementType>
    template <size_t currentIndex>
    void Vector<componentCount, elementType>::InitializeTemplateList()
    {
        static_assert(currentIndex == componentCount, "Incorrect number of elements passed");
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Min(const Vector<componentCount, elementType>& first,
        const Vector<componentCount, elementType>& second)
    {
        return first.Min(second);
    }

    template <size_t componentCount, typename elementType>
    Vector<componentCount, elementType> Max(const Vector<componentCount, elementType>& first,
        const Vector<componentCount, elementType>& second)
    {
        return first.Max(second);
    }

    using Vector2 = Vector<2>;
    using Vector3 = Vector<3>;
    using Vector4 = Vector<4>;

    using Vector2u = Vector<2, uint32_t>;
    using Vector3u = Vector<3, uint32_t>;
    using Vector4u = Vector<4, uint32_t>;

} // namespace Boolka


namespace std
{
    template<>
    struct hash<HM::Vector2>
    {
        size_t operator()(HM::Vector2 const& vector) const
        {
            return (
                (hash<float>()(vector.x()) ^
                    (hash<float>()(vector.y()) << 1))
                );
        }
    };

    template<>
    struct hash<HM::Vector3>
    {
        size_t operator()(HM::Vector3 const& vector) const
        {
            return (
                (hash<float>()(vector.x()) ^
                    (hash<float>()(vector.y()) << 1) ^
                    (hash<float>()(vector.z())))
                );
        }
    };

    template<>
    struct hash<HM::Vector4>
    {
        size_t operator()(HM::Vector4 const& vector) const
        {
            return (
                (hash<float>()(vector.x()) ^
                    (hash<float>()(vector.y()) << 1) ^
                    (hash<float>()(vector.z())) ^
                    (hash<float>()(vector.w()) << 1)
                    )
                );
        }
    };
}




