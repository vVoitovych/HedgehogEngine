#pragma once

#include "ContentTypes.hpp"

#include <array>
#include <memory>

namespace FS
{
    class FileSystemManager;
}

namespace RHI
{
    class IRHIDevice;
    class IRHITexture;
}

namespace Editor
{
    // The ImGui texture id of each type's icon, indexed by ContentType; nullptr for a type drawn as
    // a coloured tile (DrawAssetIcon).
    using ContentIconIds = std::array<void*, CONTENT_TYPE_COUNT>;

    // The Content panel's pictures (Editor/Resources/Icons/), one texture per type that has one,
    // uploaded once when the device is ready. A type without a picture, or whose picture fails to
    // load, has no texture. Pictures in that folder for types the editor does not have yet are left
    // alone until those types exist.
    class ContentIcons
    {
    public:
        ContentIcons();
        ~ContentIcons();

        ContentIcons(const ContentIcons&)            = delete;
        ContentIcons& operator=(const ContentIcons&) = delete;
        ContentIcons(ContentIcons&&)                 = delete;
        ContentIcons& operator=(ContentIcons&&)      = delete;

        void Load(const RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem);

        // Destroys the textures: after the GPU is idle and every id made from them is released.
        void Release();

        // The type's icon, or nullptr.
        [[nodiscard]] const RHI::IRHITexture* Get(ContentType type) const;

    private:
        std::array<std::unique_ptr<RHI::IRHITexture>, CONTENT_TYPE_COUNT> m_Textures;
    };
}
