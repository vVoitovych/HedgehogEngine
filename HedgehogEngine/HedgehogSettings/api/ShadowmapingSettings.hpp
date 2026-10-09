#pragma once

#include "HedgehogSettingsApi.hpp"

#include <cstdint>

namespace HedgehogSettings
{
    class ShadowmapSettings
    {
    public:
        HEDGEHOG_SETTINGS_API ShadowmapSettings();

        ~ShadowmapSettings() = default;
        ShadowmapSettings(const ShadowmapSettings&) = delete;
        ShadowmapSettings(ShadowmapSettings&&) = delete;
        ShadowmapSettings& operator=(const ShadowmapSettings&) = delete;
        ShadowmapSettings& operator=(ShadowmapSettings&&) = delete;

        HEDGEHOG_SETTINGS_API uint32_t GetShadowmapSize() const;
        HEDGEHOG_SETTINGS_API void SetShadowmapSize(uint32_t size);

        HEDGEHOG_SETTINGS_API uint32_t GetCascadesCount() const;
        HEDGEHOG_SETTINGS_API void SetCascadesCount(uint32_t cascadesCount);

        HEDGEHOG_SETTINGS_API float GetCascadeSplitLambda() const;
        HEDGEHOG_SETTINGS_API void SetCascadeSplitLambda(float val);

        HEDGEHOG_SETTINGS_API float GetSplit1() const;
        HEDGEHOG_SETTINGS_API void SetSplit1(float val);

        HEDGEHOG_SETTINGS_API float GetSplit2() const;
        HEDGEHOG_SETTINGS_API void SetSplit2(float val);

        HEDGEHOG_SETTINGS_API float GetSplit3() const;
        HEDGEHOG_SETTINGS_API void SetSplit3(float val);

        HEDGEHOG_SETTINGS_API void SetDefaultSplits();

        // The layers whose objects cast shadows (RENDERING.md section 2), as a bitmask over
        // LayerSettings' 32 layers. Independent of any view's layer mask: an object on a layer a
        // view hides still shadows what that view shows. Changing it rebuilds no GPU resource, so
        // it does not mark the settings dirty.
        HEDGEHOG_SETTINGS_API uint32_t GetShadowCasterMask() const;
        HEDGEHOG_SETTINGS_API void SetShadowCasterMask(uint32_t mask);

        // How the forward pass samples the sun's shadow atlas (engine_settings.yaml's shadowmap:
        // depth_bias, slope_bias, normal_offset, pcf_radius, cascade_blend). Each setter clamps to
        // its range and ignores a non-finite value; none rebuilds a GPU resource, so none marks the
        // settings dirty.

        // Subtracted from the receiver's depth, in the shadow map's [0, 1] depth.
        static constexpr float DEFAULT_DEPTH_BIAS = 0.0005f;
        static constexpr float MAX_DEPTH_BIAS     = 0.05f;
        HEDGEHOG_SETTINGS_API float GetDepthBias() const;
        HEDGEHOG_SETTINGS_API void SetDepthBias(float bias);

        // Added to the depth bias times the surface's slope to the light (tan of its angle).
        static constexpr float DEFAULT_SLOPE_BIAS = 0.002f;
        static constexpr float MAX_SLOPE_BIAS     = 0.1f;
        HEDGEHOG_SETTINGS_API float GetSlopeBias() const;
        HEDGEHOG_SETTINGS_API void SetSlopeBias(float bias);

        // How far the receiver is pushed along its normal before the lookup, in texels of its cascade.
        static constexpr float DEFAULT_NORMAL_OFFSET = 1.0f;
        static constexpr float MAX_NORMAL_OFFSET     = 10.0f;
        HEDGEHOG_SETTINGS_API float GetNormalOffset() const;
        HEDGEHOG_SETTINGS_API void SetNormalOffset(float texels);

        // Percentage-closer filtering taps on each side of the lookup: (2r + 1)^2 taps, 0 for one.
        static constexpr uint32_t DEFAULT_PCF_RADIUS = 1;
        static constexpr uint32_t MAX_PCF_RADIUS     = 3;
        HEDGEHOG_SETTINGS_API uint32_t GetPcfRadius() const;
        HEDGEHOG_SETTINGS_API void SetPcfRadius(uint32_t radius);

        // The fraction at the far end of each cascade blended into the next, 0 for a hard switch.
        static constexpr float DEFAULT_CASCADE_BLEND = 0.1f;
        static constexpr float MAX_CASCADE_BLEND     = 0.5f;
        HEDGEHOG_SETTINGS_API float GetCascadeBlend() const;
        HEDGEHOG_SETTINGS_API void SetCascadeBlend(float fraction);

        HEDGEHOG_SETTINGS_API bool IsDirty() const;
        HEDGEHOG_SETTINGS_API void CleanDirtyState();

    private:
        uint32_t m_ShadowmapSize = 2048;
        uint32_t m_CascadesCount = 4;

        float m_CascadeSplitLambda = 0.95f;

        float m_Split1 = 10.0f;
        float m_Split2 = 25.0f;
        float m_Split3 = 50.0f;

        // Every layer until the editor reserves its own (gizmos), which must never cast.
        uint32_t m_ShadowCasterMask = 0xFFFFFFFFu;

        float    m_DepthBias    = DEFAULT_DEPTH_BIAS;
        float    m_SlopeBias    = DEFAULT_SLOPE_BIAS;
        float    m_NormalOffset = DEFAULT_NORMAL_OFFSET;
        uint32_t m_PcfRadius    = DEFAULT_PCF_RADIUS;
        float    m_CascadeBlend = DEFAULT_CASCADE_BLEND;

        bool m_IsDirty = false;
    };
}


