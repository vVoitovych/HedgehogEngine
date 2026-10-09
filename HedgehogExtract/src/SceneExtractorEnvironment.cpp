#include "api/SceneExtractor.hpp"

#include "api/RenderScene.hpp"

#include "ECS/api/ECS.hpp"
#include "HedgehogEngine/api/ECS/components/EnvironmentComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/EnvironmentSystem.hpp"

#include <algorithm>
#include <string_view>

namespace HX
{
    void SceneExtractor::ExtractEnvironment(const ECS::ECS& ecs, const HedgehogEngine::EnvironmentSystem& environmentSystem,
                                            RenderScene& outScene) const
    {
        RenderEnvironment& environment = outScene.Environment;
        environment.Present            = false;

        // The first enabled one by entity id, found without sorting the view.
        const HedgehogEngine::EnvironmentComponent* chosen       = nullptr;
        ECS::Entity                                 chosenEntity = ECS::INVALID_ENTITY;
        for (const ECS::Entity entity : environmentSystem.GetEntities())
        {
            const auto& component = ecs.GetComponent<HedgehogEngine::EnvironmentComponent>(entity);
            if (component.Enabled && (!chosen || entity < chosenEntity))
            {
                chosen       = &component;
                chosenEntity = entity;
            }
        }
        if (!chosen)
            return;

        environment.Present         = true;
        environment.Intensity       = chosen->Intensity;
        environment.RotationDegrees = chosen->Rotation;
        environment.ShowSkybox      = chosen->ShowSkybox;
        environment.Exposure        = chosen->Exposure;

        // Under assets:// unless it names a mount, with forward slashes.
        constexpr std::string_view ASSETS = "assets://";
        environment.MapPath.clear();
        if (!chosen->Map.empty() && chosen->Map.find("://") == std::string::npos)
            environment.MapPath.append(ASSETS);
        environment.MapPath.append(chosen->Map);
        std::replace(environment.MapPath.begin(), environment.MapPath.end(), '\\', '/');
    }
}
