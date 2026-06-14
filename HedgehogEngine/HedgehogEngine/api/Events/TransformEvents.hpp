#pragma once

#include "ECS/api/Entity.hpp"

namespace HedgehogEngine
{
    /// Emitted by any code that writes to TransformComponent PRS fields.
    struct TransformChangedEvent   { ECS::Entity entity; };

    /// Emitted by TransformSystem after rebuilding m_LocalMatrix for an entity.
    struct LocalMatrixUpdatedEvent { ECS::Entity entity; };

    /// Emitted by HierarchySystem after writing m_ObjMatrix for an entity.
    struct WorldMatrixUpdatedEvent { ECS::Entity entity; };
}
