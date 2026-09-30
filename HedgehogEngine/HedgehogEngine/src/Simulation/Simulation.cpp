#include "HedgehogEngine/api/Simulation/Simulation.hpp"

#include "Logger/api/Logger.hpp"

#include <cassert>
#include <utility>

namespace HedgehogEngine
{
    Simulation::Simulation(SceneManager& sceneManager)
        : m_SceneManager(sceneManager)
    {
    }

    void Simulation::AddSystem(std::shared_ptr<ISimulationSystem> system)
    {
        assert(system && "Simulation::AddSystem: null system.");
        m_Systems.push_back(std::move(system));
        if (m_State != SimulationState::Edit)
            m_Systems.back()->OnPlayStart();
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

        for (const auto& system : m_Systems)
            system->OnPlayStart();
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

        for (auto it = m_Systems.rbegin(); it != m_Systems.rend(); ++it)
            (*it)->OnPlayStop();

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
        {
            for (const auto& system : m_Systems)
                system->FixedUpdate(fixedStep);
        }

        for (const auto& system : m_Systems)
            system->Update(deltaTime);

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
