#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include "JoltState.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyInterface.h>

#include <algorithm>
#include <tuple>
#include <utility>

namespace HP
{
    namespace
    {
        // One key per pair of bodies, whichever way round Jolt names them.
        uint64_t MakePairKey(BodyHandle a, BodyHandle b)
        {
            const uint32_t low  = std::min(a.Value, b.Value);
            const uint32_t high = std::max(a.Value, b.Value);
            return static_cast<uint64_t>(low) << 32 | high;
        }

        BodyHandle FirstOfPair(uint64_t key) { return BodyHandle{ static_cast<uint32_t>(key >> 32) }; }

        BodyHandle SecondOfPair(uint64_t key) { return BodyHandle{ static_cast<uint32_t>(key) }; }

        // The pair's order and ties, by which the drained events are sorted.
        auto SortKey(const ContactEvent& event)
        {
            return std::make_tuple(event.Type, event.UserDataA, event.UserDataB, event.BodyA.Value, event.BodyB.Value);
        }
    }

    void PhysicsWorld::CollectContacts()
    {
        JoltState&                state  = *m_State;
        const JPH::BodyInterface& bodies = state.System->GetBodyInterface();
        const auto isAwake = [&](BodyHandle body) { return bodies.IsActive(JPH::BodyID(body.Value)); };
        // The body's user data and sensor flag, a destroyed body's included until this step is done.
        const auto exitFor = [&](uint64_t key) {
            ContactEvent event;
            event.Type  = ContactEventType::Exit;
            event.BodyA = FirstOfPair(key);
            event.BodyB = SecondOfPair(key);
            const auto recordA = state.Records.find(event.BodyA.Value);
            const auto recordB = state.Records.find(event.BodyB.Value);
            const BodyRecord a = recordA != state.Records.end() ? recordA->second : BodyRecord{};
            const BodyRecord b = recordB != state.Records.end() ? recordB->second : BodyRecord{};
            event.UserDataA    = a.UserData;
            event.UserDataB    = b.UserData;
            event.IsSensor     = a.IsSensor || b.IsSensor;
            return event;
        };

        std::vector<ContactEvent> raw;
        state.Contacts.Take(raw);
        for (const ContactEvent& event : raw)
        {
            const uint64_t key = MakePairKey(event.BodyA, event.BodyB);
            if (event.Type == ContactEventType::Enter)
            {
                // A sleeping pair touching again on waking: it never parted.
                if (state.SleepingContacts.erase(key) == 0)
                    state.Pending.push_back(event);
                continue;
            }
            // Removed with both bodies alive and asleep: Jolt dropped the contact for sleeping.
            if (IsValid(event.BodyA) && IsValid(event.BodyB) && !isAwake(event.BodyA) && !isAwake(event.BodyB))
                state.SleepingContacts[key] = false;
            else
                state.Pending.push_back(exitFor(key));
        }

        // A sleeping pair that lost a body, or woke and has gone a whole step without touching again,
        // has parted.
        for (auto pair = state.SleepingContacts.begin(); pair != state.SleepingContacts.end();)
        {
            const BodyHandle a = FirstOfPair(pair->first);
            const BodyHandle b = SecondOfPair(pair->first);
            bool             parted = !IsValid(a) || !IsValid(b);
            if (!parted && (isAwake(a) || isAwake(b)))
            {
                parted       = pair->second;
                pair->second = true;
            }
            if (parted)
            {
                state.Pending.push_back(exitFor(pair->first));
                pair = state.SleepingContacts.erase(pair);
            }
            else
                ++pair;
        }

        // The bodies destroyed before this step have had their Exits.
        for (const uint32_t body : state.Destroyed)
            state.Records.erase(body);
        state.Destroyed.clear();
    }

    void PhysicsWorld::DrainContactEvents(std::vector<ContactEvent>& out)
    {
        if (!m_State)
            return;
        const size_t first = out.size();
        out.insert(out.end(), m_State->Pending.begin(), m_State->Pending.end());
        m_State->Pending.clear();

        for (size_t index = first; index < out.size(); ++index)
        {
            ContactEvent& event = out[index];
            if (std::tie(event.UserDataA, event.BodyA.Value) > std::tie(event.UserDataB, event.BodyB.Value))
            {
                std::swap(event.BodyA, event.BodyB);
                std::swap(event.UserDataA, event.UserDataB);
                event.Normal = -event.Normal;
            }
        }
        std::sort(out.begin() + static_cast<std::ptrdiff_t>(first), out.end(),
                  [](const ContactEvent& left, const ContactEvent& right) { return SortKey(left) < SortKey(right); });
    }
}
