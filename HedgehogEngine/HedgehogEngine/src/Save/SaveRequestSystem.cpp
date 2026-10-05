#include "SaveRequestSystem.hpp"

#include "HedgehogEngine/api/Save/SaveGameManager.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/FrameContext.hpp"

namespace HedgehogEngine
{
    void SaveRequestSystem::OnRegister(ECS::ECS& ecs)
    {
        m_SaveGames = ecs.GetServices().Find<SaveGameManager>();
    }

    void SaveRequestSystem::OnUnregister(ECS::ECS& /*ecs*/)
    {
        m_SaveGames = nullptr;
    }

    ECS::SystemPhase SaveRequestSystem::GetPhase() const
    {
        return ECS::SystemPhase::Simulation;
    }

    void SaveRequestSystem::OnFrame(ECS::ECS& /*ecs*/, const ECS::FrameContext& ctx)
    {
        if (m_SaveGames && ctx.Mode == ECS::PlayMode::Playing)
        {
            m_SaveGames->ProcessRequests();
        }
    }
}
