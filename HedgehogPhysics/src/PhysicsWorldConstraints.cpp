#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include "JoltConversions.hpp"
#include "JoltState.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>

#include "Logger/api/Logger.hpp"

#include <cmath>
#include <string>

namespace HP
{
    namespace
    {
        constexpr float DEGREES_TO_RADIANS = JPH::JPH_PI / 180.0f;

        bool IsFinite(const HM::Vector3& vector)
        {
            return std::isfinite(vector.x()) && std::isfinite(vector.y()) && std::isfinite(vector.z());
        }

        // Why desc cannot be made (its bodies aside), or empty.
        std::string CheckConstraintDesc(const ConstraintDesc& desc)
        {
            if (!IsFinite(desc.Anchor) || !IsFinite(desc.AnchorB))
                return "its anchor is not finite";
            const bool hasAxis = desc.Type == ConstraintType::Hinge || desc.Type == ConstraintType::Slider;
            if (hasAxis && (!IsFinite(desc.Axis) || !(desc.Axis.LengthSlow() > 0.0f)))
                return "its axis is zero or not finite";
            if (!std::isfinite(desc.SpringFrequency) || !std::isfinite(desc.SpringDamping) || desc.SpringFrequency < 0.0f ||
                desc.SpringDamping < 0.0f)
                return "its spring's frequency and damping must be finite and 0 or more";
            if (!desc.LimitsEnabled || desc.Type == ConstraintType::Fixed)
                return {};
            const float low = desc.MinLimit;
            const float high = desc.MaxLimit;
            if (!std::isfinite(low) || !std::isfinite(high) || low > high)
                return "its minimum limit is above its maximum";
            switch (desc.Type)
            {
            case ConstraintType::Hinge:
                if (low < -180.0f || low > 0.0f || high < 0.0f || high > 180.0f)
                    return "a hinge's limits must be -180 <= min <= 0 <= max <= 180 degrees";
                break;
            case ConstraintType::Slider:
                if (low > 0.0f || high < 0.0f)
                    return "a slider's limits must be min <= 0 <= max";
                break;
            case ConstraintType::Distance:
                if (low < 0.0f)
                    return "a distance's limits must be 0 or more";
                break;
            case ConstraintType::Fixed:
                break;
            }
            return {};
        }

        JPH::SpringSettings MakeSpring(const ConstraintDesc& desc)
        {
            return JPH::SpringSettings(JPH::ESpringMode::FrequencyAndDamping, desc.SpringFrequency, desc.SpringDamping);
        }

        JPH::Ref<JPH::TwoBodyConstraintSettings> MakeSettings(const ConstraintDesc& desc)
        {
            const JPH::RVec3 anchor(ToJolt(desc.Anchor));
            const JPH::Vec3  axis = ToJolt(desc.Axis).Normalized();
            switch (desc.Type)
            {
            case ConstraintType::Fixed:
            {
                auto settings              = new JPH::FixedConstraintSettings();
                settings->mAutoDetectPoint = true;
                return settings;
            }
            case ConstraintType::Hinge:
            {
                auto settings     = new JPH::HingeConstraintSettings();
                settings->mPoint1 = settings->mPoint2 = anchor;
                settings->mHingeAxis1 = settings->mHingeAxis2 = axis;
                settings->mNormalAxis1 = settings->mNormalAxis2 = axis.GetNormalizedPerpendicular();
                if (desc.LimitsEnabled)
                {
                    settings->mLimitsMin = desc.MinLimit * DEGREES_TO_RADIANS;
                    settings->mLimitsMax = desc.MaxLimit * DEGREES_TO_RADIANS;
                }
                settings->mLimitsSpringSettings = MakeSpring(desc);
                return settings;
            }
            case ConstraintType::Slider:
            {
                auto settings     = new JPH::SliderConstraintSettings();
                settings->mPoint1 = settings->mPoint2 = anchor;
                settings->mSliderAxis1 = settings->mSliderAxis2 = axis;
                settings->mNormalAxis1 = settings->mNormalAxis2 = axis.GetNormalizedPerpendicular();
                if (desc.LimitsEnabled)
                {
                    settings->mLimitsMin = desc.MinLimit;
                    settings->mLimitsMax = desc.MaxLimit;
                }
                settings->mLimitsSpringSettings = MakeSpring(desc);
                return settings;
            }
            case ConstraintType::Distance:
            {
                auto settings     = new JPH::DistanceConstraintSettings();
                settings->mPoint1 = anchor;
                settings->mPoint2 = JPH::RVec3(ToJolt(desc.AnchorB));
                if (desc.LimitsEnabled)
                {
                    settings->mMinDistance = desc.MinLimit;
                    settings->mMaxDistance = desc.MaxLimit;
                }
                settings->mLimitsSpringSettings = MakeSpring(desc);
                return settings;
            }
            }
            return nullptr;
        }
    }

    ConstraintHandle PhysicsWorld::CreateConstraint(const ConstraintDesc& desc)
    {
        std::string problem;
        if (!m_State)
            problem = "the world is not running";
        else if (!desc.BodyA.IsSet() && !desc.BodyB.IsSet())
            problem = "it joins no body";
        else if (desc.BodyA == desc.BodyB)
            problem = "it joins a body to itself";
        else if ((desc.BodyA.IsSet() && !IsValid(desc.BodyA)) || (desc.BodyB.IsSet() && !IsValid(desc.BodyB)))
            problem = "a body it joins is not a live body of this world";
        else
            problem = CheckConstraintDesc(desc);
        if (!problem.empty())
        {
            LOGERROR("[Physics] A constraint cannot be made:", problem + ".");
            return {};
        }

        JPH::BodyInterface&                            bodies   = m_State->System->GetBodyInterface();
        const JPH::Ref<JPH::TwoBodyConstraintSettings> settings = MakeSettings(desc);
        // An unset handle's id is invalid, which Jolt takes as the world.
        JPH::Ref<JPH::TwoBodyConstraint> constraint =
            bodies.CreateConstraint(settings, JPH::BodyID(desc.BodyA.Value), JPH::BodyID(desc.BodyB.Value));
        m_State->System->AddConstraint(constraint);
        bodies.ActivateConstraint(constraint);

        uint32_t index = 0;
        if (!m_State->FreeConstraints.empty())
        {
            index = m_State->FreeConstraints.back();
            m_State->FreeConstraints.pop_back();
        }
        else
        {
            index = static_cast<uint32_t>(m_State->Constraints.size());
            m_State->Constraints.emplace_back();
        }
        ConstraintSlot& slot = m_State->Constraints[index];
        slot.Constraint      = constraint;
        slot.Type            = desc.Type;
        slot.BodyA           = desc.BodyA.Value;
        slot.BodyB           = desc.BodyB.Value;
        return ConstraintHandle{ index, slot.Generation };
    }

    void PhysicsWorld::DestroyConstraint(ConstraintHandle constraint)
    {
        if (!IsValid(constraint))
            return;
        ConstraintSlot& slot = m_State->Constraints[constraint.Index];
        // Wakes what it held, so a body it kept up falls.
        m_State->System->GetBodyInterface().ActivateConstraint(slot.Constraint);
        m_State->System->RemoveConstraint(slot.Constraint);
        slot.Constraint = nullptr;
        ++slot.Generation;
        m_State->FreeConstraints.push_back(constraint.Index);
    }

    void PhysicsWorld::DestroyConstraintsOf(BodyHandle body)
    {
        if (!m_State)
            return;
        for (uint32_t index = 0; index < m_State->Constraints.size(); ++index)
        {
            const ConstraintSlot& slot = m_State->Constraints[index];
            if (slot.Constraint && (!body.IsSet() || slot.BodyA == body.Value || slot.BodyB == body.Value))
                DestroyConstraint(ConstraintHandle{ index, slot.Generation });
        }
    }

    bool PhysicsWorld::IsValid(ConstraintHandle constraint) const
    {
        return m_State && constraint.Index < m_State->Constraints.size() &&
               m_State->Constraints[constraint.Index].Constraint &&
               m_State->Constraints[constraint.Index].Generation == constraint.Generation;
    }

    size_t PhysicsWorld::GetConstraintCount() const
    {
        return m_State ? m_State->Constraints.size() - m_State->FreeConstraints.size() : 0;
    }

    float PhysicsWorld::GetConstraintAngle(ConstraintHandle constraint) const
    {
        if (!IsValid(constraint) || m_State->Constraints[constraint.Index].Type != ConstraintType::Hinge)
            return 0.0f;
        const auto& hinge = static_cast<const JPH::HingeConstraint&>(*m_State->Constraints[constraint.Index].Constraint);
        return hinge.GetCurrentAngle() / DEGREES_TO_RADIANS;
    }

    float PhysicsWorld::GetConstraintPosition(ConstraintHandle constraint) const
    {
        if (!IsValid(constraint))
            return 0.0f;
        const ConstraintSlot& slot = m_State->Constraints[constraint.Index];
        if (slot.Type == ConstraintType::Slider)
            return static_cast<const JPH::SliderConstraint&>(*slot.Constraint).GetCurrentPosition();
        if (slot.Type != ConstraintType::Distance)
            return 0.0f;
        // Each point is in its body's centre of mass space.
        const JPH::TwoBodyConstraint& joint  = *slot.Constraint;
        const JPH::RVec3              point1 = joint.GetBody1()->GetCenterOfMassTransform() *
                                  joint.GetConstraintToBody1Matrix().GetTranslation();
        const JPH::RVec3 point2 = joint.GetBody2()->GetCenterOfMassTransform() *
                                  joint.GetConstraintToBody2Matrix().GetTranslation();
        return static_cast<float>((point2 - point1).Length());
    }
}
