#include "api/SceneExtractor.hpp"
#include "Profiler/api/Profiler.hpp"

#include "api/RenderScene.hpp"

#include "ECS/api/ECS.hpp"
#include "HedgehogEngine/api/ECS/components/EnvironmentComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/EnvironmentSystem.hpp"
#include "HedgehogEngine/api/ECS/components/PostProcessComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/PostProcessSystem.hpp"

#include <algorithm>
#include <string_view>

namespace HX
{
    namespace
    {
        // Under assets:// unless it names a mount, with forward slashes.
        void NormalizeAssetPath(const std::string& path, std::string& out)
        {
            constexpr std::string_view ASSETS = "assets://";
            out.clear();
            if (!path.empty() && path.find("://") == std::string::npos)
                out.append(ASSETS);
            out.append(path);
            std::replace(out.begin(), out.end(), '\\', '/');
        }

        // The first enabled component of the view by entity id, found without sorting it.
        template <typename T>
        const T* FindFirstEnabled(const ECS::ECS& ecs, const ECS::System& view)
        {
            const T*    chosen       = nullptr;
            ECS::Entity chosenEntity = ECS::INVALID_ENTITY;
            for (const ECS::Entity entity : view.GetEntities())
            {
                const T& component = ecs.GetComponent<T>(entity);
                if (component.Enabled && (!chosen || entity < chosenEntity))
                {
                    chosen       = &component;
                    chosenEntity = entity;
                }
            }
            return chosen;
        }
    }

    void SceneExtractor::ExtractEnvironment(const ECS::ECS& ecs, const HedgehogEngine::EnvironmentSystem& environmentSystem,
                                            RenderScene& outScene) const
    {
        HH_PROFILE_ZONE("Extract: environment");
        RenderEnvironment& environment = outScene.Environment;
        environment.Present            = false;

        const auto* chosen = FindFirstEnabled<HedgehogEngine::EnvironmentComponent>(ecs, environmentSystem);
        if (!chosen)
            return;

        environment.Present         = true;
        environment.Intensity       = chosen->Intensity;
        environment.RotationDegrees = chosen->Rotation;
        environment.ShowSkybox      = chosen->ShowSkybox;
        environment.Exposure        = chosen->Exposure;

        NormalizeAssetPath(chosen->Map, environment.MapPath);
    }

    void SceneExtractor::ExtractPostProcess(const ECS::ECS& ecs, const HedgehogEngine::PostProcessSystem& postProcessSystem,
                                            RenderScene& outScene) const
    {
        RenderPostProcess& post = outScene.PostProcess;
        post.Present            = false;
        const auto* chosen      = FindFirstEnabled<HedgehogEngine::PostProcessComponent>(ecs, postProcessSystem);
        if (!chosen)
            return;
        post.Present              = true;
        post.Fxaa                 = chosen->AntiAliasing == HedgehogEngine::PostProcessAntiAliasing::Fxaa;
        post.SsaoEnabled          = chosen->SsaoEnabled;
        post.SsaoRadius           = chosen->SsaoRadius;
        post.SsaoIntensity        = chosen->SsaoIntensity;
        post.SsaoBias             = chosen->SsaoBias;
        post.BloomEnabled         = chosen->BloomEnabled;
        post.BloomIntensity       = chosen->BloomIntensity;
        post.BloomThreshold       = chosen->BloomThreshold;
        post.BloomKnee            = chosen->BloomKnee;
        post.ColorLutContribution = chosen->ColorLutContribution;
        NormalizeAssetPath(chosen->ColorLut, post.ColorLutPath);
    }
}
