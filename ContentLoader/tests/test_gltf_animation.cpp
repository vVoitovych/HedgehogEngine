#include "doctest/doctest/doctest.h"

#include "ContentLoader/api/AnimationLoader.hpp"
#include "ContentLoader/api/MeshLoader.hpp"

#include "FileSystem/tests/test_helpers.hpp"
#include "HedgehogScripting/tests/test_log_capture.hpp"

#include "test_gltf_builder.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using ContentLoader::AnimationInterpolation;
using ContentLoader::LoadedAnimationClip;
using namespace GltfTest;

namespace
{
    constexpr int SHORT = 5122;

    template<typename T>
    int AddData(GltfBuilder& builder, const std::vector<T>& values, int componentType, size_t count,
                const std::string& type, bool normalized = false)
    {
        return builder.AddAccessor(builder.AddView(Bytes(values)), componentType, count, type, normalized);
    }

    // A skinned triangle (node 0) under a non-joint Armature (node 1) whose joints are Hip (node 2)
    // and its child Spine (node 3). The skin lists Spine first, so the engine order is Hip 0,
    // Spine 1. Two clips:
    //   Walk: Hip translation (STEP, 2 keys to 2 s), Spine rotation (LINEAR, 3 keys, unnormalized),
    //         plus an Armature translation and a morph-weights channel, both skipped.
    //   Wave: CUBICSPLINE Hip scale (2 keys to 1.5 s) and Spine rotation as normalized shorts.
    std::string AnimatedGltf()
    {
        GltfBuilder builder;
        const int position = AddData(builder, std::vector<float>{ 0, 0, 0, 1, 0, 0, 0, 1, 0 }, FLOAT, 3, "VEC3");

        const int twoKeys     = AddData(builder, std::vector<float>{ 0.0f, 2.0f }, FLOAT, 2, "SCALAR");
        const int threeKeys   = AddData(builder, std::vector<float>{ 0.0f, 0.5f, 1.0f }, FLOAT, 3, "SCALAR");
        const int waveKeys    = AddData(builder, std::vector<float>{ 0.0f, 1.5f }, FLOAT, 2, "SCALAR");
        const int hipMoves    = AddData(builder, std::vector<float>{ 0, 0, 0, 0, 1, 0 }, FLOAT, 2, "VEC3");
        const int spineTurns  = AddData(builder, std::vector<float>{ 0, 0, 0, 3, 0, 0, 2, 2, 4, 0, 0, 0 }, FLOAT, 3, "VEC4");
        const int armatureMov = AddData(builder, std::vector<float>{ 5, 5, 5, 6, 6, 6 }, FLOAT, 2, "VEC3");
        const int weights     = AddData(builder, std::vector<float>{ 0, 1 }, FLOAT, 2, "SCALAR");
        // Per key: in-tangent, value, out-tangent.
        const int hipScales   = AddData(builder, std::vector<float>{ 9, 9, 9, 1, 1, 1, 9, 9, 9, 9, 9, 9, 2, 2, 2, 9, 9, 9 },
                                        FLOAT, 6, "VEC3");
        const int spineShorts = AddData(builder, std::vector<int16_t>{ 100, 100, 100, 100, 0, 0, 16384, 16384, 100, 100, 100, 100,
                                                                       100, 100, 100, 100, 0, 0, 0, 32767, 100, 100, 100, 100 },
                                        SHORT, 6, "VEC4", true);

        const auto sampler = [](int input, int output, const char* interpolation)
        {
            return "{\"input\":" + std::to_string(input) + ",\"output\":" + std::to_string(output) +
                   ",\"interpolation\":\"" + interpolation + "\"}";
        };
        const auto channel = [](int sampler, int node, const char* path)
        {
            return "{\"sampler\":" + std::to_string(sampler) + ",\"target\":{\"node\":" + std::to_string(node) +
                   ",\"path\":\"" + path + "\"}}";
        };

        const std::string walk = "{\"name\":\"Walk\",\"samplers\":[" + sampler(twoKeys, hipMoves, "STEP") + "," +
                                 sampler(threeKeys, spineTurns, "LINEAR") + "," +
                                 sampler(twoKeys, armatureMov, "LINEAR") + "," + sampler(twoKeys, weights, "LINEAR") +
                                 "],\"channels\":[" + channel(0, 2, "translation") + "," + channel(1, 3, "rotation") +
                                 "," + channel(2, 1, "translation") + "," + channel(3, 0, "weights") + "]}";
        const std::string wave = "{\"name\":\"Wave\",\"samplers\":[" + sampler(waveKeys, hipScales, "CUBICSPLINE") + "," +
                                 sampler(waveKeys, spineShorts, "CUBICSPLINE") + "],\"channels\":[" +
                                 channel(0, 2, "scale") + "," + channel(1, 3, "rotation") + "]}";

        return builder.Gltf(
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":" + std::to_string(position) + "}}]}],"
            "\"nodes\":[{\"mesh\":0,\"skin\":0},{\"name\":\"Armature\",\"children\":[2]},"
            "{\"name\":\"Hip\",\"children\":[3]},{\"name\":\"Spine\"}],"
            "\"skins\":[{\"joints\":[3,2]}],\"scenes\":[{\"nodes\":[0,1]}],"
            "\"animations\":[" + walk + "," + wave + "]");
    }

    std::optional<std::vector<LoadedAnimationClip>> LoadClips(const std::string& name, const std::string& content)
    {
        TempDir tmp;
        tmp.WriteFile(name, content);
        FS::FileSystemManager fileSystem;
        MountAssets(fileSystem, tmp.Path());
        return ContentLoader::LoadAnimations(name, fileSystem);
    }

    bool Near(float a, float b) { return std::abs(a - b) < 1e-4f; }

    bool Near(const HM::Quaternion& a, const HM::Quaternion& b)
    {
        return Near(a.x(), b.x()) && Near(a.y(), b.y()) && Near(a.z(), b.z()) && Near(a.w(), b.w());
    }
}

TEST_CASE("glTF animation - two clips load with names, durations and key counts")
{
    const auto clips = LoadClips("animated.gltf", AnimatedGltf());
    REQUIRE(clips.has_value());
    REQUIRE(clips->size() == 2u);

    const LoadedAnimationClip& walk = (*clips)[0];
    CHECK(walk.Name == "Walk");
    CHECK(walk.Duration == 2.0f);
    REQUIRE(walk.Channels.size() == 2u);
    CHECK(walk.Channels[0].Translation.Times.size() == 2u);
    CHECK(walk.Channels[0].Translation.Interpolation == AnimationInterpolation::Step);
    CHECK(walk.Channels[0].Translation.Values[1] == HM::Vector3(0.0f, 1.0f, 0.0f));
    CHECK(walk.Channels[0].Rotation.Times.empty());
    CHECK(walk.Channels[0].Scale.Times.empty());
    CHECK(walk.Channels[1].Rotation.Times.size() == 3u);
    CHECK(walk.Channels[1].Rotation.Interpolation == AnimationInterpolation::Linear);

    const LoadedAnimationClip& wave = (*clips)[1];
    CHECK(wave.Name == "Wave");
    CHECK(wave.Duration == 1.5f);
    REQUIRE(wave.Channels.size() == 2u);
    CHECK(wave.Channels[0].Scale.Times.size() == 2u);
    CHECK(wave.Channels[1].Rotation.Times.size() == 2u);
}

TEST_CASE("glTF animation - channels address skin joints, not nodes, and non-joint nodes are skipped")
{
    const auto clips = LoadClips("animated.gltf", AnimatedGltf());
    REQUIRE(clips.has_value());

    // Walk animates Hip (node 2), Spine (node 3) and the non-joint Armature (node 1): only the two
    // joints remain, as their engine indices.
    const LoadedAnimationClip& walk = (*clips)[0];
    REQUIRE(walk.Channels.size() == 2u);
    CHECK(walk.Channels[0].Joint == 0u); // Hip
    CHECK(walk.Channels[1].Joint == 1u); // Spine

    // The same file's mesh names its joints in that order.
    TempDir tmp;
    tmp.WriteFile("animated.gltf", AnimatedGltf());
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, tmp.Path());
    const auto mesh = ContentLoader::LoadMesh("animated.gltf", fileSystem);
    REQUIRE(mesh.has_value());
    REQUIRE(mesh->Skin.has_value());
    CHECK(mesh->Skin->Joints[walk.Channels[0].Joint].Name == "Hip");
    CHECK(mesh->Skin->Joints[walk.Channels[1].Joint].Name == "Spine");
}

TEST_CASE("glTF animation - rotations are normalized, and CUBICSPLINE keeps the values with one warning per clip")
{
    LogCapture log;
    const auto clips = LoadClips("animated.gltf", AnimatedGltf());
    REQUIRE(clips.has_value());

    const auto& spineTurns = (*clips)[0].Channels[1].Rotation.Values;
    REQUIRE(spineTurns.size() == 3u);
    CHECK(Near(spineTurns[0], HM::Quaternion(0.0f, 0.0f, 0.0f, 1.0f)));
    CHECK(Near(spineTurns[1], HM::Quaternion(0.0f, 0.0f, 0.7071068f, 0.7071068f)));
    CHECK(Near(spineTurns[2], HM::Quaternion(1.0f, 0.0f, 0.0f, 0.0f)));

    const LoadedAnimationClip& wave = (*clips)[1];
    CHECK(wave.Channels[0].Scale.Interpolation == AnimationInterpolation::Linear);
    CHECK(wave.Channels[0].Scale.Values == std::vector<HM::Vector3>{ HM::Vector3(1.0f, 1.0f, 1.0f),
                                                                      HM::Vector3(2.0f, 2.0f, 2.0f) });
    const auto& waveTurns = wave.Channels[1].Rotation.Values; // normalized shorts
    REQUIRE(waveTurns.size() == 2u);
    CHECK(Near(waveTurns[0], HM::Quaternion(0.0f, 0.0f, 0.7071068f, 0.7071068f)));
    CHECK(Near(waveTurns[1], HM::Quaternion::Identity()));

    const auto warnings = log.Lines("CUBICSPLINE");
    REQUIRE(warnings.size() == 1u);
    CHECK(warnings[0].find("'Wave'") != std::string::npos);
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("glTF animation - a file without a skin has no clips, and a non-glTF file is refused")
{
    GltfBuilder builder;
    const int   position = builder.AddAccessor(builder.AddView(Bytes(std::vector<float>{ 0, 0, 0, 1, 0, 0, 0, 1, 0 })),
                                               FLOAT, 3, "VEC3");
    const auto  clips    = LoadClips("static.gltf",
                                     builder.Gltf("\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":" +
                                                  std::to_string(position) + "}}]}],\"nodes\":[{\"mesh\":0}]"));
    REQUIRE(clips.has_value());
    CHECK(clips->empty());

    CHECK_FALSE(LoadClips("mesh.obj", "v 0 0 0\n").has_value());
}
