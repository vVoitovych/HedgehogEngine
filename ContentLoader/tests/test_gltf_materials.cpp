#include "doctest/doctest/doctest.h"

#include "ContentLoader/api/MaterialLoader.hpp"
#include "ContentLoader/api/MeshLoader.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"
#include "HedgehogScripting/tests/test_log_capture.hpp"

#include "test_gltf_builder.hpp"

#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using ContentLoader::LoadedMaterial;
using namespace GltfTest;

namespace
{
    const std::vector<float> TRIANGLE = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };

    // A triangle drawn once per given material index (-1 for a primitive without one), so the mesh
    // loader sees one primitive per entry.
    std::string Meshes(GltfBuilder& builder, const std::vector<int>& materials)
    {
        const int   position = builder.AddAccessor(builder.AddView(Bytes(TRIANGLE)), FLOAT, 3, "VEC3");
        std::string json     = "\"meshes\":[{\"primitives\":[";
        for (size_t i = 0; i < materials.size(); ++i)
        {
            json += (i ? "," : "") + std::string("{\"attributes\":{\"POSITION\":") + std::to_string(position) + "}";
            if (materials[i] >= 0)
                json += ",\"material\":" + std::to_string(materials[i]);
            json += "}";
        }
        return json + "]}],\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}]";
    }

    // Two materials: Painted with every slot, factor and strength set, and a bare one with nothing.
    // The images sit in a folder beside the file, one with a space in its name.
    const std::string MATERIALS =
        "\"materials\":["
        "{\"name\":\"Painted\","
        "\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.5,0.25,1,0.75],\"metallicFactor\":0.2,\"roughnessFactor\":0.7,"
        "\"baseColorTexture\":{\"index\":0},\"metallicRoughnessTexture\":{\"index\":1}},"
        "\"normalTexture\":{\"index\":2,\"scale\":0.5},\"occlusionTexture\":{\"index\":3,\"strength\":0.3},"
        "\"emissiveTexture\":{\"index\":4},\"emissiveFactor\":[1,0.5,0]},"
        "{}],"
        "\"textures\":[{\"source\":0},{\"source\":1},{\"source\":2},{\"source\":3},{\"source\":4}],"
        "\"images\":[{\"uri\":\"maps/albedo.png\"},{\"uri\":\"maps/./mr.png\"},{\"uri\":\"maps/My%20Normal.png\"},"
        "{\"uri\":\"../shared/ao.png\"},{\"uri\":\"maps\\\\glow.png\"}]";

    struct MaterialFile
    {
        TempDir               Dir;
        FS::FileSystemManager FileSystem;

        explicit MaterialFile(const std::string& name, const std::string& content)
        {
            Dir.WriteFile(name, content);
            MountAssets(FileSystem, Dir.Path());
        }
    };

    bool Near(const HM::Vector4& a, const HM::Vector4& b)
    {
        return std::abs(a.x() - b.x()) < 1e-6f && std::abs(a.y() - b.y()) < 1e-6f && std::abs(a.z() - b.z()) < 1e-6f
            && std::abs(a.w() - b.w()) < 1e-6f;
    }
}

TEST_CASE("glTF materials - every material in file order, with its values and maps next to the file")
{
    GltfBuilder builder;
    const std::string rest = Meshes(builder, { 0 }) + "," + MATERIALS;
    MaterialFile file("Models/Car/car.gltf", builder.Gltf(rest));

    LogCapture log;
    const auto materials = ContentLoader::LoadGltfMaterials("Models/Car/car.gltf", file.FileSystem);
    REQUIRE(materials.has_value());
    CHECK(log.Lines("[WARNING").empty());
    CHECK(log.Lines("[ERROR").empty());
    REQUIRE(materials->size() == 2);

    const LoadedMaterial& painted = (*materials)[0];
    CHECK(painted.Name == "Painted");
    CHECK(Near(painted.BaseColorFactor, HM::Vector4(0.5f, 0.25f, 1.0f, 0.75f)));
    CHECK(painted.Metallic == doctest::Approx(0.2f));
    CHECK(painted.Roughness == doctest::Approx(0.7f));
    CHECK(painted.NormalScale == doctest::Approx(0.5f));
    CHECK(painted.OcclusionStrength == doctest::Approx(0.3f));
    CHECK(painted.EmissiveFactor.x() == 1.0f);
    CHECK(painted.EmissiveFactor.y() == 0.5f);
    CHECK(painted.EmissiveFactor.z() == 0.0f);
    // Next to the file: "." folded, ".." leaving its folder, a percent escape decoded, a backslash a slash.
    CHECK(painted.BaseColorMap == "assets://Models/Car/maps/albedo.png");
    CHECK(painted.MetallicRoughnessMap == "assets://Models/Car/maps/mr.png");
    CHECK(painted.NormalMap == "assets://Models/Car/maps/My Normal.png");
    CHECK(painted.OcclusionMap == "assets://Models/shared/ao.png");
    CHECK(painted.EmissiveMap == "assets://Models/Car/maps/glow.png");

    // A bare material has glTF's defaults: white, fully metallic and rough, no maps; and a name.
    const LoadedMaterial& bare = (*materials)[1];
    CHECK(bare.Name == "Material 1");
    CHECK(Near(bare.BaseColorFactor, HM::Vector4(1.0f, 1.0f, 1.0f, 1.0f)));
    CHECK(bare.Metallic == 1.0f);
    CHECK(bare.Roughness == 1.0f);
    CHECK(bare.NormalScale == 1.0f);
    CHECK(bare.OcclusionStrength == 1.0f);
    CHECK(bare.EmissiveFactor.x() == 0.0f);
    for (const std::string* map : { &bare.BaseColorMap, &bare.MetallicRoughnessMap, &bare.NormalMap, &bare.OcclusionMap,
                                    &bare.EmissiveMap })
        CHECK(map->empty());

    // The same file binary, and through its full virtual path.
    MaterialFile binary("car.glb", builder.Glb(rest));
    const auto   fromGlb = ContentLoader::LoadGltfMaterials("assets://car.glb", binary.FileSystem);
    REQUIRE(fromGlb.has_value());
    REQUIRE(fromGlb->size() == 2);
    CHECK((*fromGlb)[0].BaseColorMap == "assets://maps/albedo.png");
}

TEST_CASE("glTF materials - alpha mode, cutoff and double-sidedness, with glTF's defaults")
{
    GltfBuilder       builder;
    const std::string rest = Meshes(builder, { 0 }) + ","
                             "\"materials\":["
                             "{\"name\":\"Fence\",\"alphaMode\":\"MASK\",\"alphaCutoff\":0.25,\"doubleSided\":true},"
                             "{\"name\":\"Glass\",\"alphaMode\":\"BLEND\"},"
                             "{\"name\":\"Solid\",\"alphaMode\":\"OPAQUE\"},"
                             "{\"name\":\"Plain\"},"
                             "{\"name\":\"Odd\",\"alphaMode\":\"DITHER\"}]";
    MaterialFile file("alpha.gltf", builder.Gltf(rest));

    LogCapture log;
    const auto materials = ContentLoader::LoadGltfMaterials("alpha.gltf", file.FileSystem);
    REQUIRE(materials.has_value());
    REQUIRE(materials->size() == 5);

    CHECK((*materials)[0].AlphaMode == ContentLoader::LoadedAlphaMode::Mask);
    CHECK((*materials)[0].AlphaCutoff == doctest::Approx(0.25f));
    CHECK((*materials)[0].DoubleSided);
    CHECK((*materials)[1].AlphaMode == ContentLoader::LoadedAlphaMode::Blend);
    CHECK((*materials)[1].AlphaCutoff == 0.5f);
    CHECK_FALSE((*materials)[1].DoubleSided);
    CHECK((*materials)[2].AlphaMode == ContentLoader::LoadedAlphaMode::Opaque);
    CHECK((*materials)[3].AlphaMode == ContentLoader::LoadedAlphaMode::Opaque);
    CHECK((*materials)[3].AlphaCutoff == 0.5f);
    CHECK_FALSE((*materials)[3].DoubleSided);

    // An unknown mode reads as OPAQUE with one warning naming the file, material and mode.
    CHECK((*materials)[4].AlphaMode == ContentLoader::LoadedAlphaMode::Opaque);
    CHECK(log.Lines("[WARNING").size() == 1);
    CHECK(log.Lines("assets://alpha.gltf: material 'Odd' has alpha mode 'DITHER'").size() == 1);
}

TEST_CASE("glTF materials - an embedded image is skipped and a second UV set warned, one warning each")
{
    GltfBuilder builder;
    const int   pixelView = builder.AddView(std::string(4, '\0'));
    const std::string rest =
        Meshes(builder, { 0 }) +
        ",\"materials\":[{\"name\":\"Mixed\",\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0},"
        "\"metallicRoughnessTexture\":{\"index\":1}},\"normalTexture\":{\"index\":2,\"texCoord\":1}}],"
        "\"textures\":[{\"source\":0},{\"source\":1},{\"source\":2}],"
        "\"images\":[{\"uri\":\"data:image/png;base64,AAAA\"},{\"bufferView\":" + std::to_string(pixelView) +
        ",\"mimeType\":\"image/png\"},{\"uri\":\"normal.png\"}]";
    MaterialFile file("mixed.gltf", builder.Gltf(rest));

    LogCapture log;
    const auto materials = ContentLoader::LoadGltfMaterials("mixed.gltf", file.FileSystem);
    REQUIRE(materials.has_value());
    REQUIRE(materials->size() == 1);
    const LoadedMaterial& mixed = materials->front();
    CHECK(mixed.BaseColorMap.empty());
    CHECK(mixed.MetallicRoughnessMap.empty());
    CHECK(mixed.NormalMap == "assets://normal.png"); // kept, read with the first UVs

    const auto warnings = log.Lines("[WARNING");
    REQUIRE(warnings.size() == 3);
    CHECK(warnings[0].find("'Mixed' base colour image is embedded") != std::string::npos);
    CHECK(warnings[1].find("'Mixed' metallic-roughness image is embedded") != std::string::npos);
    CHECK(warnings[2].find("'Mixed' normal reads texture coordinates 1") != std::string::npos);
    for (const std::string& warning : warnings)
        CHECK(warning.find("assets://mixed.gltf") != std::string::npos);
}

TEST_CASE("glTF materials - a mesh keeps its first primitive's material, with one warning for a mix")
{
    SUBCASE("One material")
    {
        GltfBuilder  builder;
        MaterialFile file("one.gltf", builder.Gltf(Meshes(builder, { 1, 1 }) + "," + MATERIALS));
        LogCapture   log;
        const auto   mesh = ContentLoader::LoadMesh("one.gltf", file.FileSystem);
        REQUIRE(mesh.has_value());
        CHECK(mesh->MaterialIndex == 1);
        CHECK(log.Lines("[WARNING").empty());
    }
    SUBCASE("A mix")
    {
        GltfBuilder  builder;
        MaterialFile file("mix.gltf", builder.Gltf(Meshes(builder, { 0, 1, -1 }) + "," + MATERIALS));
        LogCapture   log;
        const auto   mesh = ContentLoader::LoadMesh("mix.gltf", file.FileSystem);
        REQUIRE(mesh.has_value());
        CHECK(mesh->MaterialIndex == 0);
        const auto warnings = log.Lines("[WARNING");
        REQUIRE(warnings.size() == 1);
        CHECK(warnings[0].find("more than one material") != std::string::npos);
    }
    SUBCASE("None")
    {
        GltfBuilder  builder;
        MaterialFile file("none.gltf", builder.Gltf(Meshes(builder, { -1 })));
        const auto   mesh = ContentLoader::LoadMesh("none.gltf", file.FileSystem);
        REQUIRE(mesh.has_value());
        CHECK(mesh->MaterialIndex == -1);
        const auto materials = ContentLoader::LoadGltfMaterials("none.gltf", file.FileSystem);
        REQUIRE(materials.has_value());
        CHECK(materials->empty());
    }
}

TEST_CASE("glTF materials - a missing file, an OBJ and broken JSON each give nothing with one error")
{
    MaterialFile file("bad.gltf", "{ not json");
    file.Dir.WriteFile("mesh.obj", "v 0 0 0\n");
    for (const char* name : { "missing.gltf", "mesh.obj", "bad.gltf" })
    {
        CAPTURE(name);
        LogCapture log;
        CHECK_FALSE(ContentLoader::LoadGltfMaterials(name, file.FileSystem).has_value());
        CHECK(log.Lines("[ERROR").size() == 1);
    }
}

TEST_CASE("glTF materials - DamagedHelmet reads as Material_MR with its five maps")
{
    // tests/ -> ContentLoader/ -> repository root.
    const std::filesystem::path assets =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Projects/FeatureTest/Assets";
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, assets);

    LogCapture log;
    const auto materials = ContentLoader::LoadGltfMaterials("Models/DamagedHelmet/DamagedHelmet.gltf", fileSystem);
    REQUIRE(materials.has_value());
    CHECK(log.Lines("[WARNING").empty());
    REQUIRE(materials->size() == 1);
    const LoadedMaterial& helmet = materials->front();
    CHECK(helmet.Name == "Material_MR");
    CHECK(helmet.BaseColorMap == "assets://Models/DamagedHelmet/Default_albedo.jpg");
    CHECK(helmet.MetallicRoughnessMap == "assets://Models/DamagedHelmet/Default_metalRoughness.jpg");
    CHECK(helmet.NormalMap == "assets://Models/DamagedHelmet/Default_normal.jpg");
    CHECK(helmet.OcclusionMap == "assets://Models/DamagedHelmet/Default_AO.jpg");
    CHECK(helmet.EmissiveMap == "assets://Models/DamagedHelmet/Default_emissive.jpg");
    CHECK(helmet.EmissiveFactor.x() == 1.0f);
    CHECK(helmet.Metallic == 1.0f);
    CHECK(helmet.Roughness == 1.0f);
    // Every map exists on disk.
    for (const std::string* map : { &helmet.BaseColorMap, &helmet.MetallicRoughnessMap, &helmet.NormalMap,
                                    &helmet.OcclusionMap, &helmet.EmissiveMap })
    {
        const auto physical = fileSystem.ResolvePhysical(*map);
        CHECK((physical && std::filesystem::exists(*physical)));
    }

    const auto mesh = ContentLoader::LoadMesh("Models/DamagedHelmet/DamagedHelmet.gltf", fileSystem);
    REQUIRE(mesh.has_value());
    CHECK(mesh->MaterialIndex == 0);
}
