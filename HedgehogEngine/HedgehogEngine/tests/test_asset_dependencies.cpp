#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Assets/EngineAssetDependencies.hpp"
#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"
#include "HedgehogEngine/api/ECS/components/AudioSourceComponent.hpp"
#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiImageComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiTextComponent.hpp"

#include "EcsSerialization/api/Assets/AssetDependencyCollector.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using EcsSerialization::AssetDependencies;
using EcsSerialization::AssetDependencyCollector;
using namespace HedgehogEngine;

namespace
{
    // engine:// at the repository (the renderer's graphs and shaders), assets:// on a temp folder.
    struct ProjectFiles
    {
        TempDir               Dir;
        FS::FileSystemManager Files;

        ProjectFiles()
        {
            // tests/ -> HedgehogEngine/ -> HedgehogEngine/ -> repository root.
            const std::filesystem::path root =
                std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("engine://", root);
            fs->RegisterPath("assets://", Dir.Path());
            Files.Register(std::move(fs));
        }
    };

    template<typename T>
    std::vector<std::string> AssetRefNames()
    {
        std::vector<std::string> names;
        for (const Reflection::PropertyDescriptor& property : T::GetProperties())
            if (Reflection::HasFlag(property.flags, Reflection::PropertyFlags::AssetRef))
                names.emplace_back(property.name);
        return names;
    }

    constexpr const char* LEVEL = R"(Version: 1
Scene name: Level
Scene:
  - Entity: 0
    Name: Root
    Parent: 0
    Children:
      - Entity: 1
        Name: Crate
        Parent: 0
        MeshComponent:
          MeshPath: Models\Crate\Crate.gltf
        RenderComponent:
          Visible: true
          Material: Materials\Crate.material
        ScriptComponent:
          ScriptEnable: true
          ScriptFile: Scripts\Player.lua
          ScriptProperties:
            speed: { Type: Number, Value: 2 }
            hitSound: { Type: AssetRef, Value: Audio/hit.wav, AssetType: Audio }
            noSound: { Type: AssetRef, Value: "", AssetType: Audio }
        AudioSourceComponent:
          Clip: Audio/music.wav
        Children:
          - Entity: 2
            Name: Lid
            Parent: 1
            RenderComponent:
              Material: Materials/Missing.material
            Children: []
      - Entity: 3
        Name: MainCamera
        Parent: 0
        CameraComponent:
          Priority: 0
        Children: []
      - Entity: 4
        Name: MapCamera
        Parent: 0
        CameraComponent:
          GraphName: Graphs/Map.graph
        Children: []
      - Entity: 5
        Name: Hud
        Parent: 0
        UiImageComponent:
          Texture: Textures/logo.png
        UiTextComponent:
          Text: Score
          Font: Fonts/ui.ttf
        Children: []
)";
}

TEST_CASE("Asset dependencies - every asset-naming component property is an AssetRef")
{
    CHECK(AssetRefNames<MeshComponent>() == std::vector<std::string>{ "MeshPath" });
    CHECK(AssetRefNames<RenderComponent>() == std::vector<std::string>{ "Material" });
    CHECK(AssetRefNames<AudioSourceComponent>() == std::vector<std::string>{ "Clip" });
    CHECK(AssetRefNames<UiImageComponent>() == std::vector<std::string>{ "Texture" });
    CHECK(AssetRefNames<UiTextComponent>() == std::vector<std::string>{ "Font" });
    CHECK(AssetRefNames<CameraComponent>() == std::vector<std::string>{ "GraphName" });
    // An animator's clip names a clip inside its mesh, not a file.
    CHECK(AssetRefNames<AnimatorComponent>().empty());
}

TEST_CASE("Asset dependencies - a camera's graph reference names an engine graph or a file")
{
    CHECK(HedgehogEngine::NormalizeGraphReference("game") == "engine://HedgehogEngine/HedgehogRenderer/assets/Graphs/game.graph");
    CHECK(HedgehogEngine::NormalizeGraphReference("Graphs\\Map.graph") == "assets://Graphs/Map.graph");
    CHECK(HedgehogEngine::NormalizeGraphReference("engine://x/y.graph") == "engine://x/y.graph");
    CHECK(HedgehogEngine::NormalizeGraphReference("").empty());
}

TEST_CASE("Asset dependencies - a fixture scene's closure is exactly its meshes, materials, textures, scripts, graphs, "
          "pipelines and shaders")
{
    ProjectFiles project;
    project.Dir.WriteFile("Scenes/Level.yaml", LEVEL);
    project.Dir.WriteFile("Models/Crate/Crate.gltf", R"({ "asset": { "version": "2.0" },
        "buffers": [ { "uri": "Crate.bin", "byteLength": 4 }, { "uri": "data:application/octet-stream;base64,AAAA" } ],
        "images": [ { "uri": "crate_normal.png" }, { "bufferView": 0, "mimeType": "image/png" } ] })");
    project.Dir.WriteFile("Models/Crate/Crate.bin", "");
    project.Dir.WriteFile("Models/Crate/crate_normal.png", "");
    project.Dir.WriteFile("Materials/Crate.material", "Type: 0\nBaseColor: Textures\\crate.png\nTransparency: 1");
    project.Dir.WriteFile("Textures/crate.png", "");
    project.Dir.WriteFile("Textures/logo.png", "");
    project.Dir.WriteFile("Scripts/Player.lua", "");
    project.Dir.WriteFile("Scripts/Base/ActorScript.lua", "");
    project.Dir.WriteFile("Audio/hit.wav", "");
    project.Dir.WriteFile("Audio/music.wav", "");
    project.Dir.WriteFile("Fonts/ui.ttf", "");
    // A project's own graph: a depth prepass alone, with no shadow atlas import.
    project.Dir.WriteFile("Graphs/Map.graph", "version: 2\npasses:\n  - type: DepthPrepass\n    name: DepthPrepass\n");

    AssetDependencyCollector collector;
    HedgehogEngine::RegisterEngineAssetDependencies(collector);
    const AssetDependencies result = collector.CollectScene("assets://Scenes/Level.yaml", project.Files);

    const std::string renderer = "engine://HedgehogEngine/HedgehogRenderer/assets/";
    std::vector<std::string> expected = {
        "assets://Audio/hit.wav",
        "assets://Audio/music.wav",
        "assets://Fonts/ui.ttf",
        "assets://Graphs/Map.graph",
        "assets://Materials/Crate.material",
        "assets://Models/Crate/Crate.bin",
        "assets://Models/Crate/Crate.gltf",
        "assets://Models/Crate/crate_normal.png",
        "assets://Scenes/Level.yaml",
        "assets://Scripts/Base/ActorScript.lua",
        "assets://Scripts/Player.lua",
        "assets://Textures/crate.png",
        "assets://Textures/logo.png",
    };
    // The default camera's engine graph "game": depth prepass, forward and game UI, plus the
    // shadow pass for the shadow atlas it imports; each shader's layout, vertex description and
    // SPIR-V stages.
    for (const char* file : {
             "Graphs/game.graph",
             "Shaders/DepthPrepass.shader", "Shaders/DepthPrepassSkinned.shader", "Shaders/ShadowmapPass.shader",
             "Shaders/ShadowmapPassSkinned.shader", "Shaders/GraphForward.shader", "Shaders/GraphForwardSkinned.shader",
             "Shaders/GameUi.shader",
             "Pipelines/DepthPrepass.pl", "Pipelines/DepthPrepassSkinned.pl", "Pipelines/ShadowmapPass.pl",
             "Pipelines/GraphForward.pl", "Pipelines/GraphForwardSkinned.pl", "Pipelines/GameUi.pl",
             "VertexDescriptions/PositionOnly.vdes", "VertexDescriptions/SkinnedPositionOnly.vdes",
             "VertexDescriptions/FullMesh.vdes", "VertexDescriptions/SkinnedFullMesh.vdes", "VertexDescriptions/Ui.vdes",
             "Shaders/DepthPrepass/Base.vert.spv", "Shaders/DepthPrepass/Skinned.vert.spv",
             "Shaders/ShadowmapPass/Shadowmap.vert.spv", "Shaders/GraphForward/Base.vert.spv",
             "Shaders/GraphForward/Skinned.vert.spv", "Shaders/GraphForward/Base.frag.spv", "Shaders/GameUi/Quad.vert.spv",
             "Shaders/GameUi/Quad.frag.spv" })
        expected.push_back(renderer + file);
    std::sort(expected.begin(), expected.end());

    CHECK(result.Assets == expected);
    REQUIRE(result.Warnings.size() == 1);
    CHECK(result.Warnings[0] == "assets://Materials/Missing.material (referenced by assets://Scenes/Level.yaml) does not exist.");
}

TEST_CASE("Asset dependencies - every shipped scene's references exist")
{
    FS::FileSystemManager files;
    const std::filesystem::path root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
    auto fs = std::make_unique<FS::FileSystem>();
    fs->RegisterPath("engine://", root);
    fs->RegisterPath("assets://", root / "Assets");
    files.Register(std::move(fs));

    AssetDependencyCollector collector;
    HedgehogEngine::RegisterEngineAssetDependencies(collector);
    for (const char* scene : { "Default.yaml", "Animated.yaml", "Hud.yaml" })
    {
        CAPTURE(scene);
        const AssetDependencies result = collector.CollectScene(std::string("assets://Scenes/") + scene, files);
        CHECK(result.Warnings.empty());
        for (const std::string& warning : result.Warnings)
            MESSAGE(warning);
        CHECK(result.Assets.size() > 1);
    }
}
