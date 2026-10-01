#pragma once

#include "ECS/api/Entity.hpp"

#include <optional>
#include <string>

namespace Editor
{
    // Dragging entities out of the hierarchy. The payload ("HH_ENTITY") carries the entity's id,
    // so a field such as a script's EntityRef property can take it.

    // Makes the last item a drag source for entity, previewed as its name.
    void DragEntitySource(ECS::Entity entity, const std::string& name);

    // Makes the last item a drop target for entities: an entity dragged over it highlights it.
    // Returns the entity released on it.
    [[nodiscard]] std::optional<ECS::Entity> AcceptEntityDrop();
}
