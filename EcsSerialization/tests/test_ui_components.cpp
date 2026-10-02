#include "doctest/doctest/doctest.h"

#include "EcsSerialization/api/Reflection/YamlReflection.hpp"

#include "HedgehogEngine/HedgehogEngine/api/ECS/components/UiButtonComponent.hpp"
#include "HedgehogEngine/HedgehogEngine/api/ECS/components/UiCanvasComponent.hpp"
#include "HedgehogEngine/HedgehogEngine/api/ECS/components/UiImageComponent.hpp"
#include "HedgehogEngine/HedgehogEngine/api/ECS/components/UiRectComponent.hpp"
#include "HedgehogEngine/HedgehogEngine/api/ECS/components/UiTextComponent.hpp"

#include "yaml-cpp/yaml.h"

#include <string>

using namespace HedgehogEngine;

namespace
{
    template<typename T>
    std::string Serialize(T& component)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        Reflection::YamlSerializeComponent(out, &component, T::GetProperties());
        out << YAML::EndMap;
        return out.c_str();
    }

    // Writes source, reads it into a default component, and checks the copy writes the same text.
    template<typename T>
    T RoundTrip(T& source)
    {
        const std::string yaml = Serialize(source);
        T                 restored;
        Reflection::YamlDeserializeComponent(&restored, YAML::Load(yaml), T::GetProperties());
        CHECK(Serialize(restored) == yaml);
        return restored;
    }

    bool Equal(const HM::Vector2& a, const HM::Vector2& b) { return a.x() == b.x() && a.y() == b.y(); }
    bool Equal(const HM::Vector4& a, const HM::Vector4& b)
    {
        return a.x() == b.x() && a.y() == b.y() && a.z() == b.z() && a.w() == b.w();
    }
}

TEST_CASE("UiCanvasComponent round-trips its scaling and sort order")
{
    UiCanvasComponent source;
    source.IsEnabled           = false;
    source.ScaleMode           = UiCanvasScaleMode::ConstantPixelSize;
    source.ReferenceResolution = HM::Vector2(1280.0f, 720.0f);
    source.MatchWidthOrHeight  = 0.25f;
    source.SortOrder           = -3;

    const UiCanvasComponent restored = RoundTrip(source);
    CHECK_FALSE(restored.IsEnabled);
    CHECK(restored.ScaleMode == UiCanvasScaleMode::ConstantPixelSize);
    CHECK(Equal(restored.ReferenceResolution, HM::Vector2(1280.0f, 720.0f)));
    CHECK(restored.MatchWidthOrHeight == 0.25f);
    CHECK(restored.SortOrder == -3);
}

TEST_CASE("UiRectComponent round-trips its anchors, pivot, offset, size and visibility")
{
    UiRectComponent source;
    source.IsVisible = false;
    source.AnchorMin = HM::Vector2(0.0f, 1.0f);
    source.AnchorMax = HM::Vector2(1.0f, 1.0f);
    source.Pivot     = HM::Vector2(0.5f, 1.0f);
    source.Offset    = HM::Vector2(4.0f, -8.0f);
    source.Size      = HM::Vector2(-20.0f, 48.0f);

    const UiRectComponent restored = RoundTrip(source);
    CHECK_FALSE(restored.IsVisible);
    CHECK(Equal(restored.AnchorMin, source.AnchorMin));
    CHECK(Equal(restored.AnchorMax, source.AnchorMax));
    CHECK(Equal(restored.Pivot, source.Pivot));
    CHECK(Equal(restored.Offset, source.Offset));
    CHECK(Equal(restored.Size, source.Size));
}

TEST_CASE("UiImageComponent round-trips its texture and colour")
{
    UiImageComponent source;
    source.Texture = "Textures/hud.png";
    source.Color   = HM::Vector4(0.25f, 0.5f, 0.75f, 0.5f);

    const UiImageComponent restored = RoundTrip(source);
    CHECK(restored.Texture == "Textures/hud.png");
    CHECK(Equal(restored.Color, source.Color));
}

TEST_CASE("UiTextComponent round-trips its text, font, size, colour, alignment and wrap")
{
    UiTextComponent source;
    source.Text          = "Score: 10\nLives: 3";
    source.Font          = "Fonts/Karla-Regular.ttf";
    source.FontSize      = 32.0f;
    source.Color         = HM::Vector4(1.0f, 0.5f, 0.0f, 1.0f);
    source.Align         = UiTextAlign::Right;
    source.VerticalAlign = UiTextVerticalAlign::Bottom;
    source.Wrap          = false;

    const UiTextComponent restored = RoundTrip(source);
    CHECK(restored.Text == source.Text);
    CHECK(restored.Font == source.Font);
    CHECK(restored.FontSize == 32.0f);
    CHECK(Equal(restored.Color, source.Color));
    CHECK(restored.Align == UiTextAlign::Right);
    CHECK(restored.VerticalAlign == UiTextVerticalAlign::Bottom);
    CHECK_FALSE(restored.Wrap);
}

TEST_CASE("UiButtonComponent round-trips its tints and interactability, never its runtime state")
{
    UiButtonComponent source;
    source.IsInteractable = false;
    source.NormalTint     = HM::Vector4(1.0f, 0.0f, 0.0f, 1.0f);
    source.HoverTint      = HM::Vector4(0.0f, 1.0f, 0.0f, 1.0f);
    source.PressedTint    = HM::Vector4(0.0f, 0.0f, 1.0f, 1.0f);
    source.DisabledTint   = HM::Vector4(0.1f, 0.2f, 0.3f, 0.4f);
    source.State          = UiButtonState::Pressed;

    const std::string yaml = Serialize(source);
    CHECK(yaml.find("State") == std::string::npos);

    const UiButtonComponent restored = RoundTrip(source);
    CHECK_FALSE(restored.IsInteractable);
    CHECK(Equal(restored.NormalTint, source.NormalTint));
    CHECK(Equal(restored.HoverTint, source.HoverTint));
    CHECK(Equal(restored.PressedTint, source.PressedTint));
    CHECK(Equal(restored.DisabledTint, source.DisabledTint));
    CHECK(restored.State == UiButtonState::Normal);
}
