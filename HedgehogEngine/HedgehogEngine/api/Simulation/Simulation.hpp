#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/Simulation/SimulationClock.hpp"

namespace ECS
{
    class ECS;
}

namespace HedgehogEngine
{
    enum class SimulationState
    {
        Edit,    // gameplay does not run; the scene is being edited
        Playing,
        Paused   // playing, but frozen: no ticks, clock stopped
    };

    // Owns Play mode. Gameplay is the ECS's own systems: Play snapshots the scene and
    // calls every registered system's OnPlayStart; Tick runs their OnFixedUpdate and
    // OnUpdate on a fixed-step clock while playing; Stop calls OnPlayStop and puts the
    // snapshot back, so nothing done during Play survives it. Systems are visited in
    // registration order (ECS::ForEachSystem), OnPlayStop in reverse.
    class Simulation
    {
    public:
        HEDGEHOG_ENGINE_API Simulation(ECS::ECS& ecs, SceneManager& sceneManager);

        Simulation(const Simulation&)            = delete;
        Simulation& operator=(const Simulation&) = delete;

        // From Edit: snapshot the scene, reset the clock, then OnPlayStart on every system.
        // From Paused: same as Resume. While playing: nothing.
        HEDGEHOG_ENGINE_API void Play();
        HEDGEHOG_ENGINE_API void Pause();
        HEDGEHOG_ENGINE_API void Resume();
        // From Playing or Paused: OnPlayStop on every system, then restore the snapshot.
        HEDGEHOG_ENGINE_API void Stop();

        // While playing: the clock's fixed steps of OnFixedUpdate, then one OnUpdate.
        // In Edit or Paused: nothing.
        HEDGEHOG_ENGINE_API void Tick(float deltaTime);

        HEDGEHOG_ENGINE_API SimulationState GetState() const;
        HEDGEHOG_ENGINE_API bool            IsEditing() const;
        // The last Tick's leftover time as a fraction of a fixed step, for interpolation.
        HEDGEHOG_ENGINE_API float           GetInterpolationAlpha() const;

        HEDGEHOG_ENGINE_API SimulationClock&       GetClock();
        HEDGEHOG_ENGINE_API const SimulationClock& GetClock() const;

    private:
        ECS::ECS&       m_ECS;
        SceneManager&   m_SceneManager;
        SimulationClock m_Clock;
        SceneSnapshot   m_Snapshot;
        SimulationState m_State              = SimulationState::Edit;
        float           m_InterpolationAlpha = 0.0f;
    };
}
