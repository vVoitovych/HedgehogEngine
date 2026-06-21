#pragma once

#include "HedgehogMath/api/Vector.hpp"
#include "HedgehogMath/api/Matrix.hpp"

#include <cstdint>
#include <string>
#include <type_traits>

namespace Reflection
{
    enum class TypeTag : uint8_t
    {
        UInt,
        Int,
        Float,
        Double,
        Bool,
        String,
        Vec2,
        Vec3,
        Vec4,
        Enum,
        Raw,
    };

    template<typename T> constexpr TypeTag TypeTagOf();

    template<> constexpr TypeTag TypeTagOf<uint32_t>()    { return TypeTag::UInt;   }
    template<> constexpr TypeTag TypeTagOf<int32_t>()     { return TypeTag::Int;    }
    template<> constexpr TypeTag TypeTagOf<float>()       { return TypeTag::Float;  }
    template<> constexpr TypeTag TypeTagOf<double>()      { return TypeTag::Double; }
    template<> constexpr TypeTag TypeTagOf<bool>()        { return TypeTag::Bool;   }
    template<> constexpr TypeTag TypeTagOf<std::string>() { return TypeTag::String; }
    template<> constexpr TypeTag TypeTagOf<HM::Vector2>() { return TypeTag::Vec2;   }
    template<> constexpr TypeTag TypeTagOf<HM::Vector3>() { return TypeTag::Vec3;   }
    template<> constexpr TypeTag TypeTagOf<HM::Vector4>() { return TypeTag::Vec4;   }
    template<> constexpr TypeTag TypeTagOf<HM::Matrix4x4>() { return TypeTag::Raw; }

    template<typename T>
    constexpr TypeTag TypeTagOf()
    {
        static_assert(std::is_enum_v<T>, "Unsupported reflection type — add a TypeTagOf<T> specialisation or use HH_PROP_NAMED with TypeTag::Raw");
        return TypeTag::Enum;
    }
}
