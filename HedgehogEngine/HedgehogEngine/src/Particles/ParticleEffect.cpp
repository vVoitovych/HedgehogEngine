#include "HedgehogEngine/api/Particles/ParticleEffect.hpp"

#include <cmath>

namespace HedgehogEngine
{
    namespace
    {
        // Where t falls among sorted keys: the key before and after it and how far between them.
        // Before the first key both are the first, after the last both are the last.
        template<typename Key>
        void FindSegment(const std::vector<Key>& keys, float t, size_t& before, size_t& after, float& weight)
        {
            weight = 0.0f;
            if (t <= keys.front().Time)
            {
                before = after = 0;
                return;
            }
            for (size_t i = 1; i < keys.size(); ++i)
            {
                if (t <= keys[i].Time)
                {
                    before = i - 1;
                    after  = i;
                    const float span = keys[i].Time - keys[i - 1].Time;
                    weight = span > 0.0f ? (t - keys[i - 1].Time) / span : 1.0f;
                    return;
                }
            }
            before = after = keys.size() - 1;
        }

        float SampleTime(size_t index, size_t count)
        {
            return count > 1 ? static_cast<float>(index) / static_cast<float>(count - 1) : 0.0f;
        }

        bool IsFinite(const HM::Vector3& v)
        {
            return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z());
        }
    }

    ParticleEffect MakeDefaultParticleEffect()
    {
        ParticleEffect effect;
        effect.Emitters.emplace_back();
        return effect;
    }

    float EvaluateCurve(const ParticleCurve& curve, float t)
    {
        if (curve.Keys.empty())
            return 1.0f;
        size_t before = 0, after = 0;
        float  weight = 0.0f;
        FindSegment(curve.Keys, t, before, after, weight);
        const float a = curve.Keys[before].Value;
        const float b = curve.Keys[after].Value;
        return weight == 0.0f ? a : a + (b - a) * weight;
    }

    HM::Vector4 EvaluateGradient(const ParticleGradient& gradient, float t)
    {
        if (gradient.Keys.empty())
            return { 1.0f, 1.0f, 1.0f, 1.0f };
        size_t before = 0, after = 0;
        float  weight = 0.0f;
        FindSegment(gradient.Keys, t, before, after, weight);
        const HM::Vector4& a = gradient.Keys[before].Color;
        const HM::Vector4& b = gradient.Keys[after].Color;
        if (weight == 0.0f)
            return a;
        return { a.x() + (b.x() - a.x()) * weight, a.y() + (b.y() - a.y()) * weight,
                 a.z() + (b.z() - a.z()) * weight, a.w() + (b.w() - a.w()) * weight };
    }

    float SampleRange(const FloatRange& range, float u)
    {
        return range.Min + (range.Max - range.Min) * u;
    }

    void BakeCurve(const ParticleCurve& curve, std::span<float> out)
    {
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = EvaluateCurve(curve, SampleTime(i, out.size()));
    }

    void BakeGradient(const ParticleGradient& gradient, std::span<HM::Vector4> out)
    {
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = EvaluateGradient(gradient, SampleTime(i, out.size()));
    }

    std::vector<std::string> ValidateParticleEffect(const ParticleEffect& effect)
    {
        std::vector<std::string> problems;
        if (effect.Emitters.empty())
        {
            problems.emplace_back("The effect has no emitters.");
            return problems;
        }

        for (size_t index = 0; index < effect.Emitters.size(); ++index)
        {
            const ParticleEmitter& emitter = effect.Emitters[index];
            const auto report = [&](const std::string& what)
            {
                problems.push_back("Emitter '" + emitter.Name + "': " + what + ".");
            };
            const auto nonNegative = [&](float value, const char* name)
            {
                if (!std::isfinite(value) || value < 0.0f)
                    report(std::string(name) + " is not a number of at least 0");
            };
            const auto checkRange = [&](const FloatRange& range, const char* name, bool mustBeNonNegative)
            {
                if (!std::isfinite(range.Min) || !std::isfinite(range.Max) || range.Min > range.Max)
                    report(std::string(name) + " has Min above Max or is not a number");
                else if (mustBeNonNegative && range.Min < 0.0f)
                    report(std::string(name) + " is negative");
            };
            const auto checkKeys = [&](const auto& keys, const char* name)
            {
                if (keys.size() > MAX_CURVE_KEYS)
                    report(std::string(name) + " has more than " + std::to_string(MAX_CURVE_KEYS) + " keys");
                for (size_t k = 0; k < keys.size(); ++k)
                {
                    const float time = keys[k].Time;
                    if (!std::isfinite(time) || time < 0.0f || time > 1.0f)
                    {
                        report(std::string(name) + " has a key outside 0 to 1");
                        return;
                    }
                    if (k > 0 && time < keys[k - 1].Time)
                    {
                        report(std::string(name) + " has keys out of order");
                        return;
                    }
                }
            };

            if (emitter.MaxParticles == 0 || emitter.MaxParticles > MAX_PARTICLES_PER_EMITTER)
                report("MaxParticles is not 1 to " + std::to_string(MAX_PARTICLES_PER_EMITTER));
            nonNegative(emitter.Rate, "Rate");
            nonNegative(emitter.Duration, "Duration");
            nonNegative(emitter.Radius, "Radius");
            nonNegative(emitter.Drag, "Drag");
            nonNegative(emitter.SoftDistance, "SoftDistance");
            nonNegative(emitter.SheetFps, "SheetFps");
            if (!std::isfinite(emitter.Angle) || emitter.Angle < 0.0f || emitter.Angle > 180.0f)
                report("Angle is not 0 to 180 degrees");
            if (!IsFinite(emitter.BoxSize) || emitter.BoxSize.x() < 0.0f || emitter.BoxSize.y() < 0.0f
                || emitter.BoxSize.z() < 0.0f)
                report("BoxSize is negative or not a number");
            if (!IsFinite(emitter.Gravity))
                report("Gravity is not a number");

            for (const ParticleBurst& burst : emitter.Bursts)
            {
                if (!std::isfinite(burst.Time) || burst.Time < 0.0f)
                    report("a burst is at a negative time");
                if (burst.Cycles != 1 && (!std::isfinite(burst.Interval) || burst.Interval <= 0.0f))
                    report("a repeating burst has an interval that is not above 0");
            }

            checkRange(emitter.Lifetime, "Lifetime", true);
            checkRange(emitter.StartSpeed, "StartSpeed", false);
            checkRange(emitter.StartSize, "StartSize", true);
            checkRange(emitter.StartRotation, "StartRotation", false);
            checkKeys(emitter.SizeOverLife.Keys, "SizeOverLife");
            checkKeys(emitter.SpeedOverLife.Keys, "SpeedOverLife");
            checkKeys(emitter.ColorOverLife.Keys, "ColorOverLife");

            if (emitter.SheetTilesX == 0 || emitter.SheetTilesY == 0)
                report("the sheet has 0 tiles");

            const ParticleCollision& collision = emitter.Collision;
            if (!std::isfinite(collision.PlaneHeight))
                report("the collision plane height is not a number");
            if (!std::isfinite(collision.Bounce) || collision.Bounce < 0.0f || collision.Bounce > 1.0f)
                report("the collision bounce is not 0 to 1");
            if (!std::isfinite(collision.LifetimeLoss) || collision.LifetimeLoss < 0.0f || collision.LifetimeLoss > 1.0f)
                report("the collision lifetime loss is not 0 to 1");

            for (const ParticleSubEmitter& sub : emitter.SubEmitters)
            {
                if (sub.Emitter >= effect.Emitters.size())
                    report("a sub-emitter names emitter " + std::to_string(sub.Emitter) + ", which does not exist");
                else if (sub.Emitter == index)
                    report("a sub-emitter names its own emitter");
            }
        }

        // A chain of sub-emitters that comes back (a -> b -> a) would spawn forever.
        enum class Visit : uint8_t { New, Open, Done };
        std::vector<Visit> visits(effect.Emitters.size(), Visit::New);
        bool cycle = false;
        const auto walk = [&](const auto& self, size_t index) -> void
        {
            visits[index] = Visit::Open;
            for (const ParticleSubEmitter& sub : effect.Emitters[index].SubEmitters)
            {
                if (sub.Emitter >= effect.Emitters.size() || sub.Emitter == index)
                    continue; // reported above
                if (visits[sub.Emitter] == Visit::Open)
                    cycle = true;
                else if (visits[sub.Emitter] == Visit::New)
                    self(self, sub.Emitter);
            }
            visits[index] = Visit::Done;
        };
        for (size_t index = 0; index < effect.Emitters.size() && !cycle; ++index)
        {
            if (visits[index] == Visit::New)
                walk(walk, index);
        }
        if (cycle)
            problems.emplace_back("The effect's sub-emitters form a chain that comes back to an emitter.");

        return problems;
    }
}
