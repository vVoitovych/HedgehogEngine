#include "EditorIcons.hpp"

#include "IconUpload.hpp"

#include "ContentLoader/api/TextureLoader.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "Logger/api/Logger.hpp"

#include "RHI/api/IRHITexture.hpp"

#include <string>

namespace Editor
{
    namespace
    {
        constexpr const char* ICON_FOLDER = "engine://Editor/Resources/Icons/UI/";
    }

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
        case EditorIcon::Count:         break;
        }
        return "";
    }

    EditorIcons::EditorIcons()  = default;
    EditorIcons::~EditorIcons() = default;

    void EditorIcons::Load(const RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem)
    {
        for (size_t index = 0; index < EDITOR_ICON_COUNT; ++index)
        {
            const char* file = GetEditorIconFile(static_cast<EditorIcon>(index));

            ContentLoader::TextureLoader image;
            if (!image.LoadFromVirtualPath(std::string(ICON_FOLDER) + file, fileSystem))
            {
                LOGWARNING("Editor: no icon '", file, "'; it is not drawn.");
                continue;
            }
            m_Textures[index] = UploadIconTexture(device, image);
        }
    }

    void EditorIcons::Release()
    {
        for (auto& texture : m_Textures)
            texture.reset();
    }

    const RHI::IRHITexture* EditorIcons::Get(EditorIcon icon) const
    {
        return m_Textures[static_cast<size_t>(icon)].get();
    }
}
