#include "doctest/doctest/doctest.h"

#include "ContentLoader/api/LoadedData.hpp"
#include "ContentLoader/api/MeshLoader.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"
#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using ContentLoader::LoadedMesh;

namespace
{
    constexpr int UNSIGNED_BYTE  = 5121;
    constexpr int UNSIGNED_SHORT = 5123;
    constexpr int UNSIGNED_INT   = 5125;
    constexpr int FLOAT          = 5126;

    template<typename T>
    std::string Bytes(const std::vector<T>& values)
    {
        std::string bytes(values.size() * sizeof(T), '\0');
        std::memcpy(bytes.data(), values.data(), bytes.size());
        return bytes;
    }

    std::string Base64(const std::string& bytes)
    {
        static constexpr char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string           encoded;
        for (size_t i = 0; i < bytes.size(); i += 3)
        {
            uint32_t  chunk = static_cast<uint8_t>(bytes[i]) << 16;
            const int count = static_cast<int>(std::min<size_t>(3, bytes.size() - i));
            if (count > 1)
                chunk |= static_cast<uint8_t>(bytes[i + 1]) << 8;
            if (count > 2)
                chunk |= static_cast<uint8_t>(bytes[i + 2]);
            for (int sextet = 0; sextet < 4; ++sextet)
                encoded += sextet <= count ? ALPHABET[(chunk >> (18 - 6 * sextet)) & 63] : '=';
        }
        return encoded;
    }

    // Builds a glTF file around one binary buffer: views and accessors are added in order and
    // the caller writes the meshes, nodes, skins and scenes.
    struct GltfBuilder
    {
        std::string              Bin;
        std::vector<std::string> Views;
        std::vector<std::string> Accessors;

        int AddView(const std::string& bytes, int stride = 0)
        {
            while (Bin.size() % 4 != 0)
                Bin += '\0';
            Views.push_back("{\"buffer\":0,\"byteOffset\":" + std::to_string(Bin.size()) + ",\"byteLength\":" +
                            std::to_string(bytes.size()) +
                            (stride > 0 ? ",\"byteStride\":" + std::to_string(stride) : std::string()) + "}");
            Bin += bytes;
            return static_cast<int>(Views.size()) - 1;
        }

        int AddAccessor(int view, int componentType, size_t count, const std::string& type, bool normalized = false)
        {
            Accessors.push_back("{\"bufferView\":" + std::to_string(view) + ",\"componentType\":" +
                                std::to_string(componentType) + ",\"count\":" + std::to_string(count) +
                                ",\"type\":\"" + type + "\"" + (normalized ? ",\"normalized\":true" : "") + "}");
            return static_cast<int>(Accessors.size()) - 1;
        }

        std::string Body(const std::string& rest, const std::string& uri) const
        {
            std::string json = "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"byteLength\":" +
                               std::to_string(Bin.size()) + uri + "}],\"bufferViews\":[";
            for (size_t i = 0; i < Views.size(); ++i)
                json += (i ? "," : "") + Views[i];
            json += "],\"accessors\":[";
            for (size_t i = 0; i < Accessors.size(); ++i)
                json += (i ? "," : "") + Accessors[i];
            return json + "]," + rest + "}";
        }

        std::string Gltf(const std::string& rest) const
        {
            return Body(rest, ",\"uri\":\"data:application/octet-stream;base64," + Base64(Bin) + "\"");
        }

        // The same file as binary glTF: a 12-byte header, the JSON chunk padded with spaces and
        // the BIN chunk padded with zeros.
        std::string Glb(const std::string& rest) const
        {
            std::string json = Body(rest, "");
            while (json.size() % 4 != 0)
                json += ' ';
            std::string bin = Bin;
            while (bin.size() % 4 != 0)
                bin += '\0';
            const auto word = [](uint32_t value) { return Bytes(std::vector<uint32_t>{ value }); };
            return word(0x46546C67) + word(2) + word(static_cast<uint32_t>(12 + 8 + json.size() + 8 + bin.size())) +
                   word(static_cast<uint32_t>(json.size())) + word(0x4E4F534A) + json +
                   word(static_cast<uint32_t>(bin.size())) + word(0x004E4942) + bin;
        }
    };

    std::vector<float> TRIANGLE = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };

    // Root (node 1, given as a matrix) with one child joint, Child (node 2, given as TRS), and a
    // skinned triangle on node 0. The skin lists Child before Root, so the loader must reorder.
    std::string SkinnedNodes(int inverseBindAccessor)
    {
        return "\"nodes\":["
               "{\"mesh\":0,\"skin\":0},"
               "{\"name\":\"Root\",\"children\":[2],\"matrix\":[0,2,0,0, -2,0,0,0, 0,0,2,0, 1,2,3,1]},"
               "{\"name\":\"Child\",\"translation\":[0,1,0],\"rotation\":[0,0.7071068,0,0.7071068],\"scale\":[1,1,3]}],"
               "\"skins\":[{\"joints\":[2,1]" +
               (inverseBindAccessor >= 0 ? ",\"inverseBindMatrices\":" + std::to_string(inverseBindAccessor) : std::string()) +
               "}],\"scenes\":[{\"nodes\":[0,1]}],\"scene\":0";
    }

    std::string SkinnedMesh(int position, int joints, int weights)
    {
        return "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":" + std::to_string(position) +
               ",\"JOINTS_0\":" + std::to_string(joints) + ",\"WEIGHTS_0\":" + std::to_string(weights) + "}}]}]";
    }

    // glTF joint indices for the three vertices: vertex 0 uses Child and Root, vertex 1 Root alone.
    const std::vector<uint32_t> GLTF_JOINTS  = { 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 };
    const std::vector<float>    GLTF_WEIGHTS = { 0.5f, 0.5f, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0 };

    // Writes the joints as componentType, either tightly packed or interleaved with 0xFF filler
    // of the same size, which the stride must skip.
    int AddJoints(GltfBuilder& builder, int componentType, bool interleaved)
    {
        const size_t size = componentType == UNSIGNED_BYTE ? 1 : componentType == UNSIGNED_SHORT ? 2 : 4;
        std::string  bytes;
        for (size_t vertex = 0; vertex < 3; ++vertex)
        {
            for (size_t slot = 0; slot < 4; ++slot)
            {
                const uint32_t value = GLTF_JOINTS[vertex * 4 + slot];
                bytes += Bytes(std::vector<uint32_t>{ value }).substr(0, size);
            }
            if (interleaved)
                bytes += std::string(size * 4, '\xFF');
        }
        const int view = builder.AddView(bytes, interleaved ? static_cast<int>(size * 8) : 0);
        return builder.AddAccessor(view, componentType, 3, "VEC4");
    }

    void MountAssets(FS::FileSystemManager& manager, const std::filesystem::path& dir)
    {
        auto fs = std::make_unique<FS::FileSystem>();
        fs->RegisterPath("assets://", dir);
        REQUIRE(manager.Register(std::move(fs)));
    }

    std::optional<LoadedMesh> LoadFile(const std::string& name, const std::string& content)
    {
        TempDir tmp;
        tmp.WriteFile(name, content);
        FS::FileSystemManager fileSystem;
        MountAssets(fileSystem, tmp.Path());
        return ContentLoader::LoadMesh(name, fileSystem);
    }

    std::string SkinnedGltf(int jointType, bool interleaved, bool binary)
    {
        GltfBuilder builder;
        const int   position = builder.AddAccessor(builder.AddView(Bytes(TRIANGLE)), FLOAT, 3, "VEC3");
        const int   joints   = AddJoints(builder, jointType, interleaved);
        const int   weights  = builder.AddAccessor(builder.AddView(Bytes(GLTF_WEIGHTS)), FLOAT, 3, "VEC4");
        const std::string rest = SkinnedMesh(position, joints, weights) + "," + SkinnedNodes(-1);
        return binary ? builder.Glb(rest) : builder.Gltf(rest);
    }

    bool Near(float a, float b) { return std::abs(a - b) < 1e-5f; }

    bool Near(const HM::Vector4& a, const HM::Vector4& b)
    {
        return Near(a.x(), b.x()) && Near(a.y(), b.y()) && Near(a.z(), b.z()) && Near(a.w(), b.w());
    }

    // After reordering Root is joint 0 and Child joint 1; zero-weight slots point at joint 0.
    void CheckSkinning(const LoadedMesh& mesh)
    {
        REQUIRE(mesh.Skin.has_value());
        REQUIRE(mesh.Joints.size() == 3u);
        REQUIRE(mesh.Weights.size() == 3u);
        CHECK(mesh.Joints[0] == HM::Vector4u(1u, 0u, 0u, 0u));
        CHECK(mesh.Joints[1] == HM::Vector4u(0u, 0u, 0u, 0u));
        CHECK(mesh.Joints[2] == HM::Vector4u(0u, 0u, 0u, 0u));
        CHECK(Near(mesh.Weights[0], HM::Vector4(0.5f, 0.5f, 0.0f, 0.0f)));
        CHECK(Near(mesh.Weights[1], HM::Vector4(1.0f, 0.0f, 0.0f, 0.0f))); // 2 normalized to 1
        CHECK(Near(mesh.Weights[2], HM::Vector4(1.0f, 0.0f, 0.0f, 0.0f))); // all zero: joint 0, weight 1
    }
}

TEST_CASE("glTF skin - JOINTS_0 as u8, u16 and u32, packed and interleaved, load identically")
{
    for (const int jointType : { UNSIGNED_BYTE, UNSIGNED_SHORT, UNSIGNED_INT })
    {
        for (const bool interleaved : { false, true })
        {
            CAPTURE(jointType);
            CAPTURE(interleaved);
            const auto mesh = LoadFile("skinned.gltf", SkinnedGltf(jointType, interleaved, false));
            REQUIRE(mesh.has_value());
            CHECK(mesh->vertices.size() == 3u);
            CheckSkinning(*mesh);
        }
    }
}

TEST_CASE("glTF skin - normalized u8 weights sum to 1")
{
    GltfBuilder builder;
    const int   position = builder.AddAccessor(builder.AddView(Bytes(TRIANGLE)), FLOAT, 3, "VEC3");
    const int   joints   = AddJoints(builder, UNSIGNED_BYTE, false);
    const std::vector<uint8_t> weightBytes = { 64, 64, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0 };
    const int weights = builder.AddAccessor(builder.AddView(Bytes(weightBytes)), UNSIGNED_BYTE, 3, "VEC4", true);

    const auto mesh = LoadFile("weights.gltf", builder.Gltf(SkinnedMesh(position, joints, weights) + "," + SkinnedNodes(-1)));
    REQUIRE(mesh.has_value());
    CheckSkinning(*mesh);
}

TEST_CASE("glTF skin - joints come parent before child with names, inverse bind matrices and bind pose")
{
    GltfBuilder builder;
    const int   position = builder.AddAccessor(builder.AddView(Bytes(TRIANGLE)), FLOAT, 3, "VEC3");
    const int   joints   = AddJoints(builder, UNSIGNED_SHORT, false);
    const int   weights  = builder.AddAccessor(builder.AddView(Bytes(GLTF_WEIGHTS)), FLOAT, 3, "VEC4");
    // glTF order: Child's matrix (a translation by -5 on x), then Root's (identity).
    std::vector<float> inverseBind = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -5, 0, 0, 1,
                                       1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0,  0, 0, 1 };
    const int ibm = builder.AddAccessor(builder.AddView(Bytes(inverseBind)), FLOAT, 2, "MAT4");

    const auto mesh = LoadFile("bind.gltf", builder.Gltf(SkinnedMesh(position, joints, weights) + "," + SkinnedNodes(ibm)));
    REQUIRE(mesh.has_value());
    REQUIRE(mesh->Skin.has_value());
    const auto& skin = mesh->Skin->Joints;
    REQUIRE(skin.size() == 2u);

    CHECK(skin[0].Name == "Root");
    CHECK(skin[0].Parent == -1);
    CHECK(skin[1].Name == "Child");
    CHECK(skin[1].Parent == 0);

    CHECK(skin[0].InverseBindMatrix == HM::Matrix4x4::GetIdentity());
    CHECK(skin[1].InverseBindMatrix == HM::Matrix4x4::GetTranslation(-5.0f, 0.0f, 0.0f));

    // Root's matrix is translate (1, 2, 3) * rotate 90 degrees about Z * scale 2.
    CHECK(skin[0].Translation == HM::Vector3(1.0f, 2.0f, 3.0f));
    CHECK(Near(skin[0].Scale.x(), 2.0f));
    CHECK(Near(skin[0].Scale.y(), 2.0f));
    CHECK(Near(skin[0].Scale.z(), 2.0f));
    CHECK(Near(skin[0].Rotation.x(), 0.0f));
    CHECK(Near(skin[0].Rotation.y(), 0.0f));
    CHECK(Near(skin[0].Rotation.z(), 0.7071068f));
    CHECK(Near(skin[0].Rotation.w(), 0.7071068f));
    // The decomposed rotation turns +X to +Y, as the matrix's first row says.
    const HM::Vector3 turned = skin[0].Rotation.Rotate(HM::Vector3(1.0f, 0.0f, 0.0f));
    CHECK(Near(turned.x(), 0.0f));
    CHECK(Near(turned.y(), 1.0f));

    CHECK(skin[1].Translation == HM::Vector3(0.0f, 1.0f, 0.0f));
    CHECK(Near(skin[1].Rotation.y(), 0.7071068f));
    CHECK(Near(skin[1].Rotation.w(), 0.7071068f));
    CHECK(skin[1].Scale == HM::Vector3(1.0f, 1.0f, 3.0f));
}

TEST_CASE("glTF skin - a joint missing from the file or the scene fails, naming the file and the joint")
{
    GltfBuilder builder;
    const int   position = builder.AddAccessor(builder.AddView(Bytes(TRIANGLE)), FLOAT, 3, "VEC3");
    const int   joints   = AddJoints(builder, UNSIGNED_BYTE, false);
    const int   weights  = builder.AddAccessor(builder.AddView(Bytes(GLTF_WEIGHTS)), FLOAT, 3, "VEC4");
    const std::string mesh = SkinnedMesh(position, joints, weights);

    SUBCASE("a joint node that does not exist")
    {
        LogCapture log;
        CHECK_FALSE(LoadFile("missing.gltf", builder.Gltf(mesh + ",\"nodes\":[{\"mesh\":0,\"skin\":0},{\"name\":\"Root\"}],"
                                                                  "\"skins\":[{\"joints\":[1,7]}],\"scenes\":[{\"nodes\":[0,1]}]"))
                         .has_value());
        const auto errors = log.Lines("[ERROR]");
        REQUIRE(errors.size() == 1u);
        CHECK(errors[0].find("missing.gltf") != std::string::npos);
        CHECK(errors[0].find("refers to node 7") != std::string::npos);
    }
    SUBCASE("a joint node outside the scene")
    {
        LogCapture log;
        CHECK_FALSE(LoadFile("orphan.gltf", builder.Gltf(mesh + ",\"nodes\":[{\"mesh\":0,\"skin\":0},{\"name\":\"Root\"},"
                                                                "{\"name\":\"Orphan\"}],"
                                                                "\"skins\":[{\"joints\":[1,2]}],\"scenes\":[{\"nodes\":[0,1]}]"))
                         .has_value());
        const auto errors = log.Lines("[ERROR]");
        REQUIRE(errors.size() == 1u);
        CHECK(errors[0].find("orphan.gltf") != std::string::npos);
        CHECK(errors[0].find("'Orphan' (node 2) ") != std::string::npos);
        CHECK(errors[0].find("not in the scene") != std::string::npos);
    }
}

TEST_CASE("glTF - .glb files load like their .gltf twins")
{
    const auto text   = LoadFile("skinned.gltf", SkinnedGltf(UNSIGNED_SHORT, true, false));
    const auto binary = LoadFile("skinned.glb", SkinnedGltf(UNSIGNED_SHORT, true, true));
    REQUIRE(text.has_value());
    REQUIRE(binary.has_value());
    CHECK(binary->vertices == text->vertices);
    CHECK(binary->indices == text->indices);
    CheckSkinning(*binary);
    REQUIRE(binary->Skin->Joints.size() == 2u);
    CHECK(binary->Skin->Joints[0].Name == "Root");
}

TEST_CASE("glTF - a static mesh has no skin, and each primitive's indices are offset by the earlier vertices")
{
    GltfBuilder builder;
    const int   position = builder.AddAccessor(builder.AddView(Bytes(TRIANGLE)), FLOAT, 3, "VEC3");
    const int   indices  = builder.AddAccessor(builder.AddView(Bytes(std::vector<uint16_t>{ 0, 1, 2 })), UNSIGNED_SHORT,
                                               3, "SCALAR");
    const std::string primitive = "{\"attributes\":{\"POSITION\":" + std::to_string(position) +
                                  "},\"indices\":" + std::to_string(indices) + "}";

    const auto mesh = LoadFile("static.gltf", builder.Gltf("\"meshes\":[{\"primitives\":[" + primitive + "," + primitive +
                                                           "]}],\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}]"));
    REQUIRE(mesh.has_value());
    CHECK_FALSE(mesh->Skin.has_value());
    CHECK(mesh->Joints.empty());
    CHECK(mesh->Weights.empty());
    REQUIRE(mesh->vertices.size() == 6u);
    CHECK(mesh->indices == std::vector<uint32_t>{ 0, 1, 2, 3, 4, 5 });
}
