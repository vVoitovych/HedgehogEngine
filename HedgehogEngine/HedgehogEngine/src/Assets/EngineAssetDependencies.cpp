#include "HedgehogEngine/api/Assets/EngineAssetDependencies.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"

#include "HedgehogEngine/api/ECS/components/AudioSourceComponent.hpp"
#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiImageComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiTextComponent.hpp"

#include "HedgehogCommon/api/EngineRenderAssets.hpp"

#include "EcsSerialization/api/Assets/AssetDependencyCollector.hpp"

#include "yaml-cpp/yaml.h"

#include <string_view>

namespace HedgehogEngine
{
    namespace
    {
        // HedgehogScripting's base class, which every script runs on (the engine cannot include it).
        constexpr std::string_view BASE_SCRIPT_PATH = "engine://Content/Scripts/Base/ActorScript.lua";
        constexpr std::string_view GRAPH_EXTENSION  = ".graph";

        using EcsSerialization::NormalizeAssetPath;
        using EcsSerialization::ResolveRelativeAssetPath;

        void ReadScript(const YAML::Node& component, std::vector<std::string>& out)
        {
            if (const YAML::Node file = component["ScriptFile"]; file && !file.as<std::string>().empty())
            {
                out.push_back(NormalizeAssetPath(file.as<std::string>()));
                out.emplace_back(BASE_SCRIPT_PATH);
            }
            const YAML::Node properties = component["ScriptProperties"];
            if (!properties || !properties.IsMap())
                return;
            for (const auto& entry : properties)
            {
                const YAML::Node type  = entry.second["Type"];
                const YAML::Node value = entry.second["Value"];
                if (type && value && type.as<std::string>() == "AssetRef" && !value.as<std::string>().empty())
                    out.push_back(NormalizeAssetPath(value.as<std::string>()));
            }
        }

        std::string FollowMaterial(const std::string&, const std::string& text, std::vector<std::string>& out)
        {
            const YAML::Node material = YAML::Load(text);
            if (const YAML::Node baseColor = material["BaseColor"]; baseColor && !baseColor.as<std::string>().empty())
                out.push_back(NormalizeAssetPath(baseColor.as<std::string>()));
            return {};
        }

        void AppendPassTypeShaders(std::string_view passType, std::vector<std::string>& out)
        {
            for (const PassTypeShader& entry : PASS_TYPE_SHADERS)
                if (entry.PassType == passType && !entry.Shader.empty())
                    out.emplace_back(entry.Shader);
        }

        std::string FollowGraph(const std::string&, const std::string& text, std::vector<std::string>& out)
        {
            const YAML::Node graph = YAML::Load(text);
            if (const YAML::Node passes = graph["passes"]; passes && passes.IsSequence())
                for (const YAML::Node& pass : passes)
                    if (const YAML::Node type = pass["type"])
                        AppendPassTypeShaders(type.as<std::string>(), out);
            // A graph imports only what the shared phase renders: the shadow atlas.
            if (const YAML::Node imports = graph["imports"]; imports && imports.IsSequence() && imports.size() > 0)
                AppendPassTypeShaders(SHADOW_PASS_TYPE, out);
            return {};
        }

        std::string FollowShader(const std::string& path, const std::string& text, std::vector<std::string>& out)
        {
            const YAML::Node shader = YAML::Load(text);
            for (const char* key : { "pipeline_layout", "vertex_description" })
                if (const YAML::Node file = shader[key])
                    out.push_back(ResolveRelativeAssetPath(path, file.as<std::string>()));
            if (const YAML::Node stages = shader["shaders"]; stages && stages.IsSequence())
                for (const YAML::Node& stage : stages)
                    if (const YAML::Node file = stage["path"])
                        out.push_back(ResolveRelativeAssetPath(path, file.as<std::string>()));
            return {};
        }

        std::string FollowGltf(const std::string& path, const std::string& text, std::vector<std::string>& out)
        {
            const YAML::Node gltf = YAML::Load(text);
            for (const char* key : { "buffers", "images" })
            {
                const YAML::Node list = gltf[key];
                if (!list || !list.IsSequence())
                    continue;
                for (const YAML::Node& entry : list)
                    if (const YAML::Node uri = entry["uri"]; uri && !uri.as<std::string>().starts_with("data:"))
                        out.push_back(ResolveRelativeAssetPath(path, uri.as<std::string>()));
            }
            return {};
        }
    }

    std::vector<EngineRuntimeAsset> GetEngineRuntimeAssets()
    {
        // MeshSystem's default meshes and MaterialContainer's default texture are loaded at start.
        std::vector<EngineRuntimeAsset> assets = {
            { HedgehogSettings::ProjectSettings::PATH, true },
            // The engine root's marker: a package holding it is its own engine root, even when
            // it sits inside the repository (Build/<name>), whose marker is further up.
            { "engine://Engine.yaml", false },
            { HedgehogSettings::Settings::PATH, false },
            { EngineContext::INPUT_ACTIONS_PATH, false },
            { MeshSystem::sDefaultMeshPath, true },
            { MeshSystem::sDefaultSpherePath, true },
            { MaterialContainer::DEFAULT_CELL_TEXTURE, true },
        };
        for (const std::string_view graph : SHIPPED_GRAPHS)
            assets.push_back({ NormalizeGraphReference(std::string(graph)), true });
        return assets;
    }

    std::string NormalizeGraphReference(const std::string& graphName)
    {
        const bool isName = !graphName.empty() && graphName.find_first_of("/\\:") == std::string::npos &&
                            !graphName.ends_with(GRAPH_EXTENSION);
        if (isName)
            return std::string(ENGINE_GRAPH_DIRECTORY) + "/" + graphName + std::string(GRAPH_EXTENSION);
        return NormalizeAssetPath(graphName);
    }

    void RegisterEngineAssetDependencies(EcsSerialization::AssetDependencyCollector& collector)
    {
        collector.AddReflectedComponent<MeshComponent>("MeshComponent");
        collector.AddReflectedComponent<RenderComponent>("RenderComponent");
        collector.AddReflectedComponent<AudioSourceComponent>("AudioSourceComponent");
        collector.AddReflectedComponent<UiImageComponent>("UiImageComponent");
        collector.AddReflectedComponent<UiTextComponent>("UiTextComponent");
        collector.AddReflectedComponent<CameraComponent>("CameraComponent", NormalizeGraphReference);
        collector.AddComponentReader("ScriptComponent", ReadScript);

        collector.AddFollower(".material", FollowMaterial);
        collector.AddFollower(std::string(GRAPH_EXTENSION), FollowGraph);
        collector.AddFollower(".shader", FollowShader);
        collector.AddFollower(".gltf", FollowGltf);
    }
}
