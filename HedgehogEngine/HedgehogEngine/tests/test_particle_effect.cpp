#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Particles/ParticleEffect.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <string>

using namespace HedgehogEngine;

namespace
{
    ParticleCurve Curve(std::initializer_list<ParticleCurveKey> keys) { return ParticleCurve{ keys }; }

    // Whether the effect's problems are exactly one, mentioning text.
    bool OneProblem(const ParticleEffect& effect, const std::string& text)
    {
        const std::vector<std::string> problems = ValidateParticleEffect(effect);
        return problems.size() == 1 && problems[0].find(text) != std::string::npos;
    }

    // The default effect with its emitter changed by edit.
    template<typename Edit>
    ParticleEffect WithEmitter(Edit edit)
    {
        ParticleEffect effect = MakeDefaultParticleEffect();
        effect.Emitters[0].Name = "Smoke";
        edit(effect.Emitters[0]);
        return effect;
    }
}

TEST_CASE("Particle curves are exact at their keys, linear between and flat outside")
{
    const ParticleCurve curve = Curve({ { 0.0f, 1.0f }, { 0.5f, 3.0f }, { 1.0f, 2.0f } });
    CHECK(EvaluateCurve(curve, 0.0f) == 1.0f);
    CHECK(EvaluateCurve(curve, 0.5f) == 3.0f);
    CHECK(EvaluateCurve(curve, 1.0f) == 2.0f);
    CHECK(EvaluateCurve(curve, 0.25f) == doctest::Approx(2.0f));
    CHECK(EvaluateCurve(curve, 0.75f) == doctest::Approx(2.5f));
    CHECK(EvaluateCurve(curve, -1.0f) == 1.0f);
    CHECK(EvaluateCurve(curve, 2.0f) == 2.0f);

    // One key is a constant; none is 1.
    CHECK(EvaluateCurve(Curve({ { 0.3f, 4.0f } }), 0.9f) == 4.0f);
    CHECK(EvaluateCurve(ParticleCurve{}, 0.5f) == 1.0f);

    // Two keys at one time: a step.
    const ParticleCurve step = Curve({ { 0.0f, 0.0f }, { 0.5f, 0.0f }, { 0.5f, 1.0f }, { 1.0f, 1.0f } });
    CHECK(EvaluateCurve(step, 0.49f) == 0.0f);
    CHECK(EvaluateCurve(step, 0.51f) == 1.0f);
}

TEST_CASE("Particle gradients blend their colours as curves do, white without keys")
{
    ParticleGradient gradient;
    gradient.Keys = { { 0.0f, { 1.0f, 1.0f, 0.0f, 1.0f } }, { 1.0f, { 1.0f, 0.0f, 0.0f, 0.0f } } };
    const HM::Vector4 middle = EvaluateGradient(gradient, 0.5f);
    CHECK(middle.x() == doctest::Approx(1.0f));
    CHECK(middle.y() == doctest::Approx(0.5f));
    CHECK(middle.z() == doctest::Approx(0.0f));
    CHECK(middle.w() == doctest::Approx(0.5f));
    CHECK(EvaluateGradient(gradient, 0.0f) == gradient.Keys[0].Color);
    CHECK(EvaluateGradient(gradient, 1.5f) == gradient.Keys[1].Color);
    CHECK(EvaluateGradient(ParticleGradient{}, 0.3f) == HM::Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
}

TEST_CASE("Baking samples a curve and a gradient at evenly spaced times from 0 to 1")
{
    const ParticleCurve curve = Curve({ { 0.0f, 0.0f }, { 1.0f, 4.0f } });
    std::array<float, 5> row{};
    BakeCurve(curve, row);
    CHECK(row == std::array<float, 5>{ 0.0f, 1.0f, 2.0f, 3.0f, 4.0f });

    std::array<float, 1> single{};
    BakeCurve(curve, single);
    CHECK(single[0] == 0.0f);

    ParticleGradient gradient;
    gradient.Keys = { { 0.0f, { 0.0f, 0.0f, 0.0f, 1.0f } }, { 1.0f, { 1.0f, 1.0f, 1.0f, 1.0f } } };
    std::array<HM::Vector4, 3> colours{};
    BakeGradient(gradient, colours);
    CHECK(colours[0] == gradient.Keys[0].Color);
    CHECK(colours[1].x() == doctest::Approx(0.5f));
    CHECK(colours[2] == gradient.Keys[1].Color);

    CHECK(SampleRange({ 2.0f, 6.0f }, 0.0f) == 2.0f);
    CHECK(SampleRange({ 2.0f, 6.0f }, 0.25f) == 3.0f);
    CHECK(SampleRange({ 2.0f, 6.0f }, 1.0f) == 6.0f);
}

TEST_CASE("The default effect is one visible emitter and validates clean")
{
    const ParticleEffect effect = MakeDefaultParticleEffect();
    REQUIRE(effect.Emitters.size() == 1);
    const ParticleEmitter& emitter = effect.Emitters[0];
    CHECK(emitter.Rate > 0.0f);
    CHECK(emitter.StartSize.Min > 0.0f);
    CHECK(emitter.StartColor.w() == 1.0f);
    CHECK(emitter.Loop);
    CHECK(ValidateParticleEffect(effect).empty());
}

TEST_CASE("Validation reports each problem once, naming its emitter")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();

    CHECK(OneProblem(ParticleEffect{}, "no emitters"));
    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e) { e.MaxParticles = 0; }), "Emitter 'Smoke': MaxParticles"));
    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e) { e.MaxParticles = MAX_PARTICLES_PER_EMITTER + 1; }), "MaxParticles"));
    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e) { e.Rate = -1.0f; }), "Rate"));
    CHECK(OneProblem(WithEmitter([&](ParticleEmitter& e) { e.Duration = nan; }), "Duration"));
    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e) { e.Angle = 200.0f; }), "Angle"));
    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e) { e.BoxSize = { 1.0f, -1.0f, 1.0f }; }), "BoxSize"));
    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e) { e.Lifetime = { 2.0f, 1.0f }; }), "Lifetime has Min above Max"));
    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e) { e.StartSize = { -1.0f, 1.0f }; }), "StartSize is negative"));
    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e) { e.Bursts = { { -1.0f, 5, 1, 1.0f } }; }), "negative time"));
    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e) { e.Bursts = { { 0.0f, 5, 0, 0.0f } }; }), "interval"));
    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e) { e.SheetTilesY = 0; }), "0 tiles"));
    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e) { e.Collision.Bounce = 1.5f; }), "bounce"));
    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e) { e.Collision.LifetimeLoss = -0.1f; }), "lifetime loss"));

    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e)
        { e.SizeOverLife = Curve({ { 0.5f, 1.0f }, { 0.2f, 2.0f } }); }), "SizeOverLife has keys out of order"));
    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e) { e.SpeedOverLife = Curve({ { 1.5f, 1.0f } }); }),
                     "SpeedOverLife has a key outside 0 to 1"));
    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e)
        {
            e.ColorOverLife.Keys.resize(MAX_CURVE_KEYS + 1);
            for (size_t i = 0; i < e.ColorOverLife.Keys.size(); ++i)
                e.ColorOverLife.Keys[i].Time = static_cast<float>(i) / 10.0f;
        }), "ColorOverLife has more than 8 keys"));

    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e) { e.SubEmitters = { { 0, 1 } }; }), "its own emitter"));
    CHECK(OneProblem(WithEmitter([](ParticleEmitter& e) { e.SubEmitters = { { 3, 1 } }; }), "emitter 3, which does not exist"));

    // Several problems are each reported.
    const ParticleEffect broken = WithEmitter([](ParticleEmitter& e)
    {
        e.Rate        = -1.0f;
        e.SheetTilesX = 0;
    });
    CHECK(ValidateParticleEffect(broken).size() == 2);
}

TEST_CASE("Sub-emitters may chain but not come back")
{
    ParticleEffect effect;
    effect.Emitters.resize(3);
    effect.Emitters[0].Name        = "Rocket";
    effect.Emitters[1].Name        = "Burst";
    effect.Emitters[2].Name        = "Sparkle";
    effect.Emitters[0].SubEmitters = { { 1, 20 } };
    effect.Emitters[1].SubEmitters = { { 2, 1 } };
    CHECK(ValidateParticleEffect(effect).empty());

    effect.Emitters[2].SubEmitters = { { 0, 1 } };
    CHECK(OneProblem(effect, "comes back"));
}
