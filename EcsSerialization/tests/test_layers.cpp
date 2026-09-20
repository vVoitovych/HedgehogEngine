#include "doctest/doctest/doctest.h"

#include "EcsSerialization/api/Reflection/YamlReflection.hpp"

#include "HedgehogEngine/HedgehogEngine/api/Reflection/ComponentMacros.hpp"
#include "HedgehogEngine/HedgehogSettings/api/LayerSettings.hpp"

#include "yaml-cpp/yaml.h"

#include <cstdint>
#include <string>

namespace
{
    // Mirrors RenderComponent's shape. What is under test is the reflected uint32 property —
    // its round-trip, and its default when a scene predates the key — not RenderComponent
    // itself, which lives in the HedgehogEngine DLL and would pull the whole engine into this
    // test binary for no extra coverage.
HH_BEGIN_COMPONENT(LayeredTestComponent)
    HH_PROP_NAMED(bool,     IsVisible, "Visible", true, None)
    HH_PROP_NAMED(uint32_t, Layer,     "Layer",   0u,   None)
HH_END_COMPONENT(LayeredTestComponent)

    std::string Serialize(LayeredTestComponent& component)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        Reflection::YamlSerializeComponent(out, &component, LayeredTestComponent::GetProperties());
        out << YAML::EndMap;
        return out.c_str();
    }
}

TEST_CASE("A reflected layer index round-trips through YAML")
{
    LayeredTestComponent source;
    source.Layer     = 7;
    source.IsVisible = false;

    LayeredTestComponent restored;
    Reflection::YamlDeserializeComponent(&restored, YAML::Load(Serialize(source)),
                                         LayeredTestComponent::GetProperties());

    CHECK(restored.Layer == 7u);
    CHECK(restored.IsVisible == false);
}

TEST_CASE("A component written before layers existed loads as layer 0")
{
    // The registry default-constructs before deserialising, so an absent key must leave the
    // default standing rather than producing a garbage index.
    const YAML::Node legacy = YAML::Load("Visible: false");

    LayeredTestComponent restored;
    Reflection::YamlDeserializeComponent(&restored, legacy, LayeredTestComponent::GetProperties());

    CHECK(restored.Layer == 0u);
    CHECK(restored.IsVisible == false);
}

TEST_CASE("Renaming a layer cannot re-bucket objects")
{
    HedgehogSettings::LayerSettings layers;

    LayeredTestComponent component;
    component.Layer = 7;

    const std::string before = Serialize(component);

    layers.SetLayerName(7, "Editor Gizmos");
    REQUIRE(layers.GetLayerDisplayName(7) == "Editor Gizmos");

    const std::string after = Serialize(component);

    CHECK(component.Layer == 7u);
    CHECK(before == after);
}

TEST_CASE("LayerSettings display names")
{
    HedgehogSettings::LayerSettings layers;

    SUBCASE("layer 0 is Default and cannot be blanked")
    {
        CHECK(layers.GetLayerDisplayName(0) == "Default");

        layers.SetLayerName(0, "");
        CHECK(layers.GetLayerDisplayName(0) == "Default");

        layers.SetLayerName(0, "Everything");
        CHECK(layers.GetLayerDisplayName(0) == "Everything");
    }

    SUBCASE("an unnamed slot stays selectable under a generated label")
    {
        CHECK(layers.GetLayerName(12).empty());
        CHECK(layers.GetLayerDisplayName(12) == "Layer 12");
    }

    SUBCASE("clearing a non-zero layer reverts it to the generated label")
    {
        layers.SetLayerName(3, "Water");
        REQUIRE(layers.GetLayerDisplayName(3) == "Water");

        layers.SetLayerName(3, "");
        CHECK(layers.GetLayerDisplayName(3) == "Layer 3");
    }

    SUBCASE("renaming marks the settings dirty until cleaned")
    {
        CHECK_FALSE(layers.IsDirty());

        layers.SetLayerName(5, "UI");
        CHECK(layers.IsDirty());

        layers.CleanDirtyState();
        CHECK_FALSE(layers.IsDirty());
    }

    SUBCASE("setting the same name again is not a change")
    {
        layers.SetLayerName(5, "UI");
        layers.CleanDirtyState();

        layers.SetLayerName(5, "UI");
        CHECK_FALSE(layers.IsDirty());
    }
}
