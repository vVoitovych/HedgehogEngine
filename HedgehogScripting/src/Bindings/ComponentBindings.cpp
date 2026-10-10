#include "Bindings.hpp"
#include "ScriptHandles.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"
#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/AnimationSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/LightSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        using HedgehogEngine::AnimatorComponent;
        using HedgehogEngine::CameraComponent;
        using HedgehogEngine::CameraProjectionType;
        using HedgehogEngine::CameraTargetMode;
        using HedgehogEngine::LightComponent;
        using HedgehogEngine::LightType;
        using HedgehogEngine::MeshComponent;

        constexpr std::string_view ASSETS_PREFIX = "assets://";

        // A read-write field: every access looks the component up again, so a write lands in the
        // ECS at once and the next extraction renders it.
        template<typename T, typename V>
        auto Field(ECS::ECS& ecs, V T::*member)
        {
            return sol::property(
                [&ecs, member](const ScriptComponentRef<T>& ref) { return Resolve(ecs, ref).*member; },
                [&ecs, member](const ScriptComponentRef<T>& ref, const V& value) { Resolve(ecs, ref).*member = value; });
        }

        // An enum field, read and written as its integer value (the matching global table names
        // them); a value outside the enum is a script error.
        template<typename T, typename E>
        auto EnumField(ECS::ECS& ecs, E T::*member, int count, const char* enumName)
        {
            return sol::property(
                [&ecs, member](const ScriptComponentRef<T>& ref) { return static_cast<int>(Resolve(ecs, ref).*member); },
                [&ecs, member, count, enumName](const ScriptComponentRef<T>& ref, int value)
                {
                    if (value < 0 || value >= count)
                        throw std::runtime_error(std::to_string(value) + " is not a " + enumName);
                    Resolve(ecs, ref).*member = static_cast<E>(value);
                });
        }

        void RegisterEnums(sol::state& lua)
        {
            lua.new_enum("LightType",
                         "Directional", static_cast<int>(LightType::DirectionLight),
                         "Point",       static_cast<int>(LightType::PointLight),
                         "Spot",        static_cast<int>(LightType::SpotLight));
            lua.new_enum("CameraProjectionType",
                         "Perspective",  static_cast<int>(CameraProjectionType::Perspective),
                         "Orthographic", static_cast<int>(CameraProjectionType::Orthographic));
            lua.new_enum("CameraTargetMode",
                         "Main",    static_cast<int>(CameraTargetMode::Main),
                         "Texture", static_cast<int>(CameraTargetMode::Texture));
        }

        void RegisterLight(sol::state& lua, ECS::ECS& ecs, HedgehogEngine::LightSystem& lights)
        {
            using LightRef = ScriptComponentRef<LightComponent>;
            lua.new_usertype<LightRef>(
                "Light", sol::no_constructor,
                "enabled",   Field(ecs, &LightComponent::Enable),
                "type",      EnumField(ecs, &LightComponent::LightType, 3, "LightType"),
                "color",     Field(ecs, &LightComponent::Color),
                "intensity", Field(ecs, &LightComponent::Intensity),
                "radius",    Field(ecs, &LightComponent::Radius),
                "coneAngle", Field(ecs, &LightComponent::ConeAngle),
                // One light casts shadows at a time; LightSystem turns the others off, as the
                // inspector's checkbox does.
                "castShadows", sol::property(
                    [&ecs](const LightRef& ref) { return Resolve(ecs, ref).CastShadows; },
                    [&ecs, &lights](const LightRef& ref, bool cast)
                    {
                        (void)Resolve(ecs, ref);
                        lights.SetShadowCasting(ecs, ref.Entity.Id, cast);
                    }),
                sol::meta_function::to_string, ToText<LightComponent>("Light"));
        }

        void RegisterCamera(sol::state& lua, ECS::ECS& ecs)
        {
            using CameraRef = ScriptComponentRef<CameraComponent>;
            lua.new_usertype<CameraRef>(
                "Camera", sol::no_constructor,
                "enabled",    Field(ecs, &CameraComponent::IsEnabled),
                "projection", EnumField(ecs, &CameraComponent::ProjectionType, 2, "CameraProjectionType"),
                "fov",        Field(ecs, &CameraComponent::Fov),
                "orthoSize",  Field(ecs, &CameraComponent::OrthoSize),
                "near",       Field(ecs, &CameraComponent::NearPlane),
                "far",        Field(ecs, &CameraComponent::FarPlane),
                // A mask over the 32 layers, so any value from 0 to 0xFFFFFFFF.
                "layerMask", sol::property(
                    [&ecs](const CameraRef& ref) { return static_cast<int64_t>(Resolve(ecs, ref).LayerMask); },
                    [&ecs](const CameraRef& ref, int64_t mask)
                    {
                        if (mask < 0 || mask > static_cast<int64_t>(UINT32_MAX))
                            throw std::runtime_error(std::to_string(mask) + " is not a 32-bit layer mask");
                        Resolve(ecs, ref).LayerMask = static_cast<uint32_t>(mask);
                    }),
                "targetMode", EnumField(ecs, &CameraComponent::TargetMode, 2, "CameraTargetMode"),
                "targetName", Field(ecs, &CameraComponent::TargetName),
                "graphName",  Field(ecs, &CameraComponent::GraphName),
                "priority",   Field(ecs, &CameraComponent::Priority),
                sol::meta_function::to_string, ToText<CameraComponent>("Camera"));
        }

        void RegisterMesh(sol::state& lua, ECS::ECS& ecs, HedgehogEngine::MeshSystem& meshes,
                          const FS::FileSystemManager& files)
        {
            using MeshRef = ScriptComponentRef<MeshComponent>;
            lua.new_usertype<MeshRef>(
                "Mesh", sol::no_constructor,
                // The path under assets://, as scenes store it. Setting it loads the mesh, as the
                // inspector does; a path with no file behind it is a script error.
                "path", sol::property(
                    [&ecs](const MeshRef& ref) { return Resolve(ecs, ref).MeshPath; },
                    [&ecs, &meshes, &files](const MeshRef& ref, std::string path)
                    {
                        (void)Resolve(ecs, ref);
                        if (path.starts_with(ASSETS_PREFIX))
                            path.erase(0, ASSETS_PREFIX.size());
                        if (path.empty() || !files.Exists(std::string(ASSETS_PREFIX) + path))
                            throw std::runtime_error("no mesh file at " + std::string(ASSETS_PREFIX) + path);
                        meshes.LoadMesh(ecs, ref.Entity.Id, path);
                    }),
                sol::meta_function::to_string, ToText<MeshComponent>("Mesh"));
        }

        // Plays the clips of the entity's skinned mesh through AnimationSystem, so a script and the
        // animator's saved fields drive the same state.
        void RegisterAnimator(sol::state& lua, HedgehogEngine::EngineContext& context)
        {
            using AnimatorRef = ScriptComponentRef<AnimatorComponent>;
            ECS::ECS&                        ecs        = context.GetECS();
            HedgehogEngine::AnimationSystem& animations = *context.GetAnimationSystem();
            lua.new_usertype<AnimatorRef>(
                "Animator", sol::no_constructor,
                // play(clip, fade?, loop?): from the clip's start, crossfading over fade seconds
                // (CrossfadeTime when left out), looping as loop says (the animator's loop when
                // left out). An unknown clip logs a warning naming it and keeps the current one;
                // returns whether the clip was found.
                "play", [&ecs, &animations, &context](const AnimatorRef& ref, const std::string& clip,
                                                      std::optional<float> fade, std::optional<bool> loop)
                {
                    (void)Resolve(ecs, ref);
                    if (fade && (!std::isfinite(*fade) || *fade < 0.0f))
                        throw std::runtime_error("a crossfade time must be a finite number of seconds, not below 0");
                    return animations.Play(ecs, context.GetResourceCatalog().GetMeshContainer(), ref.Entity.Id, clip, fade,
                                           loop);
                },
                "stop", [&ecs, &animations](const AnimatorRef& ref)
                {
                    (void)Resolve(ecs, ref);
                    animations.Stop(ecs, ref.Entity.Id);
                },
                // False once a non-looping clip has reached its end.
                "isPlaying", [&ecs](const AnimatorRef& ref)
                {
                    const AnimatorComponent& animator = Resolve(ecs, ref);
                    return animator.Playing && !animator.Finished;
                },
                "getClipNames", [&ecs, &animations, &context](const AnimatorRef& ref)
                {
                    (void)Resolve(ecs, ref);
                    return sol::as_table(animations.GetClipNames(ecs, context.GetResourceCatalog().GetMeshContainer(),
                                                                 ref.Entity.Id));
                },
                // The clip playing now, or nil.
                "clip", sol::readonly_property([&ecs](const AnimatorRef& ref, sol::this_state state) -> sol::object
                {
                    const AnimatorComponent& animator = Resolve(ecs, ref);
                    if (!animator.Playing || animator.CurrentClip.empty())
                        return sol::lua_nil;
                    return sol::make_object(state, animator.CurrentClip);
                }),
                "speed", Field(ecs, &AnimatorComponent::Speed),
                "loop",  Field(ecs, &AnimatorComponent::Loop),
                // Seconds into the current clip. Setting it lets a finished clip finish again.
                "time", sol::property(
                    [&ecs](const AnimatorRef& ref) { return HedgehogEngine::AnimationSystem::GetTime(Resolve(ecs, ref)); },
                    [&ecs](const AnimatorRef& ref, float time)
                    {
                        if (!std::isfinite(time))
                            throw std::runtime_error("an animation time must be a finite number");
                        HedgehogEngine::AnimationSystem::SetTime(Resolve(ecs, ref), time);
                    }),
                sol::meta_function::to_string, ToText<AnimatorComponent>("Animator"));
        }
    }

    void RegisterComponents(sol::state& lua, HedgehogEngine::EngineContext& context)
    {
        RegisterEnums(lua);
        RegisterLight(lua, context.GetECS(), *context.GetLightSystem());
        RegisterCamera(lua, context.GetECS());
        RegisterMesh(lua, context.GetECS(), *context.GetMeshSystem(), context.GetFileSystem());
        RegisterAnimator(lua, context);
    }
}
