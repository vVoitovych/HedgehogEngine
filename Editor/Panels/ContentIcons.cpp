#include "ContentIcons.hpp"

#include "ContentLoader/api/TextureLoader.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "Logger/api/Logger.hpp"

#include "RHI/api/IRHIDevice.hpp"

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
            case ContentType::Pipeline:
            case ContentType::VertexDescription:
            case ContentType::RenderGraph:
            case ContentType::Other:             return nullptr;
            }
            return nullptr;
        }

        std::unique_ptr<RHI::IRHITexture> Upload(const RHI::IRHIDevice& device, const ContentLoader::TextureLoader& image)
        {
            const uint32_t width  = static_cast<uint32_t>(image.GetWidth());
            const uint32_t height = static_cast<uint32_t>(image.GetHeight());
            const size_t   size   = static_cast<size_t>(width) * height * 4; // decoded as RGBA

            auto staging = device.CreateBuffer(size, RHI::BufferUsage::TransferSrc, RHI::MemoryUsage::CpuToGpu);
            staging->CopyData(image.GetData(), size);

            RHI::TextureDesc desc;
            desc.Width  = width;
            desc.Height = height;
            desc.Format = RHI::Format::R8G8B8A8Srgb;
            desc.Usage  = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;
            auto texture = device.CreateTexture(desc);

            device.ExecuteImmediately([&](RHI::IRHICommandList& cmd)
            {
                const RHI::TextureBarrier toCopy{ texture.get(), RHI::ResourceState::Undefined, RHI::ResourceState::CopyDst };
                cmd.Barrier({ &toCopy, 1 }, {});
                cmd.CopyBufferToTexture(*staging, *texture);
                const RHI::TextureBarrier toRead{ texture.get(), RHI::ResourceState::CopyDst, RHI::ResourceState::ShaderResource };
                cmd.Barrier({ &toRead, 1 }, {});
            });
            return texture;
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
            m_Textures[index] = Upload(device, image);
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
