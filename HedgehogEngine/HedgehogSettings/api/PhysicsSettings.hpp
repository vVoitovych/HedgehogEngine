#pragma once

#include "HedgehogSettingsApi.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace HedgehogSettings
{
    // The physics world's configuration (epic HE-304), engine_settings.yaml's physics section. It
    // lives beside the render layers and the shadow map rather than in Project.yaml because it is the
    // game's runtime configuration, shipped with it and read before Play; Project.yaml is the
    // project's identity and packaging. Plain data: HedgehogSettings knows nothing of HedgehogPhysics,
    // and the engine turns these values into the physics world's desc.
    struct PhysicsSettings
    {
        // As many as HedgehogPhysics' layers.
        static constexpr uint32_t LAYER_COUNT = 16;
        // WorkerThreads: the hardware's threads minus one. 0 runs the physics on the stepping thread.
        static constexpr int32_t AUTO_WORKER_THREADS = -1;
        static constexpr uint16_t ALL_LAYERS        = 0xffffu;

        // Metres per second squared; the engine is Z-up.
        std::array<float, 3> Gravity = { 0.0f, 0.0f, -9.81f };
        // Labels only: bodies store a layer's index. Layer 0 is Default, the rest unnamed.
        std::array<std::string, LAYER_COUNT> LayerNames = { "Default" };
        // Row i holds the layers layer i collides with, bit j for layer j; kept symmetric.
        std::array<uint16_t, LAYER_COUNT> CollisionMasks = MakeFullMasks();
        int32_t                           WorkerThreads  = AUTO_WORKER_THREADS;

        // Whether layers a and b collide; false for a layer out of range.
        [[nodiscard]] HEDGEHOG_SETTINGS_API bool Collides(uint32_t a, uint32_t b) const;
        // Sets both directions; a layer out of range changes nothing.
        HEDGEHOG_SETTINGS_API void SetCollides(uint32_t a, uint32_t b, bool collide);
        // The name, or "Layer <n>" for an unnamed one.
        [[nodiscard]] HEDGEHOG_SETTINGS_API std::string GetLayerDisplayName(uint32_t layer) const;

        bool operator==(const PhysicsSettings&) const = default;

    private:
        static constexpr std::array<uint16_t, LAYER_COUNT> MakeFullMasks()
        {
            std::array<uint16_t, LAYER_COUNT> masks{};
            masks.fill(ALL_LAYERS);
            return masks;
        }
    };
}
