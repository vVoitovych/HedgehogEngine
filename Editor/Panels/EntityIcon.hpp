#pragma once

#include "EditorIcons.hpp"

namespace Editor
{
    // What an entity has that decides its icon in the hierarchy and the inspector.
    struct EntityTraits
    {
        bool HasCamera        = false;
        bool HasLight         = false;
        bool HasUiCanvas      = false;
        bool HasUiElement     = false; // a UiRectComponent
        bool HasAudioSource   = false;
        bool HasAudioListener = false;
        bool HasMesh          = false;
        bool HasChildren      = false;
    };

    // The entity's icon: the first of camera, light, UI canvas, UI element, audio source, audio
    // listener and mesh it has; else a folder when it only groups children, else a game object.
    [[nodiscard]] EditorIcon ChooseEntityIcon(const EntityTraits& traits);
}
