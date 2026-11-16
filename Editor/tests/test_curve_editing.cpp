#include "doctest/doctest/doctest.h"

#include "Tools/ParticleEditor/CurveEditing.hpp"

#include <vector>

using namespace Editor;
using HedgehogEngine::ParticleCurve;
using HedgehogEngine::ParticleGradient;

namespace
{
    std::vector<float> Times(const ParticleCurve& curve)
    {
        std::vector<float> times;
        for (const auto& key : curve.Keys)
            times.push_back(key.Time);
        return times;
    }

    bool IsValidSizeCurve(const ParticleCurve& curve)
    {
        HedgehogEngine::ParticleEffect effect = HedgehogEngine::MakeDefaultParticleEffect();
        effect.Emitters[0].SizeOverLife       = curve;
        return HedgehogEngine::ValidateParticleEffect(effect).empty();
    }
}

TEST_CASE("Curve editing - keys go in time order, clamped to 0..1, at most eight")
{
    ParticleCurve curve;
    CHECK(AddCurveKey(curve, 0.5f, 2.0f) == 0u);
    CHECK(AddCurveKey(curve, 0.2f, 1.0f) == 0u);
    CHECK(AddCurveKey(curve, 3.0f, 0.0f) == 2u);  // clamped to 1
    CHECK(AddCurveKey(curve, -1.0f, 0.5f) == 0u); // clamped to 0
    CHECK(Times(curve) == std::vector<float>{ 0.0f, 0.2f, 0.5f, 1.0f });
    for (int i = 0; i < 4; ++i)
        CHECK(AddCurveKey(curve, 0.1f * static_cast<float>(i + 6), 1.0f).has_value());
    CHECK(curve.Keys.size() == HedgehogEngine::MAX_CURVE_KEYS);
    CHECK_FALSE(AddCurveKey(curve, 0.3f, 1.0f).has_value()); // a ninth is refused
    CHECK(IsValidSizeCurve(curve));
}

TEST_CASE("Curve editing - a key dragged past others stays in order, and the last key deleted leaves 1")
{
    ParticleCurve curve = MakeCurvePreset(CurvePreset::EaseInOut);
    const float   value = curve.Keys[1].Value;
    // The key at 0.25 dragged past 0.5 lands second, then past 0.75 third, its value moved with it.
    size_t index = MoveCurveKey(curve, 1, 0.6f, value);
    CHECK(index == 2u);
    index = MoveCurveKey(curve, index, 0.9f, 3.0f);
    CHECK(index == 3u);
    CHECK(curve.Keys[3].Value == 3.0f);
    CHECK(Times(curve) == std::vector<float>{ 0.0f, 0.5f, 0.75f, 0.9f, 1.0f });
    CHECK(MoveCurveKey(curve, 0, -5.0f, 0.0f) == 0u);
    CHECK(MoveCurveKey(curve, 9, 0.5f, 0.0f) == 9u); // out of range: nothing
    CHECK(IsValidSizeCurve(curve));

    while (!curve.Keys.empty())
        DeleteCurveKey(curve, 0);
    CHECK(HedgehogEngine::EvaluateCurve(curve, 0.3f) == 1.0f);
    CHECK(MakeCurvePreset(CurvePreset::Constant).Keys.empty());
    CHECK(HedgehogEngine::EvaluateCurve(MakeCurvePreset(CurvePreset::FadeOut), 0.25f) == doctest::Approx(0.75f));
}

TEST_CASE("Curve editing - the key under a point, its value axis scaled as drawn")
{
    const ParticleCurve curve = MakeCurvePreset(CurvePreset::FadeIn); // (0, 0) and (1, 1)
    CHECK(FindCurveKey(curve, 0.02f, 0.0f, 0.05f, 1.0f) == 0u);
    CHECK(FindCurveKey(curve, 0.98f, 1.0f, 0.05f, 1.0f) == 1u);
    CHECK_FALSE(FindCurveKey(curve, 0.5f, 0.5f, 0.05f, 1.0f).has_value());
    // On a range of 10, a value 0.3 away is 0.03 as drawn: within the radius.
    CHECK(FindCurveKey(curve, 1.0f, 1.3f, 0.05f, 10.0f) == 1u);
    CHECK_FALSE(FindCurveKey(curve, 1.0f, 1.3f, 0.05f, 1.0f).has_value());
}

TEST_CASE("Gradient editing - stops take the colour where they are added and stay in order")
{
    ParticleGradient gradient;
    gradient.Keys = { { 0.0f, { 1.0f, 0.0f, 0.0f, 1.0f } }, { 1.0f, { 0.0f, 0.0f, 1.0f, 0.0f } } };
    REQUIRE(AddGradientKey(gradient, 0.5f) == 1u);
    CHECK(gradient.Keys[1].Color.x() == doctest::Approx(0.5f));
    CHECK(gradient.Keys[1].Color.w() == doctest::Approx(0.5f));

    // The middle stop dragged to the end ties with the last and stays before it; dragged back past
    // the first, it lands first; then it is deleted.
    CHECK(MoveGradientKey(gradient, 1, 2.0f) == 1u);
    CHECK(gradient.Keys[1].Time == 1.0f);
    CHECK(gradient.Keys[2].Color.z() == 1.0f);
    gradient.Keys[0].Time = 0.1f;
    CHECK(MoveGradientKey(gradient, 1, 0.05f) == 0u);
    DeleteGradientKey(gradient, 0);
    CHECK(gradient.Keys.size() == 2u);
    CHECK(gradient.Keys[0].Color.x() == 1.0f);

    for (int i = 0; i < 10; ++i)
        AddGradientKey(gradient, 0.05f * static_cast<float>(i));
    CHECK(gradient.Keys.size() == HedgehogEngine::MAX_CURVE_KEYS);
}
