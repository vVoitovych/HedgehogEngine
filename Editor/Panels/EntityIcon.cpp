#include "EntityIcon.hpp"

namespace Editor
{
    EditorIcon ChooseEntityIcon(const EntityTraits& traits)
    {
        if (traits.HasCamera)        return EditorIcon::Camera;
        if (traits.HasLight)         return EditorIcon::Light;
        if (traits.HasUiCanvas)      return EditorIcon::UiCanvas;
        if (traits.HasUiElement)     return EditorIcon::UiRect;
        if (traits.HasAudioSource)   return EditorIcon::AudioSource;
        if (traits.HasAudioListener) return EditorIcon::AudioListener;
        if (traits.HasMesh)          return EditorIcon::Mesh;
        if (traits.HasChildren)      return EditorIcon::Folder;
        return EditorIcon::GameObject;
    }
}
