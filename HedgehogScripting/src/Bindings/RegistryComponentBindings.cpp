#include "Bindings.hpp"
#include "ScriptHandles.hpp"

#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"

#include "EcsSerialization/api/ComponentTypeRegistry.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        using EcsSerialization::ComponentInfo;
        using Reflection::PropertyDescriptor;
        using Reflection::TypeTag;

        // A component reached by its registry key, as plugins' components are: the entity's handle
        // and the key only. Every access looks the type, the component and the property up again,
        // so nothing of a plugin's (which may unload) is ever kept.
        struct RegistryComponentRef
        {
            ScriptEntity Entity;
            std::string  Key;
        };

        // Engine components with their own bindings: getComponent and addComponent hand those out
        // (entity:get<Name>() / add<Name>(); the transform's handle for TransformComponent), so
        // their side effects (shadow casting, mesh loading, transform events) still happen.
        struct HandWritten
        {
            const char* Key;
            const char* Name;
        };
        constexpr HandWritten HAND_WRITTEN[] = {
            { "LightComponent", "Light" },         { "CameraComponent", "Camera" },     { "MeshComponent", "Mesh" },
            { "AnimatorComponent", "Animator" },   { "AudioSourceComponent", "AudioSource" },
            { "UiRectComponent", "UiRect" },       { "UiImageComponent", "UiImage" },   { "UiTextComponent", "UiText" },
            { "UiButtonComponent", "UiButton" },
        };

        const char* FindHandWritten(const std::string& key)
        {
            for (const HandWritten& entry : HAND_WRITTEN)
                if (key == entry.Key)
                    return entry.Name;
            return nullptr;
        }

        const ComponentInfo& FindInfo(HedgehogEngine::EngineContext& context, const std::string& key)
        {
            const ComponentInfo* info = context.GetComponentTypes().Find(key);
            if (info == nullptr)
                throw std::runtime_error("component type '" + key + "' is not registered (is its plugin loaded?)");
            return *info;
        }

        // The component's storage and its type, looked up now.
        std::pair<void*, const ComponentInfo*> Resolve(HedgehogEngine::EngineContext& context, const RegistryComponentRef& ref)
        {
            ECS::ECS& ecs = context.GetECS();
            RequireValid(ecs, ref.Entity);
            const ComponentInfo& info = FindInfo(context, ref.Key);
            if (!info.Has(ecs, ref.Entity.Id))
                throw std::runtime_error(Describe(ref.Entity) + " has no " + ref.Key);
            return { info.Get(ecs, ref.Entity.Id), &info };
        }

        const PropertyDescriptor& FindProperty(const ComponentInfo& info, const std::string& name)
        {
            for (const PropertyDescriptor& property : info.Properties)
                if (name == property.name)
                    return property;
            throw std::runtime_error(info.Key + " has no property '" + name + "'");
        }

        sol::object Read(sol::this_state state, ECS::ECS& ecs, const ComponentInfo& info, const PropertyDescriptor& property, void* component)
        {
            void* field = property.accessor(component);
            switch (property.type)
            {
            case TypeTag::Bool:   return sol::make_object(state, *static_cast<bool*>(field));
            case TypeTag::Int:    return sol::make_object(state, *static_cast<int32_t*>(field));
            case TypeTag::UInt:   return sol::make_object(state, *static_cast<uint32_t*>(field));
            case TypeTag::Float:  return sol::make_object(state, *static_cast<float*>(field));
            case TypeTag::Double: return sol::make_object(state, *static_cast<double*>(field));
            case TypeTag::String: return sol::make_object(state, *static_cast<std::string*>(field));
            case TypeTag::Vec3:   return sol::make_object(state, *static_cast<HM::Vector3*>(field));
            case TypeTag::Enum:
            {
                int32_t value = 0; // stored as the serializer reads it
                std::memcpy(&value, field, sizeof(value));
                return sol::make_object(state, value);
            }
            case TypeTag::Vec2:
            {
                const HM::Vector2& v = *static_cast<HM::Vector2*>(field);
                return sol::make_object(state, sol::state_view(state).create_table_with("x", v.x(), "y", v.y()));
            }
            case TypeTag::Vec4:
            {
                const HM::Vector4& v = *static_cast<HM::Vector4*>(field);
                return sol::make_object(state, sol::state_view(state).create_table_with("x", v.x(), "y", v.y(), "z", v.z(), "w", v.w()));
            }
            case TypeTag::Entity:
            {
                const ECS::Entity entity = *static_cast<ECS::Entity*>(field);
                if (entity == ECS::INVALID_ENTITY || !ecs.IsAlive(entity))
                    return sol::lua_nil;
                return sol::make_object(state, MakeScriptEntity(ecs, entity));
            }
            case TypeTag::Raw:
                break;
            }
            throw std::runtime_error(info.Key + "." + property.name + " cannot be read from scripts");
        }

        [[noreturn]] void WrongType(const ComponentInfo& info, const PropertyDescriptor& property, const char* expected, const sol::object& value)
        {
            throw std::runtime_error(info.Key + "." + property.name + " takes " + expected + ", not " +
                                     sol::type_name(value.lua_state(), value.get_type()));
        }

        // A whole number in [low, high], or a script error naming the property.
        double RequireWhole(const ComponentInfo& info, const PropertyDescriptor& property, const sol::object& value, double low, double high)
        {
            if (value.get_type() != sol::type::number)
                WrongType(info, property, "an integer", value);
            const double number = value.as<double>();
            if (!std::isfinite(number) || std::floor(number) != number || number < low || number > high)
                throw std::runtime_error(info.Key + "." + property.name + " takes an integer from " + std::to_string(static_cast<int64_t>(low)) +
                                         " to " + std::to_string(static_cast<int64_t>(high)) + ", not " + std::to_string(number));
            return number;
        }

        void Write(ECS::ECS& ecs, const ComponentInfo& info, const PropertyDescriptor& property, void* component, const sol::object& value)
        {
            void* field = property.accessor(component);
            switch (property.type)
            {
            case TypeTag::Bool:
                if (value.get_type() != sol::type::boolean)
                    WrongType(info, property, "a boolean", value);
                *static_cast<bool*>(field) = value.as<bool>();
                return;
            case TypeTag::Int:
                *static_cast<int32_t*>(field) = static_cast<int32_t>(RequireWhole(
                    info, property, value, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()));
                return;
            case TypeTag::UInt:
                *static_cast<uint32_t*>(field) =
                    static_cast<uint32_t>(RequireWhole(info, property, value, 0.0, std::numeric_limits<uint32_t>::max()));
                return;
            case TypeTag::Float:
            case TypeTag::Double:
            {
                if (value.get_type() != sol::type::number)
                    WrongType(info, property, "a number", value);
                const double number = value.as<double>();
                if (property.type == TypeTag::Float)
                    *static_cast<float*>(field) = static_cast<float>(number);
                else
                    *static_cast<double*>(field) = number;
                return;
            }
            case TypeTag::String:
                if (value.get_type() != sol::type::string)
                    WrongType(info, property, "a string", value);
                *static_cast<std::string*>(field) = value.as<std::string>();
                return;
            case TypeTag::Vec3:
                if (!value.is<HM::Vector3>())
                    WrongType(info, property, "a Vector3", value);
                *static_cast<HM::Vector3*>(field) = value.as<HM::Vector3>();
                return;
            case TypeTag::Entity:
                if (value.get_type() == sol::type::lua_nil)
                {
                    *static_cast<ECS::Entity*>(field) = ECS::INVALID_ENTITY;
                    return;
                }
                if (!value.is<ScriptEntity>())
                    WrongType(info, property, "an Entity or nil", value);
                {
                    const ScriptEntity handle         = value.as<ScriptEntity>();
                    *static_cast<ECS::Entity*>(field) = IsValid(ecs, handle) ? handle.Id : ECS::INVALID_ENTITY;
                }
                return;
            case TypeTag::Enum:
            case TypeTag::Vec2:
            case TypeTag::Vec4:
            case TypeTag::Raw:
                break;
            }
            throw std::runtime_error(info.Key + "." + property.name + " is read-only for scripts");
        }

        // entity:get<Name>() or add<Name>() of a hand-written binding.
        sol::object CallEntityMethod(sol::this_state state, const ScriptEntity& entity, const std::string& method)
        {
            sol::state_view        lua(state);
            sol::protected_function function = lua["Entity"][method];
            sol::protected_function_result result = function(entity);
            if (!result.valid())
            {
                const sol::error error = result;
                throw std::runtime_error(error.what());
            }
            return result.get<sol::object>();
        }
    }

    void RegisterRegistryComponents(sol::state& lua, HedgehogEngine::EngineContext& context)
    {
        ECS::ECS& ecs = context.GetECS();

        lua.new_usertype<RegistryComponentRef>(
            "RegistryComponent", sol::no_constructor,
            // Every name is a reflected property of the component, looked up by name on each access.
            sol::meta_function::index,
            [&context, &ecs](const RegistryComponentRef& ref, const std::string& name, sol::this_state state)
            {
                const auto [component, info] = Resolve(context, ref);
                return Read(state, ecs, *info, FindProperty(*info, name), component);
            },
            sol::meta_function::new_index,
            [&context, &ecs](const RegistryComponentRef& ref, const std::string& name, const sol::object& value)
            {
                const auto [component, info] = Resolve(context, ref);
                Write(ecs, *info, FindProperty(*info, name), component, value);
            },
            sol::meta_function::to_string,
            [](const RegistryComponentRef& ref) { return ref.Key + " of " + Describe(ref.Entity); });

        sol::usertype<ScriptEntity> entity = lua["Entity"];

        // nil when the entity has none. An engine component with its own binding gives that.
        entity["getComponent"] = [&context, &ecs](const ScriptEntity& e, const std::string& key, sol::this_state state) -> sol::object
        {
            RequireValid(ecs, e);
            if (key == "TransformComponent")
            {
                if (!ecs.HasComponent<HedgehogEngine::TransformComponent>(e.Id))
                    return sol::lua_nil;
                return sol::make_object(state, ScriptComponentRef<HedgehogEngine::TransformComponent>{ e });
            }
            if (const char* name = FindHandWritten(key))
                return CallEntityMethod(state, e, std::string("get") + name);
            const ComponentInfo& info = FindInfo(context, key);
            if (!info.Has(ecs, e.Id))
                return sol::lua_nil;
            return sol::make_object(state, RegistryComponentRef{ e, key });
        };
        entity["hasComponent"] = [&context, &ecs](const ScriptEntity& e, const std::string& key)
        {
            RequireValid(ecs, e);
            return FindInfo(context, key).Has(ecs, e.Id);
        };
        // Adds the component as the editor's Add Component menu does (its AddDefault); an existing
        // one is returned as is.
        entity["addComponent"] = [&context, &ecs](const ScriptEntity& e, const std::string& key, sol::this_state state) -> sol::object
        {
            RequireValid(ecs, e);
            if (const char* name = FindHandWritten(key))
                return CallEntityMethod(state, e, std::string("add") + name);
            const ComponentInfo& info = FindInfo(context, key);
            if (!info.Has(ecs, e.Id))
            {
                if (!info.Addable || !info.AddDefault)
                    throw std::runtime_error(key + " cannot be added by scripts");
                info.AddDefault(ecs, e.Id);
            }
            return sol::make_object(state, RegistryComponentRef{ e, key });
        };
    }
}
