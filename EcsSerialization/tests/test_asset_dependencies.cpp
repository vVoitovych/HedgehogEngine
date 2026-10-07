#include "doctest/doctest/doctest.h"

#include "EcsSerialization/api/Assets/AssetDependencyCollector.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "yaml-cpp/yaml.h"

#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <vector>

using EcsSerialization::AssetDependencies;
using EcsSerialization::AssetDependencyCollector;

namespace
{
    // assets:// on a fresh temp directory.
    struct AssetFiles
    {
        TempDir               Dir;
        FS::FileSystemManager Files;

        AssetFiles()
        {
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("assets://", Dir.Path());
            Files.Register(std::move(fs));
        }
    };

    // A reflected component written by hand: one AssetRef string with a default, one without, and
    // a plain string the collector must not read.
    struct TestPropComponent
    {
        std::string Model   = "Models/Default.obj";
        std::string Texture;
        std::string Label   = "Textures/not_an_asset.png";

        static void* ModelAccessor(void* c) { return &static_cast<TestPropComponent*>(c)->Model; }
        static void* TextureAccessor(void* c) { return &static_cast<TestPropComponent*>(c)->Texture; }
        static void* LabelAccessor(void* c) { return &static_cast<TestPropComponent*>(c)->Label; }

        static std::span<const Reflection::PropertyDescriptor> GetProperties()
        {
            static const Reflection::PropertyDescriptor properties[] = {
                { Reflection::TypeTag::String, "Model", ModelAccessor, Reflection::PropertyFlags::AssetRef, 0.0f, 0.0f },
                { Reflection::TypeTag::String, "Texture", TextureAccessor, Reflection::PropertyFlags::AssetRef, 0.0f, 0.0f },
                { Reflection::TypeTag::String, "Label", LabelAccessor, Reflection::PropertyFlags::None, 0.0f, 0.0f },
            };
            return properties;
        }
    };

    // A ".prefab" here is one asset path per line, standing in for a document that references others.
    std::string FollowLines(const std::string&, const std::string& text, std::vector<std::string>& out)
    {
        std::istringstream lines(text);
        for (std::string line; std::getline(lines, line);)
            if (!line.empty())
                out.push_back(EcsSerialization::NormalizeAssetPath(line));
        return {};
    }

    // A scene whose one entity holds `components`, with a child holding `childComponents`.
    std::string MakeScene(const std::string& components, const std::string& childComponents = {})
    {
        std::string scene = "Version: 1\nScene name: Test\nScene:\n  - Entity: 0\n    Name: Root\n    Parent: 0\n" + components;
        scene += "    Children:\n      - Entity: 1\n        Name: Child\n        Parent: 0\n" + childComponents;
        return scene + "        Children: []\n";
    }
}

TEST_CASE("Asset dependencies - paths are normalized under their mount")
{
    using EcsSerialization::NormalizeAssetPath;
    using EcsSerialization::ResolveRelativeAssetPath;
    CHECK(NormalizeAssetPath("Models\\viking_room.obj") == "assets://Models/viking_room.obj");
    CHECK(NormalizeAssetPath("assets://Models/./a/../b.obj") == "assets://Models/b.obj");
    CHECK(NormalizeAssetPath("engine://x\\y.graph") == "engine://x/y.graph");
    CHECK(NormalizeAssetPath("D:\\Graphs\\a.graph") == "D:/Graphs/a.graph");
    CHECK(NormalizeAssetPath("").empty());

    CHECK(ResolveRelativeAssetPath("engine://a/Shaders/F.shader", "../Pipelines/F.pl") == "engine://a/Pipelines/F.pl");
    CHECK(ResolveRelativeAssetPath("assets://Models/Crate/Crate.gltf", "crate.png") == "assets://Models/Crate/crate.png");
    CHECK(ResolveRelativeAssetPath("assets://Models/a.gltf", "assets://Textures/t.png") == "assets://Textures/t.png");
}

TEST_CASE("Asset dependencies - a reflected component's AssetRef properties are read, defaults included")
{
    AssetFiles files;
    files.Dir.WriteFile("Scenes/Test.yaml",
                        MakeScene("    TestPropComponent:\n      Texture: Textures\\wood.png\n",
                                  "        TestPropComponent:\n          Model: Models/Crate.obj\n          Texture: \"\"\n"));
    files.Dir.WriteFile("Models/Default.obj", "");
    files.Dir.WriteFile("Models/Crate.obj", "");
    files.Dir.WriteFile("Textures/wood.png", "");
    files.Dir.WriteFile("Textures/not_an_asset.png", "");

    AssetDependencyCollector collector;
    collector.AddReflectedComponent<TestPropComponent>("TestPropComponent");
    const AssetDependencies result = collector.CollectScene("assets://Scenes/Test.yaml", files.Files);

    // The root's Model is left out, so its default counts; the child's empty Texture names nothing.
    CHECK(result.Assets == std::vector<std::string>{ "assets://Models/Crate.obj", "assets://Models/Default.obj",
                                                     "assets://Scenes/Test.yaml", "assets://Textures/wood.png" });
    CHECK(result.Warnings.empty());
}

TEST_CASE("Asset dependencies - followers are followed, cycles end and missing files are warnings")
{
    AssetFiles files;
    files.Dir.WriteFile("Scenes/Test.yaml", MakeScene("    Spawner:\n      Prefab: Prefabs/A.prefab\n"));
    files.Dir.WriteFile("Prefabs/A.prefab", "Prefabs/B.prefab\nTextures/a.png\n");
    files.Dir.WriteFile("Prefabs/B.prefab", "Prefabs/A.prefab\nMeshes/missing.obj\nData/plain.bin\n");
    files.Dir.WriteFile("Textures/a.png", "");
    files.Dir.WriteFile("Data/plain.bin", "");

    AssetDependencyCollector collector;
    collector.AddComponentReader("Spawner", [](const YAML::Node& component, std::vector<std::string>& out)
                                 { out.push_back(EcsSerialization::NormalizeAssetPath(component["Prefab"].as<std::string>())); });
    collector.AddFollower(".PREFAB", FollowLines); // matched ignoring case
    const AssetDependencies result = collector.CollectScene("assets://Scenes/Test.yaml", files.Files);

    CHECK(result.Assets == std::vector<std::string>{ "assets://Data/plain.bin", "assets://Prefabs/A.prefab",
                                                     "assets://Prefabs/B.prefab", "assets://Scenes/Test.yaml",
                                                     "assets://Textures/a.png" });
    REQUIRE(result.Warnings.size() == 1);
    CHECK(result.Warnings[0] == "assets://Meshes/missing.obj (referenced by assets://Prefabs/B.prefab) does not exist.");
}

TEST_CASE("Asset dependencies - a prefab instance names its prefab, and a subtree document reads like a scene")
{
    AssetFiles files;
    files.Dir.WriteFile("Scenes/Street.yaml", "Scene name: Street\nScene:\n"
                                              "  - Entity: 0\n    Name: Root\n    Parent: 0\n    Children:\n"
                                              "      - Entity: 1\n        Name: Lamp\n        Parent: 0\n"
                                              "        Prefab: Prefabs\\Lamp.prefab\n        Entities: [1]\n"
                                              "        Children: []\n");
    files.Dir.WriteFile("Prefabs/Lamp.prefab", "");

    AssetDependencyCollector collector;
    const AssetDependencies  result = collector.CollectScene("assets://Scenes/Street.yaml", files.Files);
    CHECK(result.Assets == std::vector<std::string>{ "assets://Prefabs/Lamp.prefab", "assets://Scenes/Street.yaml" });
    CHECK(result.Warnings.empty());

    // A prefab's Root holds its entities under Subtree: read as a scene's are.
    collector.AddReflectedComponent<TestPropComponent>("TestPropComponent");
    std::vector<std::string> references;
    collector.ReadSceneReferences(YAML::Load("Subtree:\n  - Entity: 0\n    TestPropComponent:\n      Texture: a.png\n"
                                             "    Children:\n      - Entity: 1\n        Prefab: Prefabs/Inner.prefab\n"),
                                  references);
    CHECK(references == std::vector<std::string>{ "assets://Models/Default.obj", "assets://a.png",
                                                  "assets://Prefabs/Inner.prefab" });
}

TEST_CASE("Asset dependencies - an unreadable asset is a warning and the rest is still collected")
{
    AssetFiles files;
    files.Dir.WriteFile("Scenes/Test.yaml", MakeScene("    Spawner:\n      Prefab: Prefabs/Bad.prefab\n",
                                                      "        Spawner:\n          Prefab: Prefabs/Good.prefab\n"));
    files.Dir.WriteFile("Prefabs/Bad.prefab", "");
    files.Dir.WriteFile("Prefabs/Good.prefab", "");

    AssetDependencyCollector collector;
    collector.AddComponentReader("Spawner", [](const YAML::Node& component, std::vector<std::string>& out)
                                 { out.push_back(EcsSerialization::NormalizeAssetPath(component["Prefab"].as<std::string>())); });
    collector.AddFollower(".prefab", [](const std::string& path, const std::string&, std::vector<std::string>&)
                          { return path.ends_with("Bad.prefab") ? std::string("not a prefab") : std::string(); });
    const AssetDependencies result = collector.CollectScene("assets://Scenes/Test.yaml", files.Files);

    CHECK(result.Assets.size() == 3);
    REQUIRE(result.Warnings.size() == 1);
    CHECK(result.Warnings[0] == "assets://Prefabs/Bad.prefab (referenced by assets://Scenes/Test.yaml): not a prefab");

    // A scene that does not exist is a warning too, and collects nothing.
    const AssetDependencies missing = collector.CollectScene("Scenes/Nope.yaml", files.Files);
    CHECK(missing.Assets.empty());
    REQUIRE(missing.Warnings.size() == 1);
    CHECK(missing.Warnings[0] == "assets://Scenes/Nope.yaml does not exist.");
}
