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
