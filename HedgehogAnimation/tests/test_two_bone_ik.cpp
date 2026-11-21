#include "doctest/doctest/doctest.h"

#include "HedgehogAnimation/api/Pose.hpp"
#include "HedgehogAnimation/api/TwoBoneIk.hpp"

#include <cmath>
#include <vector>

using namespace HedgehogAnimation;

namespace
{
    // Base at the origin, then Shoulder, Elbow 1 m along +x, Hand 1 m further, bent 30 degrees
    // about +z at the elbow, so the arm bends towards +y.
    struct Arm
    {
        Skeleton                    Bones;
        std::vector<JointTransform> Local;
        std::vector<HM::Matrix4x4>  Model;

        Arm()
        {
            Bones.JointNames = { "Base", "Shoulder", "Elbow", "Hand" };
            Bones.Parents    = { -1, 0, 1, 2 };
            Bones.BindPose.resize(4);
            Bones.BindPose[1].Translation = HM::Vector3(0.0f, 0.0f, 0.5f);
            Bones.BindPose[2].Translation = HM::Vector3(1.0f, 0.0f, 0.0f);
            Bones.BindPose[2].Rotation    = HM::Quaternion::FromAxisAngle(HM::Vector3(0.0f, 0.0f, 1.0f), 30.0f);
            Bones.BindPose[3].Translation = HM::Vector3(1.0f, 0.0f, 0.0f);
            Bones.InverseBind.assign(4, HM::Matrix4x4::GetIdentity());
            Local = Bones.BindPose;
            ComputeModelPose(Bones, Local, Model);
        }

        HM::Vector3 At(uint32_t joint) const { return HM::Vector3(Model[joint][3].x(), Model[joint][3].y(), Model[joint][3].z()); }
        float       Distance(uint32_t a, uint32_t b) const { return (At(a) - At(b)).LengthSlow(); }

        void Solve(const HM::Vector3& target, const std::optional<HM::Vector3>& pole = std::nullopt, float weight = 1.0f)
        {
            SolveTwoBoneIk(Bones, *FindTwoBoneChain(Bones, 3), target, pole, weight, Local, Model);
        }

        // The model pose is the local pose's (nothing stale).
        void CheckConsistent()
        {
            std::vector<HM::Matrix4x4> fresh;
            ComputeModelPose(Bones, Local, fresh);
            for (size_t joint = 0; joint < fresh.size(); ++joint)
                for (size_t column = 0; column < 4; ++column)
                    for (size_t row = 0; row < 4; ++row)
                        CHECK(Model[joint][column][row] == doctest::Approx(fresh[joint][column][row]).epsilon(1e-4));
        }
    };

    bool Finite(const std::vector<JointTransform>& pose)
    {
        for (const JointTransform& joint : pose)
            if (!std::isfinite(joint.Rotation.x()) || !std::isfinite(joint.Rotation.y()) ||
                !std::isfinite(joint.Rotation.z()) || !std::isfinite(joint.Rotation.w()))
                return false;
        return true;
    }
}

TEST_CASE("TwoBoneIk - a chain needs two ancestors")
{
    const Arm arm;
    const std::optional<TwoBoneChain> chain = FindTwoBoneChain(arm.Bones, 3);
    REQUIRE(chain.has_value());
    CHECK(chain->Root == 1u);
    CHECK(chain->Mid == 2u);
    CHECK(chain->End == 3u);
    CHECK(FindTwoBoneChain(arm.Bones, 2).has_value());
    CHECK_FALSE(FindTwoBoneChain(arm.Bones, 1).has_value());
    CHECK_FALSE(FindTwoBoneChain(arm.Bones, 0).has_value());
    CHECK_FALSE(FindTwoBoneChain(arm.Bones, 9).has_value());
}

TEST_CASE("TwoBoneIk - reachable targets are reached within a millimetre, the bones keeping their lengths")
{
    for (const HM::Vector3 target : { HM::Vector3(1.2f, 0.4f, 0.5f), HM::Vector3(0.3f, -1.1f, 1.2f), HM::Vector3(-0.8f, 0.6f, 1.6f),
                                      HM::Vector3(0.5f, 0.0f, 0.5f) })
    {
        CAPTURE(target.x());
        CAPTURE(target.y());
        CAPTURE(target.z());
        Arm arm;
        arm.Solve(target);
        CHECK((arm.At(3) - target).LengthSlow() < 1e-3f);
        CHECK(arm.Distance(1, 2) == doctest::Approx(1.0f).epsilon(1e-4));
        CHECK(arm.Distance(2, 3) == doctest::Approx(1.0f).epsilon(1e-4));
        CHECK(arm.At(1).z() == doctest::Approx(0.5f)); // the root stays put
        arm.CheckConsistent();
    }
}

TEST_CASE("TwoBoneIk - an unreachable target straightens the chain towards it; one at the root makes no NaN")
{
    Arm arm;
    const HM::Vector3 far(10.0f, 0.0f, 0.5f);
    arm.Solve(far);
    CHECK(arm.Distance(1, 3) == doctest::Approx(2.0f).epsilon(1e-3));
    CHECK(arm.At(3).x() == doctest::Approx(2.0f).epsilon(1e-3));
    CHECK(std::abs(arm.At(3).y()) < 1e-3f);

    Arm atRoot;
    atRoot.Solve(HM::Vector3(0.0f, 0.0f, 0.5f));
    CHECK(Finite(atRoot.Local));
    CHECK(atRoot.Distance(1, 2) == doctest::Approx(1.0f).epsilon(1e-4));
}

TEST_CASE("TwoBoneIk - a pole on either side puts the bend on that side")
{
    const HM::Vector3 target(1.5f, 0.0f, 0.5f);
    for (const float side : { 1.0f, -1.0f })
    {
        CAPTURE(side);
        Arm arm;
        arm.Solve(target, HM::Vector3(0.75f, 0.0f, 0.5f + 2.0f * side));
        CHECK((arm.At(3) - target).LengthSlow() < 1e-3f);
        CHECK((arm.At(2).z() - 0.5f) * side > 0.1f); // the elbow points at the pole
        CHECK(std::abs(arm.At(2).y()) < 1e-3f);
    }
    // Without one, the present bend (towards +y) is kept.
    Arm arm;
    arm.Solve(target);
    CHECK(arm.At(2).y() > 0.1f);
}

TEST_CASE("TwoBoneIk - weight 0 changes nothing and 0.5 lands between")
{
    const HM::Vector3 target(0.3f, 1.2f, 0.9f);
    Arm untouched;
    const std::vector<JointTransform> before = untouched.Local;
    untouched.Solve(target, std::nullopt, 0.0f);
    CHECK(untouched.Local == before);

    Arm full;
    full.Solve(target);
    Arm half;
    half.Solve(target, std::nullopt, 0.5f);
    const Arm original;
    const float toFull     = (half.At(3) - full.At(3)).LengthSlow();
    const float toOriginal = (half.At(3) - original.At(3)).LengthSlow();
    CHECK(toFull > 0.05f);
    CHECK(toOriginal > 0.05f);
    CHECK((full.At(3) - original.At(3)).LengthSlow() > std::max(toFull, toOriginal));
    half.CheckConsistent();
}
