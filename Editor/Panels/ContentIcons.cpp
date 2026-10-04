#include "ContentIcons.hpp"

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
        constexpr const char* ICON_FOLDER = "engine://Editor/Resources/Icons/";

        // The picture for each type, or nullptr: those types keep their coloured tile.
        const char* GetIconFile(ContentType type)
        {
            switch (type)
            {
            case ContentType::Folder:            return "folder_icon.png";
            case ContentType::Scene:             return "scene_icon.png";
            case ContentType::Material:          return "material_icon.png";
            case ContentType::Texture:           return "texture_icon.png";
            case ContentType::Mesh:              return "mesh_icon.png";
            case ContentType::Script:            return "script_icon.png";
            case ContentType::Shader:            return "shader_icon.png";
            case ContentType::Audio:             return "sound_icon.png";
            case ContentType::Prefab:            return "prefab_icon.png";
            case ContentType::Pipeline:
            case ContentType::VertexDescription:
            case ContentType::RenderGraph:
            case ContentType::Font:
            case ContentType::Other:             return nullptr;
            }
            return nullptr;
        }
    }

    ContentIcons::ContentIcons()  = default;
    ContentIcons::~ContentIcons() = default;

    void ContentIcons::Load(const RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem)
    {
        for (size_t index = 0; index < CONTENT_TYPE_COUNT; ++index)
        {
            const char* file = GetIconFile(static_cast<ContentType>(index));
            if (!file)
                continue;

            ContentLoader::TextureLoader image;
            if (!image.LoadFromVirtualPath(std::string(ICON_FOLDER) + file, fileSystem))
            {
                LOGWARNING("Content panel: no icon '", file, "'; its type is drawn as a tile.");
                continue;
            }
            m_Textures[index] = UploadIconTexture(device, image);
        }
    }

    void ContentIcons::Release()
    {
        for (auto& texture : m_Textures)
            texture.reset();
    }

    const RHI::IRHITexture* ContentIcons::Get(ContentType type) const
    {
        return m_Textures[static_cast<size_t>(type)].get();
    }
}
