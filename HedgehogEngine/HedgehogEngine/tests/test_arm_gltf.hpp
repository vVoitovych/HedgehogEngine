#pragma once

#include "ContentLoader/tests/test_gltf_builder.hpp"

#include <string>
#include <vector>

// Test-only: a skinned arm of three joints, Shoulder at the origin, Elbow 1 m along +x bent 30
// degrees about +z, and Hand 1 m further, with one triangle bound to the shoulder and no inverse
// bind matrices (identities), so a joint's skinning matrix is its model matrix. Shared by
// HedgehogEngineTest and ScriptingTest; the skeleton's joints are Shoulder 0, Elbow 1, Hand 2.
// Its clips move the shoulder: Walk 2 m along +x each second (looping, it walks on) and Turn a
// quarter about +y over a second.
namespace ArmTest
{
    inline constexpr uint32_t HAND = 2;

    inline std::string MakeArmGltf()
    {
        GltfTest::GltfBuilder builder;
        const std::vector<float> positions = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };
        const int position = builder.AddAccessor(builder.AddView(GltfTest::Bytes(positions)), GltfTest::FLOAT, 3, "VEC3");
        const std::vector<uint8_t> joints(12, 0);
        const int joint = builder.AddAccessor(builder.AddView(GltfTest::Bytes(joints)), GltfTest::UNSIGNED_BYTE, 3, "VEC4");
        const std::vector<float> weights = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
        const int weight = builder.AddAccessor(builder.AddView(GltfTest::Bytes(weights)), GltfTest::FLOAT, 3, "VEC4");

        const std::vector<float> keyTimes = { 0, 1 };
        const int times = builder.AddAccessor(builder.AddView(GltfTest::Bytes(keyTimes)), GltfTest::FLOAT, 2, "SCALAR");
        const std::vector<float> walkValues = { 0, 0, 0, 2, 0, 0 };
        const int walk = builder.AddAccessor(builder.AddView(GltfTest::Bytes(walkValues)), GltfTest::FLOAT, 2, "VEC3");
        const std::vector<float> turnValues = { 0, 0, 0, 1, 0, 0.7071068f, 0, 0.7071068f };
        const int turn = builder.AddAccessor(builder.AddView(GltfTest::Bytes(turnValues)), GltfTest::FLOAT, 2, "VEC4");
        const auto clip = [times](const char* name, int output, const char* path)
        {
            return std::string("{\"name\":\"") + name + "\",\"samplers\":[{\"input\":" + std::to_string(times) +
                   ",\"output\":" + std::to_string(output) + ",\"interpolation\":\"LINEAR\"}],\"channels\":[" +
                   "{\"sampler\":0,\"target\":{\"node\":1,\"path\":\"" + path + "\"}}]}";
        };

        return builder.Gltf(
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":" + std::to_string(position) +
            ",\"JOINTS_0\":" + std::to_string(joint) + ",\"WEIGHTS_0\":" + std::to_string(weight) + "}}]}],"
            "\"nodes\":[{\"mesh\":0,\"skin\":0},"
            "{\"name\":\"Shoulder\",\"children\":[2]},"
            "{\"name\":\"Elbow\",\"translation\":[1,0,0],\"rotation\":[0,0,0.2588190,0.9659258],\"children\":[3]},"
            "{\"name\":\"Hand\",\"translation\":[1,0,0]}],"
            "\"animations\":[" + clip("Walk", walk, "translation") + "," + clip("Turn", turn, "rotation") + "],"
            "\"skins\":[{\"joints\":[1,2,3]}],\"scenes\":[{\"nodes\":[0,1]}],\"scene\":0");
    }
}
