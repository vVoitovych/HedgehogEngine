#include "HedgehogSettings/api/ShadowmapingSettings.hpp"

#include <algorithm>
#include <cmath>

namespace HedgehogSettings
{
    namespace
    {
        // value clamped to [0, max], or current when value is not finite.
        float ClampedOr(float value, float max, float current)
        {
            return std::isfinite(value) ? std::clamp(value, 0.0f, max) : current;
        }
    }

    ShadowmapSettings::ShadowmapSettings()
    {
    }

    uint32_t ShadowmapSettings::GetShadowmapSize() const
    {
        return m_ShadowmapSize;
    }

    void ShadowmapSettings::SetShadowmapSize(uint32_t size)
    {
        uint32_t minSize = 1;
        uint32_t maxSize = 4096;

        m_ShadowmapSize = std::max(minSize, std::min(size, maxSize));
        m_IsDirty = true;
    }

    uint32_t ShadowmapSettings::GetCascadesCount() const
    {
        return m_CascadesCount;
    }

    void ShadowmapSettings::SetCascadesCount(uint32_t cascadesCount)
    {
        uint32_t minCascades = 1;
        uint32_t maxCascades = 4;

        m_CascadesCount = std::max(minCascades, std::min(cascadesCount, maxCascades));
    }

    float ShadowmapSettings::GetCascadeSplitLambda() const
    {
        return m_CascadeSplitLambda;
    }

    void ShadowmapSettings::SetCascadeSplitLambda(float val)
    {
        m_CascadeSplitLambda = val;
    }

    float ShadowmapSettings::GetSplit1() const
    {
        return m_Split1;
    }

    void ShadowmapSettings::SetSplit1(float val)
    {
        m_Split1 = std::max(0.0f, std::min(val, m_Split2 - 0.1f));
    }

    float ShadowmapSettings::GetSplit2() const
    {
        return m_Split2;
    }

    void ShadowmapSettings::SetSplit2(float val)
    {
        m_Split2 = std::max(m_Split1 + 0.1f, std::min(val, m_Split3 - 0.1f));
    }

    float ShadowmapSettings::GetSplit3() const
    {
        return m_Split3;
    }

    void ShadowmapSettings::SetSplit3(float val)
    {
        m_Split3 = std::max(m_Split2 + 0.1f, std::min(val, 100.0f));
    }

    void ShadowmapSettings::SetDefaultSplits()
    {
        if (m_CascadesCount == 1)
        {
            m_Split1 = 100.0f;
            m_Split2 = 100.0f;
            m_Split3 = 100.0f;
        }
        else if (m_CascadesCount == 2)
        {
            m_Split1 = 35.0f;
            m_Split2 = 100.0f;
            m_Split3 = 100.0f;
        }
        else if (m_CascadesCount == 3)
        {
            m_Split1 = 20.0f;
            m_Split2 = 60.0f;
            m_Split3 = 100.0f;
        }
        else if(m_CascadesCount == 4)
        {
            m_Split1 = 10.0f;
            m_Split2 = 25.0f;
            m_Split3 = 50.0f;
        }
    }

    uint32_t ShadowmapSettings::GetShadowCasterMask() const
    {
        return m_ShadowCasterMask;
    }

    void ShadowmapSettings::SetShadowCasterMask(uint32_t mask)
    {
        m_ShadowCasterMask = mask;
    }

    float ShadowmapSettings::GetDepthBias() const
    {
        return m_DepthBias;
    }

    void ShadowmapSettings::SetDepthBias(float bias)
    {
        m_DepthBias = ClampedOr(bias, MAX_DEPTH_BIAS, m_DepthBias);
    }

    float ShadowmapSettings::GetSlopeBias() const
    {
        return m_SlopeBias;
    }

    void ShadowmapSettings::SetSlopeBias(float bias)
    {
        m_SlopeBias = ClampedOr(bias, MAX_SLOPE_BIAS, m_SlopeBias);
    }

    float ShadowmapSettings::GetNormalOffset() const
    {
        return m_NormalOffset;
    }

    void ShadowmapSettings::SetNormalOffset(float texels)
    {
        m_NormalOffset = ClampedOr(texels, MAX_NORMAL_OFFSET, m_NormalOffset);
    }

    uint32_t ShadowmapSettings::GetPcfRadius() const
    {
        return m_PcfRadius;
    }

    void ShadowmapSettings::SetPcfRadius(uint32_t radius)
    {
        m_PcfRadius = std::min(radius, MAX_PCF_RADIUS);
    }

    float ShadowmapSettings::GetCascadeBlend() const
    {
        return m_CascadeBlend;
    }

    void ShadowmapSettings::SetCascadeBlend(float fraction)
    {
        m_CascadeBlend = ClampedOr(fraction, MAX_CASCADE_BLEND, m_CascadeBlend);
    }

    bool ShadowmapSettings::IsDirty() const
    {
        return m_IsDirty;
    }

    void ShadowmapSettings::CleanDirtyState()
    {
        m_IsDirty = false;
    }

}

