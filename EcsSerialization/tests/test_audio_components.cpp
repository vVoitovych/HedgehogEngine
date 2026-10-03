#include "doctest/doctest/doctest.h"

#include "EcsSerialization/api/Reflection/YamlReflection.hpp"

#include "HedgehogEngine/HedgehogEngine/api/ECS/components/AudioListenerComponent.hpp"
#include "HedgehogEngine/HedgehogEngine/api/ECS/components/AudioSourceComponent.hpp"

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

    template<typename T>
    T RoundTrip(T& source)
    {
        const std::string yaml = Serialize(source);
        T                 restored;
        Reflection::YamlDeserializeComponent(&restored, YAML::Load(yaml), T::GetProperties());
        CHECK(Serialize(restored) == yaml);
        return restored;
    }
}

TEST_CASE("AudioSourceComponent round-trips every reflected field and never writes its sound")
{
    AudioSourceComponent source;
    source.Clip        = "Audio/Tone440.wav";
    source.Volume      = 0.5f;
    source.Pitch       = 1.25f;
    source.Loop        = true;
    source.PlayOnStart = false;
    source.Spatial     = false;
    source.MinDistance = 2.0f;
    source.MaxDistance = 40.0f;
    source.Sound       = HA::SoundHandle{ 3, 7 };

    const std::string yaml = Serialize(source);
    CHECK(yaml.find("Sound") == std::string::npos);
    CHECK(yaml.find("Clip: Audio/Tone440.wav") != std::string::npos);

    const AudioSourceComponent restored = RoundTrip(source);
    CHECK(restored.Clip == "Audio/Tone440.wav");
    CHECK(restored.Volume == 0.5f);
    CHECK(restored.Pitch == 1.25f);
    CHECK(restored.Loop);
    CHECK_FALSE(restored.PlayOnStart);
    CHECK_FALSE(restored.Spatial);
    CHECK(restored.MinDistance == 2.0f);
    CHECK(restored.MaxDistance == 40.0f);
    CHECK_FALSE(restored.Sound.IsValid());
}

TEST_CASE("AudioListenerComponent round-trips Active")
{
    AudioListenerComponent listener;
    listener.IsActive = false;
    const std::string yaml = Serialize(listener);
    CHECK(yaml.find("Active: false") != std::string::npos);
    CHECK_FALSE(RoundTrip(listener).IsActive);
}
