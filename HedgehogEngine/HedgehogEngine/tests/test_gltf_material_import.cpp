#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Assets/GltfMaterialImport.hpp"
#include "HedgehogEngine/api/Containers/MaterialData.hpp"
#include "HedgehogEngine/api/Containers/MaterialSerializer.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "Logger/api/Logger.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace HedgehogEngine;

namespace
{
    // Every log line while it lives (engine tests do not include HedgehogScripting's capture).
    class LogCapture
    {
    public:
        LogCapture()
            : m_Sink(EngineLogger::Logger::Instance().AddSink([this](EngineLogger::LogLevel, const std::string& line)
                                                               { m_Lines.push_back(line); }))
        {
        }
        ~LogCapture() { EngineLogger::Logger::Instance().RemoveSink(m_Sink); }

        LogCapture(const LogCapture&)            = delete;
        LogCapture& operator=(const LogCapture&) = delete;

        size_t Count(const std::string& fragment) const
        {
            size_t count = 0;
            for (const std::string& line : m_Lines)
                count += line.find(fragment) != std::string::npos ? 1 : 0;
            return count;
        }

    private:
        std::vector<std::string> m_Lines;
        int                      m_Sink;
    };

    std::filesystem::path RepositoryRoot()
    {
        // tests/ -> HedgehogEngine/ -> HedgehogEngine/ -> repository root.
        return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
    }

    std::string ReadFile(const std::filesystem::path& path)
    {
        std::ifstream      file(path, std::ios::binary);
        std::ostringstream text;
        text << file.rdbuf();
        return text.str();
    }

    // A glTF with no geometry and three materials: one with every value and a map in a subfolder, one
    // named with characters a file name cannot hold, and an unnamed one.
    const std::string CAR_GLTF =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"materials\":["
        "{\"name\":\"Paint\",\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.5,0.25,1,1],\"metallicFactor\":0.25,"
        "\"roughnessFactor\":0.75,\"baseColorTexture\":{\"index\":0},\"metallicRoughnessTexture\":{\"index\":1}},"
        "\"normalTexture\":{\"index\":2,\"scale\":0.5},\"occlusionTexture\":{\"index\":3,\"strength\":0.25},"
        "\"emissiveTexture\":{\"index\":4},\"emissiveFactor\":[1,0.5,0]},"
        "{\"name\":\"Glass: tinted/clear\"},"
        "{}],"
        "\"textures\":[{\"source\":0},{\"source\":1},{\"source\":2},{\"source\":3},{\"source\":4}],"
        "\"images\":[{\"uri\":\"maps/albedo.png\"},{\"uri\":\"maps/mr.png\"},{\"uri\":\"maps/normal.png\"},"
        "{\"uri\":\"maps/ao.png\"},{\"uri\":\"maps/glow.png\"}]}";

    // A temp assets:// holding Models/Car/car.gltf.
    struct CarProject
    {
        TempDir               Dir;
        FS::FileSystemManager FileSystem;

        CarProject()
        {
            Dir.WriteFile("Models/Car/car.gltf", CAR_GLTF);
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("assets://", Dir.Path());
            REQUIRE(FileSystem.Register(std::move(fs)));
        }
    };
}

TEST_CASE("glTF material import - one .material per material beside the file, with every value")
{
    CarProject project;
    LogCapture log;
    const GltfMaterialImportResult result = ImportGltfMaterials("Models\\Car\\car.gltf", project.FileSystem);
    CHECK(result.Error.empty());
    const std::vector<std::string> expected = { "assets://Models/Car/car_Paint.material",
                                                "assets://Models/Car/car_Glass__tinted_clear.material",
                                                "assets://Models/Car/car_Material_2.material" };
    CHECK(result.Paths == expected);
    CHECK(result.Written == expected);
    CHECK(log.Count("[Material] Imported") == 3);
    CHECK(log.Count("ERROR") == 0);

    MaterialData paint;
    MaterialSerializer::Deserialize(paint, expected[0], project.FileSystem);
    CHECK(paint.type == MaterialType::Opaque);
    CHECK(paint.baseColor == "Models/Car/maps/albedo.png");
    CHECK(paint.baseColorFactor == HM::Vector4(0.5f, 0.25f, 1.0f, 1.0f));
    CHECK(paint.metallic == 0.25f);
    CHECK(paint.roughness == 0.75f);
    CHECK(paint.metallicRoughnessMap == "Models/Car/maps/mr.png");
    CHECK(paint.normalMap == "Models/Car/maps/normal.png");
    CHECK(paint.normalScale == 0.5f);
    CHECK(paint.occlusionMap == "Models/Car/maps/ao.png");
    CHECK(paint.occlusionStrength == 0.25f);
    CHECK(paint.emissiveMap == "Models/Car/maps/glow.png");
    CHECK(paint.emissiveFactor == HM::Vector3(1.0f, 0.5f, 0.0f));

    // An unnamed material: glTF's defaults, fully metallic and rough, no maps.
    MaterialData unnamed;
    MaterialSerializer::Deserialize(unnamed, expected[2], project.FileSystem);
    CHECK(unnamed.metallic == 1.0f);
    CHECK(unnamed.roughness == 1.0f);
    CHECK(unnamed.baseColor.empty());
}

TEST_CASE("glTF material import - importing again writes nothing, and an edited material keeps its edits")
{
    CarProject project;
    REQUIRE(ImportGltfMaterials("Models/Car/car.gltf", project.FileSystem).Written.size() == 3);

    const std::filesystem::path edited = project.Dir.Path() / "Models/Car/car_Paint.material";
    const std::string           mine   = "Type: 0\nRoughness: 0.1\n";
    project.Dir.WriteFile("Models/Car/car_Paint.material", mine);
    const auto writeTime = std::filesystem::last_write_time(edited);

    LogCapture                     log;
    const GltfMaterialImportResult again = ImportGltfMaterials("assets://Models/Car/car.gltf", project.FileSystem);
    CHECK(again.Error.empty());
    CHECK(again.Paths.size() == 3);
    CHECK(again.Written.empty());
    CHECK(log.Count("[Material] Imported") == 0);
    CHECK(ReadFile(edited) == mine);
    CHECK(std::filesystem::last_write_time(edited) == writeTime);

    // A material deleted since is imported again, alone.
    std::filesystem::remove(project.Dir.Path() / "Models/Car/car_Material_2.material");
    const GltfMaterialImportResult third = ImportGltfMaterials("Models/Car/car.gltf", project.FileSystem);
    CHECK(third.Written == std::vector<std::string>{ "assets://Models/Car/car_Material_2.material" });
}

TEST_CASE("glTF material import - a glTF that does not load imports nothing and says why")
{
    CarProject project;
    LogCapture log;
    const GltfMaterialImportResult missing = ImportGltfMaterials("Models/Car/missing.gltf", project.FileSystem);
    CHECK(missing.Paths.empty());
    CHECK(missing.Written.empty());
    CHECK(missing.Error == "assets://Models/Car/missing.gltf cannot be read as a glTF file.");
}

TEST_CASE("glTF material import - paths are the glTF's folder, stem and the material's name made safe")
{
    CHECK(MakeImportedMaterialPath("Models/Car/car.gltf", "Paint") == "assets://Models/Car/car_Paint.material");
    CHECK(MakeImportedMaterialPath("assets://helmet.glb", "Material_MR") == "assets://helmet_Material_MR.material");
    CHECK(MakeImportedMaterialPath("Models\\A.B\\thing.gltf", "a b.c") == "assets://Models/A.B/thing_a_b_c.material");
    CHECK(MakeImportedMaterialPath("engine://Content/x.gltf", "M-1") == "engine://Content/x_M-1.material");
}

TEST_CASE("glTF material import - the shipped helmet material is exactly what importing DamagedHelmet writes")
{
    // DamagedHelmet.gltf and its buffer copied into a temp assets:// (its images are not needed to read
    // its material).
    TempDir dir;
    const std::filesystem::path helmet = RepositoryRoot() / "Projects/FeatureTest/Assets/Models/DamagedHelmet";
    for (const char* file : { "DamagedHelmet.gltf", "DamagedHelmet.bin" })
        dir.WriteFile(std::string("Models/DamagedHelmet/") + file, ReadFile(helmet / file));
    FS::FileSystemManager fileSystem;
    auto                  fs = std::make_unique<FS::FileSystem>();
    fs->RegisterPath("assets://", dir.Path());
    REQUIRE(fileSystem.Register(std::move(fs)));

    const GltfMaterialImportResult result = ImportGltfMaterials("Models/DamagedHelmet/DamagedHelmet.gltf", fileSystem);
    REQUIRE(result.Written
            == std::vector<std::string>{ "assets://Models/DamagedHelmet/DamagedHelmet_Material_MR.material" });
    const std::string imported = ReadFile(dir.Path() / "Models/DamagedHelmet/DamagedHelmet_Material_MR.material");
    // The checkout may have given the shipped file CRLF line endings.
    std::string shipped = ReadFile(helmet / "DamagedHelmet_Material_MR.material");
    std::erase(shipped, '');
    CHECK(imported == shipped);

    // It names the five maps, which ship beside it.
    for (const char* map : { "Default_albedo.jpg", "Default_metalRoughness.jpg", "Default_normal.jpg", "Default_AO.jpg",
                             "Default_emissive.jpg" })
    {
        CAPTURE(map);
        CHECK(imported.find(std::string("Models/DamagedHelmet/") + map) != std::string::npos);
        CHECK(std::filesystem::exists(helmet / map));
    }
}
