#include "CurveEditing.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Editor
{
    namespace
    {
        float ClampTime(float time)
        {
            return std::isfinite(time) ? std::clamp(time, 0.0f, 1.0f) : 0.0f;
        }

        // Puts keys[index] back in time order after its time changed; its new index. A key equal in
        // time to its neighbours stays where it was among them.
        template <typename Key>
        size_t Resort(std::vector<Key>& keys, size_t index)
        {
            while (index > 0 && keys[index].Time < keys[index - 1].Time)
            {
                std::swap(keys[index], keys[index - 1]);
                --index;
            }
            while (index + 1 < keys.size() && keys[index].Time > keys[index + 1].Time)
            {
                std::swap(keys[index], keys[index + 1]);
                ++index;
            }
            return index;
        }

        template <typename Key>
        std::optional<size_t> Insert(std::vector<Key>& keys, Key key)
        {
            if (keys.size() >= HedgehogEngine::MAX_CURVE_KEYS)
                return std::nullopt;
            keys.push_back(key);
            return Resort(keys, keys.size() - 1);
        }
    }

    std::optional<size_t> AddCurveKey(HedgehogEngine::ParticleCurve& curve, float time, float value)
    {
        return Insert(curve.Keys, HedgehogEngine::ParticleCurveKey{ ClampTime(time), std::isfinite(value) ? value : 0.0f });
    }

    size_t MoveCurveKey(HedgehogEngine::ParticleCurve& curve, size_t index, float time, float value)
    {
        if (index >= curve.Keys.size())
            return index;
        curve.Keys[index].Time = ClampTime(time);
        if (std::isfinite(value))
            curve.Keys[index].Value = value;
        return Resort(curve.Keys, index);
    }

    void DeleteCurveKey(HedgehogEngine::ParticleCurve& curve, size_t index)
    {
        if (index < curve.Keys.size())
            curve.Keys.erase(curve.Keys.begin() + static_cast<std::ptrdiff_t>(index));
    }

    HedgehogEngine::ParticleCurve MakeCurvePreset(CurvePreset preset)
    {
        HedgehogEngine::ParticleCurve curve;
        switch (preset)
        {
        case CurvePreset::Constant:
            break;
        case CurvePreset::FadeIn:
            curve.Keys = { { 0.0f, 0.0f }, { 1.0f, 1.0f } };
            break;
        case CurvePreset::FadeOut:
            curve.Keys = { { 0.0f, 1.0f }, { 1.0f, 0.0f } };
            break;
        case CurvePreset::EaseInOut:
            curve.Keys = { { 0.0f, 0.0f }, { 0.25f, 0.1f }, { 0.5f, 0.5f }, { 0.75f, 0.9f }, { 1.0f, 1.0f } };
            break;
        }
        return curve;
    }

    std::optional<size_t> FindCurveKey(const HedgehogEngine::ParticleCurve& curve, float time, float value, float radius,
                                       float valueScale)
    {
        std::optional<size_t> nearest;
        float                 best  = radius * radius;
        const float           scale = valueScale > 0.0f ? valueScale : 1.0f;
        for (size_t i = 0; i < curve.Keys.size(); ++i)
        {
            const float dt       = curve.Keys[i].Time - time;
            const float dv       = (curve.Keys[i].Value - value) / scale;
            const float distance = dt * dt + dv * dv;
            if (distance <= best)
            {
                best    = distance;
                nearest = i;
            }
        }
        return nearest;
    }

    std::optional<size_t> AddGradientKey(HedgehogEngine::ParticleGradient& gradient, float time)
    {
        const float clamped = ClampTime(time);
        return Insert(gradient.Keys,
                      HedgehogEngine::ParticleGradientKey{ clamped, HedgehogEngine::EvaluateGradient(gradient, clamped) });
    }

    size_t MoveGradientKey(HedgehogEngine::ParticleGradient& gradient, size_t index, float time)
    {
        if (index >= gradient.Keys.size())
            return index;
        gradient.Keys[index].Time = ClampTime(time);
        return Resort(gradient.Keys, index);
    }

    void DeleteGradientKey(HedgehogEngine::ParticleGradient& gradient, size_t index)
    {
        if (index < gradient.Keys.size())
            gradient.Keys.erase(gradient.Keys.begin() + static_cast<std::ptrdiff_t>(index));
    }
}
