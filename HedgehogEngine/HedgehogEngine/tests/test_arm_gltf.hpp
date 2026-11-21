#pragma once

#include "ContentLoader/tests/test_gltf_builder.hpp"

#include <string>
#include <vector>

// Test-only: a skinned arm of three joints, Shoulder at the origin, Elbow 1 m along +x bent 30
// degrees about +z, and Hand 1 m further, with one triangle bound to the shoulder and no inverse
// bind matrices (identities), so a joint's skinning matrix is its model matrix. Shared by
// HedgehogEngineTest and ScriptingTest; the skeleton's joints are Shoulder 0, Elbow 1, Hand 2.
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
        return builder.Gltf(
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":" + std::to_string(position) +
            ",\"JOINTS_0\":" + std::to_string(joint) + ",\"WEIGHTS_0\":" + std::to_string(weight) + "}}]}],"
            "\"nodes\":[{\"mesh\":0,\"skin\":0},"
            "{\"name\":\"Shoulder\",\"children\":[2]},"
            "{\"name\":\"Elbow\",\"translation\":[1,0,0],\"rotation\":[0,0,0.2588190,0.9659258],\"children\":[3]},"
            "{\"name\":\"Hand\",\"translation\":[1,0,0]}],"
            "\"skins\":[{\"joints\":[1,2,3]}],\"scenes\":[{\"nodes\":[0,1]}],\"scene\":0");
    }
}
