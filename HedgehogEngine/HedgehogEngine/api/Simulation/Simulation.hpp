#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/Simulation/ISimulationSystem.hpp"
#include "HedgehogEngine/api/Simulation/SimulationClock.hpp"

#include <memory>
#include <vector>

namespace HedgehogEngine
{
    enum class SimulationState
    {
        Edit,    // gameplay does not run; the scene is being edited
        Playing,
        Paused   // playing, but frozen: no ticks, clock stopped
    };

    // Owns Play mode. Play snapshots the scene and starts the registered systems;
    // Tick runs them on a fixed-step clock while playing; Stop ends them and puts
    // the snapshot back, so nothing done during Play survives it.
    class Simulation
    {
    public:
        HEDGEHOG_ENGINE_API explicit Simulation(SceneManager& sceneManager);

        Simulation(const Simulation&)            = delete;
        Simulation& operator=(const Simulation&) = delete;

        // Systems run in the order they were added. A system added while playing
        // or paused gets its OnPlayStart at once.
        HEDGEHOG_ENGINE_API void AddSystem(std::shared_ptr<ISimulationSystem> system);

        // From Edit: snapshot the scene, reset the clock, start every system.
        // From Paused: same as Resume. While playing: nothing.
        HEDGEHOG_ENGINE_API void Play();
        HEDGEHOG_ENGINE_API void Pause();
        HEDGEHOG_ENGINE_API void Resume();
        // From Playing or Paused: stop every system, then restore the snapshot.
        HEDGEHOG_ENGINE_API void Stop();

        // While playing: the clock's fixed steps of FixedUpdate, then one Update.
        // In Edit or Paused: nothing.
        HEDGEHOG_ENGINE_API void Tick(float deltaTime);

        HEDGEHOG_ENGINE_API SimulationState GetState() const;
        HEDGEHOG_ENGINE_API bool            IsEditing() const;
        // The last Tick's leftover time as a fraction of a fixed step, for interpolation.
        HEDGEHOG_ENGINE_API float           GetInterpolationAlpha() const;

        HEDGEHOG_ENGINE_API SimulationClock&       GetClock();
        HEDGEHOG_ENGINE_API const SimulationClock& GetClock() const;

    private:
        SceneManager&                                   m_SceneManager;
        std::vector<std::shared_ptr<ISimulationSystem>> m_Systems;
        SimulationClock                                 m_Clock;
        SceneSnapshot                                   m_Snapshot;
        SimulationState                                 m_State = SimulationState::Edit;
        float                                           m_InterpolationAlpha = 0.0f;
    };
}
