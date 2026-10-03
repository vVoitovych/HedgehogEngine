#include "Bindings.hpp"
#include "ScriptHandles.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/UiButtonComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiImageComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiRectComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiTextComponent.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        using HedgehogEngine::UiButtonComponent;
        using HedgehogEngine::UiImageComponent;
        using HedgehogEngine::UiRectComponent;
        using HedgehogEngine::UiTextComponent;

        float RequireFinite(float value, const char* what)
        {
            if (!std::isfinite(value))
                throw std::runtime_error(std::string(what) + " must be a finite number");
            return value;
        }

        // A read-write field, looked up again on every access, as ComponentBindings' are.
        template<typename T, typename V>
        auto Field(ECS::ECS& ecs, V T::*member)
        {
            return sol::property(
                [&ecs, member](const ScriptComponentRef<T>& ref) { return Resolve(ecs, ref).*member; },
                [&ecs, member](const ScriptComponentRef<T>& ref, const V& value) { Resolve(ecs, ref).*member = value; });
        }

        // One float of a vector member (index 0 is x), so scripts set a size or offset by its parts.
        template<typename T, typename V>
        auto VectorPart(ECS::ECS& ecs, V T::*member, size_t index, const char* what)
        {
            return sol::property(
                [&ecs, member, index](const ScriptComponentRef<T>& ref) { return (Resolve(ecs, ref).*member)[index]; },
                [&ecs, member, index, what](const ScriptComponentRef<T>& ref, float value)
                { (Resolve(ecs, ref).*member)[index] = RequireFinite(value, what); });
        }

        // An RGBA colour as scripts see colours elsewhere: color is the RGB part as a Vector3 (a
        // write keeps the alpha), alpha the fourth channel.
        template<typename T>
        auto ColorRgb(ECS::ECS& ecs, HM::Vector4 T::*member)
        {
            return sol::property(
                [&ecs, member](const ScriptComponentRef<T>& ref)
                {
                    const HM::Vector4& color = Resolve(ecs, ref).*member;
                    return HM::Vector3(color.x(), color.y(), color.z());
                },
                [&ecs, member](const ScriptComponentRef<T>& ref, const HM::Vector3& rgb)
                {
                    HM::Vector4& color = Resolve(ecs, ref).*member;
                    color = HM::Vector4(rgb.x(), rgb.y(), rgb.z(), color.w());
                });
        }

        template<typename T>
        auto Alpha(ECS::ECS& ecs, HM::Vector4 T::*member)
        {
            return sol::property(
                [&ecs, member](const ScriptComponentRef<T>& ref) { return (Resolve(ecs, ref).*member).w(); },
                [&ecs, member](const ScriptComponentRef<T>& ref, float alpha)
                { (Resolve(ecs, ref).*member).w() = RequireFinite(alpha, "an alpha"); });
        }

        void RegisterRect(sol::state& lua, ECS::ECS& ecs)
        {
            using UiRectRef = ScriptComponentRef<UiRectComponent>;
            lua.new_usertype<UiRectRef>(
                "UiRect", sol::no_constructor,
                // A hidden element draws nothing and takes no input, and neither do its children.
                "visible", Field(ecs, &UiRectComponent::IsVisible),
                "offsetX", VectorPart(ecs, &UiRectComponent::Offset, 0, "an offset"),
                "offsetY", VectorPart(ecs, &UiRectComponent::Offset, 1, "an offset"),
                "width",   VectorPart(ecs, &UiRectComponent::Size, 0, "a size"),
                "height",  VectorPart(ecs, &UiRectComponent::Size, 1, "a size"),
                sol::meta_function::to_string, ToText<UiRectComponent>("UiRect"));
        }

        void RegisterImage(sol::state& lua, ECS::ECS& ecs)
        {
            using UiImageRef = ScriptComponentRef<UiImageComponent>;
            lua.new_usertype<UiImageRef>(
                "UiImage", sol::no_constructor,
                // The path under assets://, prefix optional; an empty string fills with the colour alone.
                "texture", sol::property(
                    [&ecs](const UiImageRef& ref) { return Resolve(ecs, ref).Texture; },
                    [&ecs](const UiImageRef& ref, const std::string& path)
                    { Resolve(ecs, ref).Texture = path.empty() ? std::string() : ToAssetPath(path, "a texture", "textures"); }),
                "color", ColorRgb(ecs, &UiImageComponent::Color),
                "alpha", Alpha(ecs, &UiImageComponent::Color),
                sol::meta_function::to_string, ToText<UiImageComponent>("UiImage"));
        }

        void RegisterText(sol::state& lua, ECS::ECS& ecs)
        {
            using UiTextRef = ScriptComponentRef<UiTextComponent>;
            lua.new_usertype<UiTextRef>(
                "UiText", sol::no_constructor,
                "text", Field(ecs, &UiTextComponent::Text),
                // A .ttf or .otf under assets://, prefix optional.
                "font", sol::property(
                    [&ecs](const UiTextRef& ref) { return Resolve(ecs, ref).Font; },
                    [&ecs](const UiTextRef& ref, const std::string& path)
                    { Resolve(ecs, ref).Font = ToAssetPath(path, "a font", "fonts"); }),
                "fontSize", sol::property(
                    [&ecs](const UiTextRef& ref) { return Resolve(ecs, ref).FontSize; },
                    [&ecs](const UiTextRef& ref, float size)
                    {
                        if (!(RequireFinite(size, "a font size") > 0.0f))
                            throw std::runtime_error("a font size must be above 0");
                        Resolve(ecs, ref).FontSize = size;
                    }),
                "color", ColorRgb(ecs, &UiTextComponent::Color),
                "alpha", Alpha(ecs, &UiTextComponent::Color),
                sol::meta_function::to_string, ToText<UiTextComponent>("UiText"));
        }

        void RegisterButton(sol::state& lua, ECS::ECS& ecs, ClickSubscriber subscribeClick)
        {
            using UiButtonRef = ScriptComponentRef<UiButtonComponent>;
            lua.new_usertype<UiButtonRef>(
                "UiButton", sol::no_constructor,
                "interactable", Field(ecs, &UiButtonComponent::IsInteractable),
                // Read only: set by the engine's UI input in Play mode.
                "isHovered", [&ecs](const UiButtonRef& ref)
                { return Resolve(ecs, ref).State != HedgehogEngine::UiButtonState::Normal; },
                "isPressed", [&ecs](const UiButtonRef& ref)
                { return Resolve(ecs, ref).State == HedgehogEngine::UiButtonState::Pressed; },
                // onClick(fn): fn(event) runs once per click of this button, after every script's
                // OnUpdate, with event.entity the button. Returns the id Events.unsubscribe takes;
                // the subscription goes with the calling script.
                "onClick", [&ecs, subscribeClick = std::move(subscribeClick)](const UiButtonRef& ref,
                                                                              sol::protected_function handler)
                {
                    (void)Resolve(ecs, ref);
                    return subscribeClick(ref.Entity, std::move(handler));
                },
                sol::meta_function::to_string, ToText<UiButtonComponent>("UiButton"));
        }
    }

    void RegisterUi(sol::state& lua, HedgehogEngine::EngineContext& context, ClickSubscriber subscribeClick)
    {
        ECS::ECS& ecs = context.GetECS();
        RegisterRect(lua, ecs);
        RegisterImage(lua, ecs);
        RegisterText(lua, ecs);
        RegisterButton(lua, ecs, std::move(subscribeClick));
    }
}
