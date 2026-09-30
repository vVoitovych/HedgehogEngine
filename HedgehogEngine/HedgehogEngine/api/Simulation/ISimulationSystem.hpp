#pragma once

namespace HedgehogEngine
{
    // Gameplay that runs only while the simulation plays. Register with
    // EngineContext::GetSimulation().AddSystem(...). Every hook defaults to doing nothing.
    class ISimulationSystem
    {
    public:
        virtual ~ISimulationSystem() = default;

        // Play from Edit: called in registration order, after the scene snapshot is taken.
        virtual void OnPlayStart() {}
        // Stop: called in reverse registration order, before the snapshot is restored.
        virtual void OnPlayStop() {}

        // Zero or more times per frame, once per fixed step of the simulation clock.
        virtual void FixedUpdate(float /*fixedDeltaTime*/) {}
        // Once per played frame, after that frame's fixed steps, with the real frame time.
        virtual void Update(float /*deltaTime*/) {}
    };
}
