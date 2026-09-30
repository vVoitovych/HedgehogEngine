#include "Bindings.hpp"

#include "HedgehogMath/api/Quaternion.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <cstdio>
#include <string>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        using HM::Quaternion;
        using HM::Vector3;

        std::string Format(const char* format, float a, float b, float c)
        {
            char buffer[96];
            std::snprintf(buffer, sizeof(buffer), format, a, b, c);
            return buffer;
        }

        std::string Format(const char* format, float a, float b, float c, float d)
        {
            char buffer[128];
            std::snprintf(buffer, sizeof(buffer), format, a, b, c, d);
            return buffer;
        }

        void RegisterVector3(sol::state& lua)
        {
            lua.new_usertype<Vector3>(
                "Vector3",
                sol::call_constructor,
                sol::factories([]() { return Vector3(0.0f, 0.0f, 0.0f); },
                               [](float x, float y, float z) { return Vector3(x, y, z); }),

                "x", sol::property([](const Vector3& v) { return v.x(); }, [](Vector3& v, float value) { v.x() = value; }),
                "y", sol::property([](const Vector3& v) { return v.y(); }, [](Vector3& v, float value) { v.y() = value; }),
                "z", sol::property([](const Vector3& v) { return v.z(); }, [](Vector3& v, float value) { v.z() = value; }),

                "dot", [](const Vector3& a, const Vector3& b) { return HM::Dot(a, b); },
                "cross", [](const Vector3& a, const Vector3& b) { return HM::Cross(a, b); },
                "length", [](const Vector3& v) { return v.LengthSlow(); },
                // A zero vector stays zero rather than becoming NaN.
                "normalized", [](const Vector3& v) { return v.LengthSqr() > 0.0f ? v.Normalize() : v; },
                "lerp", [](const Vector3& a, const Vector3& b, float t) { return a + (b - a) * t; },

                sol::meta_function::addition, [](const Vector3& a, const Vector3& b) { return a + b; },
                sol::meta_function::subtraction, [](const Vector3& a, const Vector3& b) { return a - b; },
                sol::meta_function::multiplication,
                sol::overload([](const Vector3& v, float s) { return v * s; },
                              [](float s, const Vector3& v) { return v * s; },
                              [](const Vector3& a, const Vector3& b) { return a * b; }),
                sol::meta_function::division,
                sol::overload([](const Vector3& v, float s) { return v / s; },
                              [](const Vector3& a, const Vector3& b) { return a / b; }),
                sol::meta_function::unary_minus, [](const Vector3& v) { return -v; },
                sol::meta_function::equal_to, [](const Vector3& a, const Vector3& b) { return a == b; },
                sol::meta_function::to_string,
                [](const Vector3& v) { return Format("Vector3(%g, %g, %g)", v.x(), v.y(), v.z()); });
        }

        void RegisterQuaternion(sol::state& lua)
        {
            lua.new_usertype<Quaternion>(
                "Quat",
                sol::call_constructor,
                sol::factories([]() { return Quaternion(); },
                               [](float x, float y, float z, float w) { return Quaternion(x, y, z, w); }),

                // Read-only: build a new rotation instead of editing a component.
                "x", sol::readonly_property([](const Quaternion& q) { return q.x(); }),
                "y", sol::readonly_property([](const Quaternion& q) { return q.y(); }),
                "z", sol::readonly_property([](const Quaternion& q) { return q.z(); }),
                "w", sol::readonly_property([](const Quaternion& q) { return q.w(); }),

                "identity", []() { return Quaternion::Identity(); },
                "fromEuler",
                sol::overload([](float x, float y, float z) { return Quaternion::FromEuler(x, y, z); },
                              [](const Vector3& degrees) { return Quaternion::FromEuler(degrees); }),
                "axisAngle", [](const Vector3& axis, float degrees) { return Quaternion::FromAxisAngle(axis, degrees); },
                "lookRotation",
                sol::overload([](const Vector3& forward) { return Quaternion::LookRotation(forward); },
                              [](const Vector3& forward, const Vector3& up) { return Quaternion::LookRotation(forward, up); }),
                "slerp", [](const Quaternion& a, const Quaternion& b, float t) { return Quaternion::Slerp(a, b, t); },

                "euler", [](const Quaternion& q) { return q.ToEuler(); },
                "inverse", [](const Quaternion& q) { return q.Inverse(); },
                "normalized", [](const Quaternion& q) { return q.Normalize(); },

                sol::meta_function::multiplication,
                sol::overload([](const Quaternion& a, const Quaternion& b) { return a * b; },
                              [](const Quaternion& q, const Vector3& v) { return q * v; }),
                sol::meta_function::equal_to, [](const Quaternion& a, const Quaternion& b) { return a == b; },
                sol::meta_function::to_string,
                [](const Quaternion& q) { return Format("Quat(%g, %g, %g, %g)", q.x(), q.y(), q.z(), q.w()); });
        }
    }

    void RegisterMath(sol::state& lua)
    {
        RegisterVector3(lua);
        RegisterQuaternion(lua);
    }
}
