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

        bool m_IsDirty = false;
    };
}


