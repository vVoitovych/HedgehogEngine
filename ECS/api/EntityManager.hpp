#pragma once

#include "Entity.hpp"
#include "EcsApi.hpp"

#include <vector>
#include <array>

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

    private:
        std::vector<Entity>                 m_EntityPool;
        std::array<Signature, MAX_ENTITIES> m_Signatures;

        size_t m_EntityCount;
    };
}
