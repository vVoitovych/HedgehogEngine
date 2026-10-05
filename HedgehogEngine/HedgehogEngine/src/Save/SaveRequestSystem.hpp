#pragma once

#include "ECS/api/System.hpp"

namespace HedgehogEngine
{
    class SaveGameManager;

    // Processes the frame's save and load requests in the Simulation phase. The phase runs every
    // system's fixed steps and update before any OnFrame, so this runs after every script hook of
    // the frame whatever the registration order: a save sees the frame's state and a load never
    // runs under a script. Only while Playing; the SaveGameManager is found in OnRegister.
    class SaveRequestSystem : public ECS::System
    {
    public:
        void OnRegister(ECS::ECS& ecs) override;
        void OnUnregister(ECS::ECS& ecs) override;

        [[nodiscard]] ECS::SystemPhase GetPhase() const override;
        void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx) override;

    private:
        SaveGameManager* m_SaveGames = nullptr;
    };
}
