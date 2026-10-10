#include "doctest/doctest/doctest.h"

#include "EcsSerialization/api/Reflection/YamlReflection.hpp"

#include "HedgehogEngine/HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"

#include "yaml-cpp/yaml.h"

#include <string>

using HedgehogEngine::AnimatorComponent;

namespace
{
    std::string Serialize(AnimatorComponent& component)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        Reflection::YamlSerializeComponent(out, &component, AnimatorComponent::GetProperties());
        out << YAML::EndMap;
        return out.c_str();
    }
}

TEST_CASE("A default AnimatorComponent serialises to exactly its five reflected fields")
{
    AnimatorComponent defaults;
    const std::string expected =
        "Clip: \"\"\n"
        "Speed: 1\n"
        "Loop: true\n"
        "PlayOnStart: true\n"
        "CrossfadeTime: 0.2";
    CHECK(Serialize(defaults) == expected);
}

TEST_CASE("AnimatorComponent round-trips its reflected fields and never writes its runtime state")
{
    AnimatorComponent source;
    source.Clip          = "Walk";
    source.Speed         = 1.5f;
    source.Loop          = false;
    source.PlayOnStart   = false;
    source.CrossfadeTime = 0.35f;
    // Runtime state, set to values a save must not carry.
    source.Started      = true;
    source.Playing      = true;
    source.CurrentClip  = "Run";
    source.WarnedClip   = "Idle";
    source.Blend.Count  = 2;
    source.Blend.Entries[1].Time = 3.0f;
    source.Blend.FadeDuration    = 0.25f;
    source.PreviewTime  = 0.5f;
    source.Palette.assign(4, HM::Matrix4x4::GetTranslation(1.0f, 2.0f, 3.0f));

    const std::string yaml = Serialize(source);
    CHECK(yaml.find("Run") == std::string::npos);
    CHECK(yaml.find("Idle") == std::string::npos);
    CHECK(yaml.find("Palette") == std::string::npos);

    AnimatorComponent restored;
    Reflection::YamlDeserializeComponent(&restored, YAML::Load(yaml), AnimatorComponent::GetProperties());
    CHECK(restored.Clip == "Walk");
    CHECK(restored.Speed == doctest::Approx(1.5f));
    CHECK(restored.Loop == false);
    CHECK(restored.PlayOnStart == false);
    CHECK(restored.CrossfadeTime == doctest::Approx(0.35f));
    CHECK_FALSE(restored.Started);
    CHECK_FALSE(restored.Playing);
    CHECK(restored.CurrentClip.empty());
    CHECK(restored.Blend.Count == 0u);
    CHECK_FALSE(restored.PreviewTime.has_value());
    CHECK(restored.Palette.empty());
}
