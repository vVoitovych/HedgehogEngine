#pragma once

#include "Entity.hpp"
#include "EcsApi.hpp"

#include <vector>
#include <array>
#include <bitset>
#include <cstdint>

namespace ECS
{
    class EntityManager
    {
    public:
        ECS_API EntityManager();

        ECS_API Entity    CreateEntity();
        ECS_API void      CreateEntity(Entity entity);
        ECS_API void      DestroyEntity(Entity entity);

        ECS_API Signature GetSignature(Entity entity) const;
        ECS_API void      SetSignature(Entity entity, Signature signature);

        // False for ids out of range and ids not currently created.
        ECS_API bool      IsAlive(Entity entity) const;
        // Bumped each time the id is destroyed, so a recycled id reads differently.
        ECS_API uint32_t  GetGeneration(Entity entity) const;

    private:
        std::vector<Entity>                 m_EntityPool;
        std::array<Signature, MAX_ENTITIES> m_Signatures;
        std::array<uint32_t, MAX_ENTITIES>  m_Generations{};
        std::bitset<MAX_ENTITIES>           m_Alive;

        size_t m_EntityCount;
    };
}
