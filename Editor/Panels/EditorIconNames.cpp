#include "EditorIcons.hpp"

#include <string_view>

namespace Editor
{
    const char* GetEditorIconFile(EditorIcon icon)
    {
        switch (icon)
        {
        case EditorIcon::Logo:          return "hedgehog_logo.png";
        case EditorIcon::Play:          return "play.png";
        case EditorIcon::Pause:         return "pause.png";
        case EditorIcon::Stop:          return "stop.png";
        case EditorIcon::Settings:      return "settings.png";
        case EditorIcon::Plus:          return "plus.png";
        case EditorIcon::Search:        return "search.png";
        case EditorIcon::More:          return "more.png";
        case EditorIcon::Hierarchy:     return "hierarchy.png";
        case EditorIcon::Scene:         return "scene.png";
        case EditorIcon::Game:          return "game.png";
        case EditorIcon::Inspector:     return "inspector.png";
        case EditorIcon::Project:       return "project.png";
        case EditorIcon::Console:       return "console.png";
        case EditorIcon::GameObject:    return "game_object.png";
        case EditorIcon::Folder:        return "folder.png";
        case EditorIcon::Camera:        return "camera.png";
        case EditorIcon::Light:         return "light.png";
        case EditorIcon::Mesh:          return "mesh.png";
        case EditorIcon::Transform:     return "transform.png";
        case EditorIcon::Material:      return "material.png";
        case EditorIcon::Script:        return "script.png";
        case EditorIcon::Animator:      return "animator.png";
        case EditorIcon::UiCanvas:      return "ui_canvas.png";
        case EditorIcon::UiRect:        return "ui_rect.png";
        case EditorIcon::UiImage:       return "ui_image.png";
        case EditorIcon::UiText:        return "ui_text.png";
        case EditorIcon::UiButton:      return "ui_button.png";
        case EditorIcon::AudioSource:   return "audio_source.png";
        case EditorIcon::AudioListener: return "audio_listener.png";
        case EditorIcon::RigidBody:     return "rigid_body.png";
        case EditorIcon::Collider:      return "collider.png";
        case EditorIcon::ToolMove:      return "tool_move.png";
        case EditorIcon::ToolRotate:    return "tool_rotate.png";
        case EditorIcon::ToolScale:     return "tool_scale.png";
        case EditorIcon::SpaceLocal:    return "space_local.png";
        case EditorIcon::SpaceWorld:    return "space_world.png";
        case EditorIcon::Count:         break;
        }
        return "";
    }

    std::optional<EditorIcon> FindEditorIcon(std::string_view name)
    {
        if (name.empty())
            return std::nullopt;
        for (size_t index = 0; index < EDITOR_ICON_COUNT; ++index)
        {
            const EditorIcon       icon = static_cast<EditorIcon>(index);
            const std::string_view file = GetEditorIconFile(icon);
            if (file.size() == name.size() + 4 && file.starts_with(name) && file.ends_with(".png"))
                return icon;
        }
        return std::nullopt;
    }
}
