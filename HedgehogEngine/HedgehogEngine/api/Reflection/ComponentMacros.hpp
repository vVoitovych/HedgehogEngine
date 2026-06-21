#pragma once

#include "EcsSerialization/api/Reflection/PropertyDescriptor.hpp"

#include <span>
#include <vector>

// ─── HH_BEGIN_COMPONENT ──────────────────────────────────────────────────────
// Opens a reflected component struct.
// Provides s_TypeName, GetProperties(), and the internal self-registration
// mechanism. Each HH_PROP/HH_PROP_NAMED inside registers a PropertyDescriptor
// into a per-type static vector at program startup, in declaration order.
// Non-reflected fields may be declared between HH_PROP calls normally.
// Close with HH_END_COMPONENT.

#define HH_BEGIN_COMPONENT(Name_)                                                   \
struct Name_ {                                                                       \
    using _Self = Name_;                                                            \
    static constexpr const char* s_TypeName = #Name_;                              \
    static std::vector<::Reflection::PropertyDescriptor>& _GetPropTable() {        \
        static std::vector<::Reflection::PropertyDescriptor> t;                     \
        return t;                                                                   \
    }                                                                               \
    static std::span<const ::Reflection::PropertyDescriptor> GetProperties() {     \
        return _GetPropTable();                                                     \
    }

// ─── HH_PROP ─────────────────────────────────────────────────────────────────
// Reflected field. The C++ field name is used as the YAML serialisation key.
// Signature: HH_PROP(Type, fieldName, defaultValue, Flags)

#define HH_PROP(T_, name_, def_, flags_)                                            \
    T_ name_ = def_;                                                                \
    static void* _acc_##name_(void* c_) {                                          \
        return &static_cast<_Self*>(c_)->name_;                                    \
    }                                                                               \
    inline static bool _prop_##name_ = (_GetPropTable().push_back(                 \
        ::Reflection::PropertyDescriptor{                                            \
            ::Reflection::TypeTagOf<T_>(),                                          \
            #name_,                                                                 \
            _acc_##name_,                                                           \
            ::Reflection::PropertyFlags::flags_,                                    \
            0.0f, 0.0f                                                              \
        }), true);

// ─── HH_PROP_NAMED ────────────────────────────────────────────────────────────
// Like HH_PROP but with an explicit YAML key — use when migrating components
// that already have scene files written with a different key convention.
// Signature: HH_PROP_NAMED(Type, fieldName, "yamlKey", defaultValue, Flags)

#define HH_PROP_NAMED(T_, name_, yamlKey_, def_, flags_)                            \
    T_ name_ = def_;                                                                \
    static void* _acc_##name_(void* c_) {                                          \
        return &static_cast<_Self*>(c_)->name_;                                    \
    }                                                                               \
    inline static bool _prop_##name_ = (_GetPropTable().push_back(                 \
        ::Reflection::PropertyDescriptor{                                            \
            ::Reflection::TypeTagOf<T_>(),                                          \
            yamlKey_,                                                               \
            _acc_##name_,                                                           \
            ::Reflection::PropertyFlags::flags_,                                    \
            0.0f, 0.0f                                                              \
        }), true);

// ─── HH_PROP_SLIDER ──────────────────────────────────────────────────────────
// Reflected float/int field rendered as a slider widget in the editor.
// Signature: HH_PROP_SLIDER(Type, fieldName, defaultValue, sliderMin, sliderMax)

#define HH_PROP_SLIDER(T_, name_, def_, min_, max_)                                 \
    T_ name_ = def_;                                                                \
    static void* _acc_##name_(void* c_) {                                          \
        return &static_cast<_Self*>(c_)->name_;                                    \
    }                                                                               \
    inline static bool _prop_##name_ = (_GetPropTable().push_back(                 \
        ::Reflection::PropertyDescriptor{                                            \
            ::Reflection::TypeTagOf<T_>(),                                          \
            #name_,                                                                 \
            _acc_##name_,                                                           \
            ::Reflection::PropertyFlags::IsSlider,                                  \
            static_cast<float>(min_), static_cast<float>(max_)                     \
        }), true);

// ─── HH_PROP_NAMED_SLIDER ─────────────────────────────────────────────────────
// Like HH_PROP_SLIDER but with an explicit YAML key.
// Signature: HH_PROP_NAMED_SLIDER(Type, fieldName, "yamlKey", defaultValue, sliderMin, sliderMax)

#define HH_PROP_NAMED_SLIDER(T_, name_, yamlKey_, def_, min_, max_)                 \
    T_ name_ = def_;                                                                \
    static void* _acc_##name_(void* c_) {                                          \
        return &static_cast<_Self*>(c_)->name_;                                    \
    }                                                                               \
    inline static bool _prop_##name_ = (_GetPropTable().push_back(                 \
        ::Reflection::PropertyDescriptor{                                            \
            ::Reflection::TypeTagOf<T_>(),                                          \
            yamlKey_,                                                               \
            _acc_##name_,                                                           \
            ::Reflection::PropertyFlags::IsSlider,                                  \
            static_cast<float>(min_), static_cast<float>(max_)                     \
        }), true);

// ─── HH_PROP_ENUM / HH_PROP_NAMED_ENUM ───────────────────────────────────────
// Reflected enum field rendered as an ImGui Combo. The labels array must have
// static storage duration (e.g. a constexpr array declared inside the struct).
// Signature: HH_PROP_NAMED_ENUM(Type, fieldName, "yamlKey", defaultValue, labelsArray, labelCount)

#define HH_PROP_NAMED_ENUM(T_, name_, yamlKey_, def_, labels_, count_)              \
    T_ name_ = def_;                                                                \
    static void* _acc_##name_(void* c_) {                                          \
        return &static_cast<_Self*>(c_)->name_;                                    \
    }                                                                               \
    inline static bool _prop_##name_ = (_GetPropTable().push_back(                 \
        ::Reflection::PropertyDescriptor{                                            \
            ::Reflection::TypeTag::Enum,                                            \
            yamlKey_,                                                               \
            _acc_##name_,                                                           \
            ::Reflection::PropertyFlags::None,                                      \
            0.0f, 0.0f,                                                             \
            nullptr,                                                                \
            labels_, count_                                                         \
        }), true);

#define HH_PROP_ENUM(T_, name_, def_, labels_, count_) \
    HH_PROP_NAMED_ENUM(T_, name_, #name_, def_, labels_, count_)

// ─── HH_END_COMPONENT ────────────────────────────────────────────────────────

#define HH_END_COMPONENT(Name_) };
