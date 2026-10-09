#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Assets/EngineAssetDependencies.hpp"
#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"
#include "HedgehogEngine/api/ECS/components/AudioSourceComponent.hpp"
#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "HedgehogEngine/api/ECS/components/EnvironmentComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiImageComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiTextComponent.hpp"

#include "EcsSerialization/api/Assets/AssetDependencyCollector.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"
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
      - Entity: 6
        Name: Sky
        Parent: 0
        EnvironmentComponent:
          Map: Environments/sky.hdr
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
    CHECK(AssetRefNames<EnvironmentComponent>() == std::vector<std::string>{ "Map" });
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
    // Every map of a PBR material.
    project.Dir.WriteFile("Materials/Crate.material",
                          "Type: 0\nBaseColor: Textures\\crate.png\nTransparency: 1\nMetallicRoughnessMap: Textures/crate_mr.png\n"
                          "NormalMap: Textures/crate_n.png\nOcclusionMap: Textures/crate_ao.png\nEmissiveMap: Textures/crate_e.png\n");
    project.Dir.WriteFile("Textures/crate.png", "");
    for (const char* map : { "Textures/crate_mr.png", "Textures/crate_n.png", "Textures/crate_ao.png", "Textures/crate_e.png" })
        project.Dir.WriteFile(map, "");
    project.Dir.WriteFile("Textures/logo.png", "");
    project.Dir.WriteFile("Scripts/Player.lua", "");
    project.Dir.WriteFile("Audio/hit.wav", "");
    project.Dir.WriteFile("Audio/music.wav", "");
    project.Dir.WriteFile("Fonts/ui.ttf", "");
    project.Dir.WriteFile("Environments/sky.hdr", "");
    // A project's own graph: a depth prepass alone, with no shadow atlas import.
    project.Dir.WriteFile("Graphs/Map.graph", "version: 2\npasses:\n  - type: DepthPrepass\n    name: DepthPrepass\n");

    AssetDependencyCollector collector;
    HedgehogEngine::RegisterEngineAssetDependencies(collector);
    const AssetDependencies result = collector.CollectScene("assets://Scenes/Level.yaml", project.Files);

    const std::string renderer = "engine://HedgehogEngine/HedgehogRenderer/assets/";
    std::vector<std::string> expected = {
        "assets://Audio/hit.wav",
        "assets://Audio/music.wav",
        "assets://Environments/sky.hdr",
        "assets://Fonts/ui.ttf",
        "assets://Graphs/Map.graph",
        "assets://Materials/Crate.material",
        "assets://Models/Crate/Crate.bin",
        "assets://Models/Crate/Crate.gltf",
        "assets://Models/Crate/crate_normal.png",
        "assets://Scenes/Level.yaml",
        // The base script every script runs on, in the engine's Content.
        "engine://Content/Scripts/Base/ActorScript.lua",
        "assets://Scripts/Player.lua",
        "assets://Textures/crate.png",
        "assets://Textures/crate_ao.png",
        "assets://Textures/crate_e.png",
        "assets://Textures/crate_mr.png",
        "assets://Textures/crate_n.png",
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
    fs->RegisterPath("assets://", root / FS::DEFAULT_PROJECT_DIRECTORY / "Assets");
    files.Register(std::move(fs));

    AssetDependencyCollector collector;
    HedgehogEngine::RegisterEngineAssetDependencies(collector);
    const auto scenes = files.ListDirectory("assets://Scenes");
    REQUIRE(scenes.has_value());
    for (const FS::DirectoryEntry& scene : *scenes)
    {
        CAPTURE(scene.Name);
        const AssetDependencies result = collector.CollectScene("assets://Scenes/" + scene.Name, files);
        CHECK(result.Warnings.empty());
        for (const std::string& warning : result.Warnings)
            MESSAGE(warning);
        CHECK(result.Assets.size() > 1);
    }

    // A prefab instance's prefab is reached, and through it what its entities use.
    const AssetDependencies prefabs = collector.CollectScene("assets://Scenes/Prefabs.yaml", files);
    for (const char* asset : { "assets://Prefabs/LampPost.prefab", "assets://Materials/test1.material",
                               "assets://Materials/test3.material", "engine://Content/Models/Default/sphere.obj" })
    {
        CAPTURE(asset);
        CHECK(std::find(prefabs.Assets.begin(), prefabs.Assets.end(), asset) != prefabs.Assets.end());
    }
}

TEST_CASE("Asset dependencies - the engine's required runtime assets are in its Content folder and exist")
{
    FS::FileSystemManager files;
    const std::filesystem::path root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
    auto fs = std::make_unique<FS::FileSystem>();
    fs->RegisterPath("engine://", root);
    files.Register(std::move(fs));

    for (const std::string& path : { HedgehogEngine::MeshSystem::sDefaultMeshPath, HedgehogEngine::MeshSystem::sDefaultSpherePath,
                                     std::string(HedgehogEngine::MaterialContainer::DEFAULT_CELL_TEXTURE) })
    {
        CAPTURE(path);
        CHECK(path.starts_with(FS::ENGINE_CONTENT_PREFIX));
        CHECK(files.Exists(path));
        const auto runtime = HedgehogEngine::GetEngineRuntimeAssets();
        CHECK(std::any_of(runtime.begin(), runtime.end(),
                          [&](const HedgehogEngine::EngineRuntimeAsset& asset) { return asset.Path == path && asset.Required; }));
    }
}
