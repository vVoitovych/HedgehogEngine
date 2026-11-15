#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Particles/ParticleEffectFile.hpp"

#include <string>

using namespace HedgehogEngine;

namespace
{
    // Every field of two emitters off its default, one a sub-emitter of the other.
    const std::string FULL =
        "Version: 1\n"
        "Emitters:\n"
        "  - Name: Fire\n"
        "    MaxParticles: 500\n"
        "    Rate: 40\n"
        "    Bursts:\n"
        "      - {Time: 0, Count: 20, Cycles: 3, Interval: 0.5}\n"
        "      - {Time: 1.5, Count: 5}\n"
        "    Duration: 2\n"
        "    Loop: false\n"
        "    Shape: Cone\n"
        "    Radius: 0.25\n"
        "    Angle: 15\n"
        "    BoxSize: [2, 3, 4]\n"
        "    EmitFromSurface: true\n"
        "    Lifetime: [0.5, 1]\n"
        "    StartSpeed: [1, 2]\n"
        "    StartSize: [0.125, 0.5]\n"
        "    StartRotation: [-90, 90]\n"
        "    StartColor: [1, 0.5, 0.25, 1]\n"
        "    Gravity: [0, 0, 2]\n"
        "    Drag: 0.5\n"
        "    SizeOverLife: [[0, 0.5], [1, 2]]\n"
        "    SpeedOverLife: [[0, 1], [0.75, 0.25], [1, 0]]\n"
        "    ColorOverLife: [[0, 1, 1, 0, 1], [1, 1, 0, 0, 0]]\n"
        "    Texture: Textures/Particles/Fire.png\n"
        "    SheetTiles: [4, 2]\n"
        "    SheetFps: 12\n"
        "    Blend: Additive\n"
        "    SimulationSpace: Local\n"
        "    SoftDistance: 0.5\n"
        "    Collision: {Mode: HeightPlane, PlaneHeight: -1, Bounce: 0.25, LifetimeLoss: 0.125}\n"
        "    SubEmitters:\n"
        "      - {Emitter: 1, Count: 4}\n"
        "  - Name: Embers\n"
        "    Collision: {Mode: DepthBuffer}\n";

    // FULL with the line containing from replaced by to.
    std::string Replace(const std::string& from, const std::string& to)
    {
        std::string text = FULL;
        const size_t at  = text.find(from);
        REQUIRE(at != std::string::npos);
        return text.replace(at, from.size(), to);
    }

    // Whether text is refused with an error mentioning what.
    bool Refused(const std::string& text, const std::string& what)
    {
        const ParticleEffectReadResult result = ParseParticleEffect(text);
        const bool refused = !result.Effect && result.Error.find(what) != std::string::npos;
        if (!refused)
            MESSAGE("expected an error about '" << what << "', got '" << result.Error << "'");
        return refused;
    }
}

TEST_CASE("A .particle file with every field set reads back field by field and writes to exactly its text")
{
    const ParticleEffectReadResult result = ParseParticleEffect(FULL);
    REQUIRE_MESSAGE(result.Effect, result.Error);
    const ParticleEffect& effect = *result.Effect;
    REQUIRE(effect.Emitters.size() == 2);

    const ParticleEmitter& fire = effect.Emitters[0];
    CHECK(fire.Name == "Fire");
    CHECK(fire.MaxParticles == 500);
    CHECK(fire.Rate == 40.0f);
    REQUIRE(fire.Bursts.size() == 2);
    CHECK(fire.Bursts[0] == ParticleBurst{ 0.0f, 20, 3, 0.5f });
    CHECK(fire.Bursts[1] == ParticleBurst{ 1.5f, 5, 1, 1.0f });
    CHECK(fire.Duration == 2.0f);
    CHECK_FALSE(fire.Loop);
    CHECK(fire.Shape == ParticleShape::Cone);
    CHECK(fire.Radius == 0.25f);
    CHECK(fire.Angle == 15.0f);
    CHECK(fire.BoxSize == HM::Vector3{ 2.0f, 3.0f, 4.0f });
    CHECK(fire.EmitFromSurface);
    CHECK(fire.Lifetime == FloatRange{ 0.5f, 1.0f });
    CHECK(fire.StartSpeed == FloatRange{ 1.0f, 2.0f });
    CHECK(fire.StartSize == FloatRange{ 0.125f, 0.5f });
    CHECK(fire.StartRotation == FloatRange{ -90.0f, 90.0f });
    CHECK(fire.StartColor == HM::Vector4{ 1.0f, 0.5f, 0.25f, 1.0f });
    CHECK(fire.Gravity == HM::Vector3{ 0.0f, 0.0f, 2.0f });
    CHECK(fire.Drag == 0.5f);
    CHECK(fire.SizeOverLife.Keys == std::vector<ParticleCurveKey>{ { 0.0f, 0.5f }, { 1.0f, 2.0f } });
    CHECK(fire.SpeedOverLife.Keys.size() == 3);
    REQUIRE(fire.ColorOverLife.Keys.size() == 2);
    CHECK(fire.ColorOverLife.Keys[1].Color == HM::Vector4{ 1.0f, 0.0f, 0.0f, 0.0f });
    CHECK(fire.Texture == "Textures/Particles/Fire.png");
    CHECK(fire.SheetTilesX == 4);
    CHECK(fire.SheetTilesY == 2);
    CHECK(fire.SheetFps == 12.0f);
    CHECK(fire.Blend == ParticleBlend::Additive);
    CHECK(fire.SimulationSpace == ParticleSpace::Local);
    CHECK(fire.SoftDistance == 0.5f);
    CHECK(fire.Collision == ParticleCollision{ ParticleCollisionMode::HeightPlane, -1.0f, 0.25f, 0.125f });
    CHECK(fire.SubEmitters == std::vector<ParticleSubEmitter>{ { 1, 4 } });

    // The second emitter is all defaults but its collision mode.
    ParticleEmitter embers;
    embers.Name           = "Embers";
    embers.Collision.Mode = ParticleCollisionMode::DepthBuffer;
    CHECK(effect.Emitters[1] == embers);

    // Written back: exactly the text read, and read again: the same effect.
    const std::string written = WriteParticleEffect(effect);
    CHECK(written == FULL);
    const ParticleEffectReadResult again = ParseParticleEffect(written);
    REQUIRE(again.Effect);
    CHECK(*again.Effect == effect);
}

TEST_CASE("A bare emitter writes only its name, and the default effect reads back as itself")
{
    const std::string text = WriteParticleEffect(MakeDefaultParticleEffect());
    CHECK(text == "Version: 1\nEmitters:\n  - Name: Emitter\n");
    const ParticleEffectReadResult result = ParseParticleEffect(text);
    REQUIRE(result.Effect);
    CHECK(*result.Effect == MakeDefaultParticleEffect());
}

TEST_CASE("A .particle file with a mistake is refused with one message naming it")
{
    CHECK(Refused("Version: [1", "not valid YAML"));
    CHECK(Refused("- a\n", "not a map"));
    CHECK(Refused("Emitters: []\n", "Version is missing"));
    CHECK(Refused(Replace("Version: 1", "Version: 2"), "reads version 1, not 2"));
    CHECK(Refused("Version: 1\n", "Emitters is missing"));
    CHECK(Refused(Replace("Version: 1\n", "Version: 1\nExtra: 3\n"), "unknown key 'Extra'"));
    CHECK(Refused(Replace("    Rate: 40", "    Rates: 40"), "Emitters[0]: unknown key 'Rates'"));
    CHECK(Refused(Replace("  - Name: Embers\n", "  - Rate: 3\n"), "Emitters[1]: Name is missing"));
    CHECK(Refused(Replace("Rate: 40", "Rate: lots"), "Emitter 'Fire'.Rate: 'lots' is not a number"));
    CHECK(Refused(Replace("MaxParticles: 500", "MaxParticles: 2.5"), "MaxParticles: '2.5' is not a whole number"));
    CHECK(Refused(Replace("Loop: false", "Loop: maybe"), "Loop: 'maybe' is not true or false"));
    CHECK(Refused(Replace("Shape: Cone", "Shape: Torus"), "Shape: unknown value 'Torus' (Point, Sphere, Cone, Box)"));
    CHECK(Refused(Replace("Blend: Additive", "Blend: Multiply"), "Blend: unknown value 'Multiply'"));
    CHECK(Refused(Replace("BoxSize: [2, 3, 4]", "BoxSize: [2, 3]"), "BoxSize: expected a list of 3 numbers"));
    CHECK(Refused(Replace("Lifetime: [0.5, 1]", "Lifetime: [1, 0.5]"), "Lifetime: Min is above Max"));
    CHECK(Refused(Replace("[[0, 0.5], [1, 2]]", "[[0, 0.5, 9]]"), "SizeOverLife[0]: expected a list of 2 numbers"));
    CHECK(Refused(Replace("{Time: 1.5, Count: 5}", "{Time: 1.5, Count: 5, When: now}"), "Bursts[1]: unknown key 'When'"));
    CHECK(Refused(Replace("Mode: HeightPlane", "Mode: Sphere"), "Collision.Mode: unknown value 'Sphere'"));
    CHECK(Refused(Replace("{Emitter: 1, Count: 4}", "{Count: 4}"), "SubEmitters[0]: Emitter is missing"));
    // Read but invalid: ValidateParticleEffect's message.
    CHECK(Refused(Replace("{Emitter: 1, Count: 4}", "{Emitter: 0}"), "a sub-emitter names its own emitter"));
    CHECK(Refused(Replace("SheetTiles: [4, 2]", "SheetTiles: [0, 2]"), "the sheet has 0 tiles"));
}
