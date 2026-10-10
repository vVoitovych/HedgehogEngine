#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
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
    // Every log line while it lives.
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
        size_t Size() const { return m_Lines.size(); }

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

    void CheckPbrDefaults(const MaterialData& material)
    {
        CHECK(material.baseColorFactor == HM::Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        CHECK(material.metallic == 0.0f);
        CHECK(material.roughness == 0.5f);
        CHECK(material.metallicRoughnessMap.empty());
        CHECK(material.normalMap.empty());
        CHECK(material.normalScale == 1.0f);
        CHECK(material.occlusionMap.empty());
        CHECK(material.occlusionStrength == 1.0f);
        CHECK(material.emissiveMap.empty());
        CHECK(material.emissiveFactor == HM::Vector3(0.0f, 0.0f, 0.0f));
        CHECK(material.alphaCutoff == 0.5f);
        CHECK_FALSE(material.doubleSided);
    }
}

TEST_CASE("Material format - the shipped materials load their own keys and the PBR defaults, with no warning")
{
    const std::filesystem::path root = RepositoryRoot();
    for (const std::filesystem::path& file :
         { root / "Projects/FeatureTest/Assets/Materials/test1.material", root / "Projects/FeatureTest/Assets/Materials/test2.material",
           root / "Projects/FeatureTest/Assets/Materials/test3.material", root / "Templates/Empty/Assets/Materials/Default.material" })
    {
        CAPTURE(file.string());
        MaterialData material;
        LogCapture   log;
        REQUIRE(MaterialSerializer::ReadText(material, ReadFile(file), file.string()));
        CHECK(log.Size() == 0);
        CHECK(material.type == MaterialType::Opaque);
        CHECK_FALSE(material.baseColor.empty());
        CheckPbrDefaults(material);
    }

    MaterialData helmet;
    REQUIRE(MaterialSerializer::ReadText(helmet, ReadFile(root / "Projects/FeatureTest/Assets/Materials/test2.material"), "test2"));
    CHECK(helmet.baseColor == "Models\\DamagedHelmet\\Default_albedo.jpg");
    CHECK(helmet.transparency == doctest::Approx(0.464f));
}

TEST_CASE("Material format - every field round-trips, and a second write gives the same text")
{
    MaterialData material;
    material.type                 = MaterialType::Cutoff;
    material.baseColor            = "Textures/brick.png";
    material.transparency         = 0.25f;
    material.alphaCutoff          = 0.375f;
    material.doubleSided          = true;
    material.baseColorFactor      = HM::Vector4(0.5f, 0.25f, 1.0f, 0.75f);
    material.metallic             = 1.0f;
    material.roughness            = 0.125f;
    material.metallicRoughnessMap = "Textures/brick_mr.png";
    material.normalMap            = "Textures/brick_n.png";
    material.normalScale          = 0.5f;
    material.occlusionMap         = "Textures/brick_ao.png";
    material.occlusionStrength    = 0.75f;
    material.emissiveMap          = "Textures/brick_e.png";
    material.emissiveFactor       = HM::Vector3(2.0f, 1.0f, 0.5f);

    const std::string text = MaterialSerializer::WriteText(material);
    MaterialData      read;
    LogCapture        log;
    REQUIRE(MaterialSerializer::ReadText(read, text, "brick"));
    CHECK(log.Size() == 0);

    CHECK(read.type == MaterialType::Cutoff);
    CHECK(read.baseColor == material.baseColor);
    CHECK(read.transparency == material.transparency);
    CHECK(read.alphaCutoff == material.alphaCutoff);
    CHECK(read.doubleSided);
    CHECK(read.baseColorFactor == material.baseColorFactor);
    CHECK(read.metallic == material.metallic);
    CHECK(read.roughness == material.roughness);
    CHECK(read.metallicRoughnessMap == material.metallicRoughnessMap);
    CHECK(read.normalMap == material.normalMap);
    CHECK(read.normalScale == material.normalScale);
    CHECK(read.occlusionMap == material.occlusionMap);
    CHECK(read.occlusionStrength == material.occlusionStrength);
    CHECK(read.emissiveMap == material.emissiveMap);
    CHECK(read.emissiveFactor == material.emissiveFactor);
    CHECK(MaterialSerializer::WriteText(read) == text);

    // The keys come in a fixed order.
    std::vector<size_t> positions;
    for (const char* key : { "Type:", "BaseColor:", "BaseColorFactor:", "Transparency:", "AlphaCutoff:", "DoubleSided:", "Metallic:", "Roughness:",
                             "MetallicRoughnessMap:", "NormalMap:", "NormalScale:", "OcclusionMap:",
                             "OcclusionStrength:", "EmissiveMap:", "EmissiveFactor:" })
    {
        CAPTURE(key);
        const size_t position = text.find(key);
        REQUIRE(position != std::string::npos);
        positions.push_back(position);
    }
    for (size_t i = 1; i < positions.size(); ++i)
        CHECK(positions[i - 1] < positions[i]);
}

TEST_CASE("Material format - a missing key keeps its default")
{
    MaterialData material;
    LogCapture   log;
    REQUIRE(MaterialSerializer::ReadText(material, "Metallic: 0.8\nNormalMap: Textures/n.png\n", "partial"));
    CHECK(log.Size() == 0);
    CHECK(material.metallic == doctest::Approx(0.8f));
    CHECK(material.normalMap == "Textures/n.png");
    CHECK(material.type == MaterialType::Opaque);
    CHECK(material.baseColor.empty());
    CHECK(material.transparency == 1.0f);
    CHECK(material.roughness == 0.5f);
    CHECK(material.baseColorFactor == HM::Vector4(1.0f, 1.0f, 1.0f, 1.0f));
}

TEST_CASE("Material format - an unreadable value keeps its default with one warning naming the file and key")
{
    struct Case
    {
        const char* Text;
        const char* Key;
    };
    const Case cases[] = {
        { "Metallic: shiny\n", "Metallic" },
        { "Roughness: .nan\n", "Roughness" },
        { "BaseColorFactor: [1, 2]\n", "BaseColorFactor" },
        { "EmissiveFactor: blue\n", "EmissiveFactor" },
        { "Type: 7\n", "Type" },
        { "Type: 0.5\n", "Type" },
        { "NormalMap: [a, b]\n", "NormalMap" },
        { "AlphaCutoff: 1.5\n", "AlphaCutoff" },
        { "AlphaCutoff: -0.1\n", "AlphaCutoff" },
        { "AlphaCutoff: half\n", "AlphaCutoff" },
        { "DoubleSided: maybe\n", "DoubleSided" },
    };
    for (const Case& bad : cases)
    {
        CAPTURE(bad.Text);
        MaterialData material;
        LogCapture   log;
        REQUIRE(MaterialSerializer::ReadText(material, std::string(bad.Text) + "Transparency: 0.5\n", "assets://bad.material"));
        CHECK(log.Size() == 1);
        CHECK(log.Count(std::string("[Material] assets://bad.material: ") + bad.Key + " is not") == 1);
        CHECK(material.transparency == 0.5f); // the rest of the file is read
        CHECK(material.type == MaterialType::Opaque);
        CheckPbrDefaults(material);
    }
}

TEST_CASE("Material format - text that is not a map of keys is refused with one error, changing nothing")
{
    for (const char* text : { "[1, 2, 3]", "Metallic: [", "just a word" })
    {
        CAPTURE(text);
        MaterialData material;
        material.metallic = 0.25f;
        LogCapture log;
        CHECK_FALSE(MaterialSerializer::ReadText(material, text, "broken"));
        CHECK(log.Count("[Material] broken") == 1);
        CHECK(material.metallic == 0.25f);
    }
}

TEST_CASE("Material format - each slot names its map, and setting one marks the material dirty")
{
    MaterialData material;
    material.baseColor            = "b.png";
    material.normalMap            = "n.png";
    material.metallicRoughnessMap = "mr.png";
    material.occlusionMap         = "ao.png";
    material.emissiveMap          = "e.png";
    CHECK(GetMaterialTexture(material, MaterialTextureSlot::BaseColor) == "b.png");
    CHECK(GetMaterialTexture(material, MaterialTextureSlot::Normal) == "n.png");
    CHECK(GetMaterialTexture(material, MaterialTextureSlot::MetallicRoughness) == "mr.png");
    CHECK(GetMaterialTexture(material, MaterialTextureSlot::Occlusion) == "ao.png");
    CHECK(GetMaterialTexture(material, MaterialTextureSlot::Emissive) == "e.png");
    CHECK(MATERIAL_TEXTURE_SLOT_COUNT == 5);

    TempDir               dir;
    FS::FileSystemManager files;
    auto                  fs = std::make_unique<FS::FileSystem>();
    fs->RegisterPath("assets://", dir.Path());
    files.Register(std::move(fs));

    MaterialContainer container;
    container.CreateNewMaterial(files, "assets://Materials/New.material");
    REQUIRE(container.GetMaterialCount() == 1);
    CHECK_FALSE(container.GetMaterialDataByIndex(0).isDirty);
    CHECK(container.GetMaterialDataByIndex(0).baseColor == MaterialContainer::DEFAULT_CELL_TEXTURE);
    container.SetTexture(0, MaterialTextureSlot::Normal, "Textures/n.png");
    CHECK(container.GetMaterialDataByIndex(0).normalMap == "Textures/n.png");
    CHECK(container.GetMaterialDataByIndex(0).baseColor == MaterialContainer::DEFAULT_CELL_TEXTURE);
    CHECK(container.GetMaterialDataByIndex(0).isDirty);

    // Saved and read back through the file it was created at.
    container.SaveMaterial(0, files);
    MaterialData saved;
    MaterialSerializer::Deserialize(saved, "assets://Materials/New.material", files);
    CHECK(saved.path == "Materials/New.material");
    CHECK(saved.normalMap == "Textures/n.png");
}
