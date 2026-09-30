#include "HedgehogEngine/api/Simulation/Simulation.hpp"

#include "ECS/api/ECS.hpp"

#include "Logger/api/Logger.hpp"

namespace HedgehogEngine
{
    Simulation::Simulation(ECS::ECS& ecs, SceneManager& sceneManager)
        : m_ECS(ecs)
        , m_SceneManager(sceneManager)
    {
    }

    void Simulation::Play()
    {
        if (m_State == SimulationState::Paused)
        {
            Resume();
            return;
        }
        if (m_State != SimulationState::Edit)
            return;

        m_Snapshot = m_SceneManager.CaptureSnapshot();
        m_Clock.Reset();
        m_Clock.SetPaused(false);
        m_InterpolationAlpha = 0.0f;
        m_State              = SimulationState::Playing;

        m_ECS.ForEachSystem([this](ECS::System& system) { system.OnPlayStart(m_ECS); });
    }

    void Simulation::Pause()
    {
        if (m_State != SimulationState::Playing)
            return;
        m_State = SimulationState::Paused;
        m_Clock.SetPaused(true);
    }

    void Simulation::Resume()
    {
        if (m_State != SimulationState::Paused)
            return;
        m_State = SimulationState::Playing;
        m_Clock.SetPaused(false);
    }

    void Simulation::Stop()
    {
        if (m_State == SimulationState::Edit)
            return;

        m_ECS.ForEachSystemReverse([this](ECS::System& system) { system.OnPlayStop(m_ECS); });

        m_State = SimulationState::Edit;
        m_Clock.SetPaused(false);
        m_InterpolationAlpha = 0.0f;

        if (!m_SceneManager.RestoreSnapshot(m_Snapshot))
            LOGERROR("Simulation::Stop: the scene captured on Play could not be restored.");
        m_Snapshot = {};
    }

    void Simulation::Tick(float deltaTime)
    {
        if (m_State != SimulationState::Playing)
            return;

        const SimulationSteps steps     = m_Clock.Advance(deltaTime);
        const float           fixedStep = m_Clock.GetFixedDeltaTime();
        for (uint32_t step = 0; step < steps.Count; ++step)
            m_ECS.ForEachSystem([&](ECS::System& system) { system.OnFixedUpdate(m_ECS, fixedStep); });

        m_ECS.ForEachSystem([&](ECS::System& system) { system.OnUpdate(m_ECS, deltaTime); });

        m_InterpolationAlpha = steps.Alpha;
    }

    SimulationState Simulation::GetState() const
    {
        return m_State;
    }

    bool Simulation::IsEditing() const
    {
        return m_State == SimulationState::Edit;
    }

    float Simulation::GetInterpolationAlpha() const
    {
        return m_InterpolationAlpha;
    }

    SimulationClock& Simulation::GetClock()
    {
        return m_Clock;
    }

    const SimulationClock& Simulation::GetClock() const
    {
        return m_Clock;
    }
}
