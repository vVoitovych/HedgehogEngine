#include "doctest/doctest/doctest.h"

#include "EcsSerialization/api/Reflection/YamlReflection.hpp"

#include "HedgehogEngine/HedgehogEngine/api/ECS/components/CameraComponent.hpp"

#include "yaml-cpp/yaml.h"

#include <cstdint>
#include <string>

namespace
{
    std::string Serialize(HedgehogEngine::CameraComponent& component)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        Reflection::YamlSerializeComponent(out, &component, HedgehogEngine::CameraComponent::GetProperties());
        out << YAML::EndMap;
        return out.c_str();
    }
}

TEST_CASE("A default CameraComponent serialises to the exact block committed in Default.yaml")
{
    // Pins the emitted form so Default.yaml's camera entity — transcribed from this output —
    // cannot silently drift from what the serialiser actually produces.
    HedgehogEngine::CameraComponent defaults;
    const std::string expected =
        "Enabled: true\n"
        "ProjectionType: 0\n"
        "Fov: 60\n"
        "OrthoSize: 10\n"
        "NearPlane: 0.1\n"
        "FarPlane: 1000\n"
        "LayerMask: 4294967295\n"
        "TargetMode: 0\n"
        "TargetName: \"\"\n"
        "GraphName: game\n"
        "Priority: 0";
    CHECK(Serialize(defaults) == expected);
}

TEST_CASE("CameraComponent round-trips TargetIntent (mode + name) and LayerMask through YAML")
{
    using HedgehogEngine::CameraComponent;
    using HedgehogEngine::CameraProjectionType;
    using HedgehogEngine::CameraTargetMode;

    CameraComponent source;
    source.IsEnabled      = false;
    source.ProjectionType = CameraProjectionType::Orthographic;
    source.OrthoSize      = 25.0f;
    source.NearPlane      = 0.5f;
    source.FarPlane       = 500.0f;
    source.LayerMask      = 0x0000A5A5u;
    source.TargetMode     = CameraTargetMode::Texture;
    source.TargetName     = "SecurityCam";
    source.GraphName      = "scene";
    source.Priority       = -3;

    CameraComponent restored;
    Reflection::YamlDeserializeComponent(&restored, YAML::Load(Serialize(source)),
                                         CameraComponent::GetProperties());

    CHECK(restored.IsEnabled      == false);
    CHECK(restored.ProjectionType == CameraProjectionType::Orthographic);
    CHECK(restored.OrthoSize      == doctest::Approx(25.0f));
    CHECK(restored.NearPlane      == doctest::Approx(0.5f));
    CHECK(restored.FarPlane       == doctest::Approx(500.0f));
    CHECK(restored.LayerMask      == 0x0000A5A5u);
    CHECK(restored.TargetMode     == CameraTargetMode::Texture);
    CHECK(restored.TargetName     == "SecurityCam");
    CHECK(restored.GraphName      == "scene");
    CHECK(restored.Priority       == -3);
}

TEST_CASE("A CameraComponent written before layers existed loads its LayerMask as the default")
{
    // Mirrors test_layers.cpp's legacy-load case: an absent key must leave the default
    // standing (every layer visible) rather than producing a garbage mask.
    const YAML::Node legacy = YAML::Load("Enabled: true\nFov: 90");

    HedgehogEngine::CameraComponent restored;
    Reflection::YamlDeserializeComponent(&restored, legacy, HedgehogEngine::CameraComponent::GetProperties());

    CHECK(restored.LayerMask == 0xFFFFFFFFu);
    CHECK(restored.Fov       == doctest::Approx(90.0f));
}
