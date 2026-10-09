#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/System.hpp"
#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace HedgehogEngine
{
    class ResourceCatalog;

    // The key a material path is listed by, FileSystem's MakeAssetKey: backslashes as slashes,
    // "assets://" dropped (materials live under it, and the container adds it back), and "." segments
    // and repeated slashes folded, so "Materials\a.material", "assets://Materials/a.material" and
    // "Materials/./a.material" are one material. Case is kept.
    [[nodiscard]] HEDGEHOG_ENGINE_API std::string NormalizeMaterialPath(std::string_view path);

    // Lists the material files RenderComponents name, by NormalizeMaterialPath, so one file loads
    // once however a component spells it; the components keep their own text. In the Sync phase it has the ResourceCatalog
    // service (found in OnRegister; nothing without one) load the ones listed since the last frame.
    class RenderSystem : public ECS::System
    {
    public:
        HEDGEHOG_ENGINE_API void OnRegister(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnUnregister(ECS::ECS& ecs) override;
        [[nodiscard]] HEDGEHOG_ENGINE_API ECS::SystemPhase GetPhase() const override;
        HEDGEHOG_ENGINE_API void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx) override;

        HEDGEHOG_ENGINE_API void   Update(ECS::ECS& ecs, ECS::Entity entity);
        HEDGEHOG_ENGINE_API void   UpdateSystem(ECS::ECS& ecs);
        HEDGEHOG_ENGINE_API size_t GetMaterialsCount() const;
        HEDGEHOG_ENGINE_API const std::vector<std::string>&  GetMaterials() const;
        HEDGEHOG_ENGINE_API RenderComponent& GetRenderComponentByIndex(ECS::ECS& ecs, size_t index) const;
        HEDGEHOG_ENGINE_API const std::vector<ECS::Entity>&  GetEntities() const;

    private:
        void UpdateMaterialPath(ECS::ECS& ecs, ECS::Entity entity);

    private:
        std::vector<std::string> m_MaterialPaths;
        ResourceCatalog*         m_Catalog = nullptr;
    };
}
