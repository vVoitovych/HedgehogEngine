#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include "JoltConversions.hpp"
#include "JoltLayers.hpp"
#include "JoltShapes.hpp"
#include "JoltState.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>

#include "Logger/api/Logger.hpp"

#include <cmath>
#include <string>

namespace HP
{
    namespace
    {
        const JPH::Vec3 UP = JPH::Vec3::sAxisZ();

        bool IsFinite(const HM::Vector3& vector)
        {
            return std::isfinite(vector.x()) && std::isfinite(vector.y()) && std::isfinite(vector.z());
        }

        bool IsPositive(float value) { return std::isfinite(value) && value > 0.0f; }
        bool IsNonNegative(float value) { return std::isfinite(value) && value >= 0.0f; }

        bool IsFinite(const BodyPose& pose)
        {
            return IsFinite(pose.Position) && std::isfinite(pose.Rotation.x()) && std::isfinite(pose.Rotation.y()) &&
                   std::isfinite(pose.Rotation.z()) && std::isfinite(pose.Rotation.w());
        }

        // Why desc cannot make a character, or empty.
        std::string CheckCharacterDesc(const CharacterDesc& desc)
        {
            if (!IsPositive(desc.Radius) || !IsNonNegative(desc.HalfHeight))
                return "its capsule needs a positive radius and a half height of 0 or more";
            if (!IsFinite(desc.Center))
                return "its capsule's centre is not finite";
            if (!IsFinite(desc.Pose))
                return "its pose is not finite";
            if (desc.Layer >= PHYSICS_LAYER_COUNT)
                return "its layer " + std::to_string(desc.Layer) + " is not below " +
                       std::to_string(PHYSICS_LAYER_COUNT);
            if (!IsPositive(desc.Mass))
                return "its mass must be positive";
            if (!std::isfinite(desc.MaxSlopeDegrees) || desc.MaxSlopeDegrees <= 0.0f || desc.MaxSlopeDegrees >= 90.0f)
                return "its steepest slope must be between 0 and 90 degrees";
            if (!IsNonNegative(desc.MaxStrength) || !IsNonNegative(desc.StepHeight) ||
                !IsNonNegative(desc.StickToFloorDistance) || !IsNonNegative(desc.PredictiveContactDistance))
                return "its strength, step height, stick-to-floor and contact distances must be 0 or more";
            return {};
        }

        CharacterGroundState FromJolt(JPH::CharacterBase::EGroundState state)
        {
            switch (state)
            {
            case JPH::CharacterBase::EGroundState::OnGround:      return CharacterGroundState::OnGround;
            case JPH::CharacterBase::EGroundState::OnSteepGround: return CharacterGroundState::OnSteepGround;
            case JPH::CharacterBase::EGroundState::NotSupported:  return CharacterGroundState::NotSupported;
            case JPH::CharacterBase::EGroundState::InAir:         return CharacterGroundState::InAir;
            }
            return CharacterGroundState::InAir;
        }
    }

    CharacterHandle PhysicsWorld::CreateCharacter(const CharacterDesc& desc)
    {
        if (!m_State)
        {
            LOGERROR("[Physics] A character cannot be made: the world is not running.");
            return {};
        }
        if (const std::string problem = CheckCharacterDesc(desc); !problem.empty())
        {
            LOGERROR("[Physics] A character cannot be made: " + problem + ".");
            return {};
        }

        ShapeDesc capsule;
        capsule.Type       = ShapeType::Capsule;
        capsule.Radius     = desc.Radius;
        capsule.HalfHeight = desc.HalfHeight;
        capsule.Center     = desc.Center;
        const JPH::Ref<JPH::Shape> shape = MakeShape(capsule);

        const JPH::ObjectLayer        layer = ToObjectLayer(desc.Layer, true);
        JPH::CharacterVirtualSettings settings;
        settings.mUp                        = UP;
        settings.mShape                     = shape;
        settings.mInnerBodyShape            = shape;
        settings.mInnerBodyLayer            = layer;
        settings.mMass                      = desc.Mass;
        settings.mMaxStrength               = desc.MaxStrength;
        settings.mMaxSlopeAngle             = JPH::DegreesToRadians(desc.MaxSlopeDegrees);
        settings.mPredictiveContactDistance = desc.PredictiveContactDistance;
        // Only contacts below the lower sphere's centre hold it up; the rest are walls.
        settings.mSupportingVolume = JPH::Plane(UP, desc.HalfHeight - desc.Center.z());

        JoltState& state = *m_State;
        uint32_t   index = 0;
        if (!state.FreeCharacters.empty())
        {
            index = state.FreeCharacters.back();
            state.FreeCharacters.pop_back();
        }
        else
        {
            index = static_cast<uint32_t>(state.Characters.size());
            state.Characters.emplace_back();
        }
        CharacterSlot& slot = state.Characters[index];
        slot.Character = new JPH::CharacterVirtual(&settings, JPH::RVec3(ToJolt(desc.Pose.Position)),
                                                   ToJolt(desc.Pose.Rotation), desc.UserData, state.System.get());
        slot.StepHeight           = desc.StepHeight;
        slot.StickToFloorDistance = desc.StickToFloorDistance;
        const JPH::BodyID inner   = slot.Character->GetInnerBodyID();
        if (!inner.IsInvalid())
            state.Records[inner.GetIndexAndSequenceNumber()] = BodyRecord{ desc.UserData, false };
        return CharacterHandle{ index, slot.Generation };
    }

    void PhysicsWorld::DestroyCharacter(CharacterHandle character)
    {
        if (!IsValid(character))
            return;
        CharacterSlot&    slot  = m_State->Characters[character.Index];
        const JPH::BodyID inner = slot.Character->GetInnerBodyID();
        slot.Character          = nullptr; // removes and destroys its inner body
        if (!inner.IsInvalid())
            m_State->Destroyed.push_back(inner.GetIndexAndSequenceNumber());
        ++slot.Generation;
        m_State->FreeCharacters.push_back(character.Index);
    }

    void PhysicsWorld::DestroyAllCharacters()
    {
        if (!m_State)
            return;
        for (uint32_t index = 0; index < m_State->Characters.size(); ++index)
            if (m_State->Characters[index].Character)
                DestroyCharacter(CharacterHandle{ index, m_State->Characters[index].Generation });
    }

    bool PhysicsWorld::IsValid(CharacterHandle character) const
    {
        return m_State && character.Index < m_State->Characters.size() &&
               m_State->Characters[character.Index].Character &&
               m_State->Characters[character.Index].Generation == character.Generation;
    }

    void PhysicsWorld::SetCharacterPose(CharacterHandle character, const BodyPose& pose)
    {
        if (!IsValid(character) || !IsFinite(pose))
            return;
        JPH::CharacterVirtual& ch    = *m_State->Characters[character.Index].Character;
        const JPH::ObjectLayer layer = m_State->System->GetBodyInterface().GetObjectLayer(ch.GetInnerBodyID());
        ch.SetPosition(JPH::RVec3(ToJolt(pose.Position)));
        ch.SetRotation(ToJolt(pose.Rotation));
        ch.RefreshContacts(m_State->System->GetDefaultBroadPhaseLayerFilter(layer),
                           m_State->System->GetDefaultLayerFilter(layer), JPH::BodyFilter(), JPH::ShapeFilter(),
                           *m_State->TempAllocator);
    }

    void PhysicsWorld::MoveCharacter(CharacterHandle character, const HM::Vector3& velocity,
                                     const HM::Vector3& gravity, float dt)
    {
        if (!IsValid(character) || !IsPositive(dt) || !IsFinite(velocity) || !IsFinite(gravity))
            return;
        CharacterSlot&         slot = m_State->Characters[character.Index];
        JPH::CharacterVirtual& ch   = *slot.Character;

        // Jolt's recipe: on the ground, carried by it (and a jump added); otherwise keeping the
        // vertical speed it had. Then gravity, and the walk, never further up a slope too steep.
        const JPH::Vec3 wanted     = ToJolt(velocity);
        const JPH::Vec3 walk       = wanted - UP * wanted.Dot(UP);
        const JPH::Vec3 current    = ch.GetLinearVelocity();
        const JPH::Vec3 ground     = ch.GetGroundVelocity();
        const bool      onGround   = ch.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
        const bool      movingAway = (current - ground).Dot(UP) > 0.1f;
        JPH::Vec3       next       = (onGround && !movingAway) ? ground : UP * current.Dot(UP);
        if (onGround && wanted.Dot(UP) > 0.0f)
            next += UP * wanted.Dot(UP);
        next += ToJolt(gravity) * dt;
        next += ch.CancelVelocityTowardsSteepSlopes(walk);
        ch.SetLinearVelocity(next);

        JPH::CharacterVirtual::ExtendedUpdateSettings update;
        update.mStickToFloorStepDown = -UP * slot.StickToFloorDistance;
        update.mWalkStairsStepUp     = UP * slot.StepHeight;
        const JPH::ObjectLayer layer = m_State->System->GetBodyInterface().GetObjectLayer(ch.GetInnerBodyID());
        ch.ExtendedUpdate(dt, ToJolt(gravity), update, m_State->System->GetDefaultBroadPhaseLayerFilter(layer),
                          m_State->System->GetDefaultLayerFilter(layer), JPH::BodyFilter(), JPH::ShapeFilter(),
                          *m_State->TempAllocator);
    }

    CharacterState PhysicsWorld::GetCharacterState(CharacterHandle character) const
    {
        if (!IsValid(character))
            return {};
        const JPH::CharacterVirtual& ch = *m_State->Characters[character.Index].Character;
        CharacterState               state;
        state.Position       = FromJolt(JPH::Vec3(ch.GetPosition()));
        state.LinearVelocity = FromJolt(ch.GetLinearVelocity());
        state.Ground         = FromJolt(ch.GetGroundState());
        if (state.Ground != CharacterGroundState::InAir)
        {
            state.GroundNormal   = FromJolt(ch.GetGroundNormal());
            state.GroundVelocity = FromJolt(ch.GetGroundVelocity());
            state.GroundUserData = ch.GetGroundUserData();
        }
        return state;
    }

    size_t PhysicsWorld::GetCharacterCount() const
    {
        return m_State ? m_State->Characters.size() - m_State->FreeCharacters.size() : 0;
    }
}
