#pragma once

#include "ContentLoaderApi.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Block compressed textures with their mips, as the Cooker writes them beside a source image and the
// renderer uploads them: DDS files with the DX10 header only (every one is the engine's own, so a
// legacy FourCC file is refused). Pure functions over bytes, shared by the runtime and the Cooker.
namespace ContentLoader
{
    // What a texture holds, which picks its compressed format and file name.
    enum class TextureEncoding : uint32_t
    {
        Color,  // sRGB colour (base colour, emissive, UI): BC7 sRGB
        Data,   // linear data (metallic-roughness, occlusion): BC7 linear
        Normal, // tangent-space normal, x and y only: BC5
    };

    // ContentLoader does not include the RHI; the renderer maps these to its block formats.
    enum class CompressedFormat : uint32_t
    {
        BC1Unorm,
        BC1Srgb,
        BC4Unorm,
        BC5Unorm,
        BC7Unorm,
        BC7Srgb,
    };

    // The bytes of one 4 x 4 block: 8 for BC1 and BC4, 16 for BC5 and BC7.
    CONTENT_LOADER_API uint32_t GetCompressedBlockBytes(CompressedFormat format);

    // The bytes of a width x height level in whole 4 x 4 blocks.
    CONTENT_LOADER_API uint64_t GetCompressedMipSize(CompressedFormat format, uint32_t width, uint32_t height);

    // One mip level: its size in texels and its bytes, Size bytes from Offset in the texture's Data.
    struct CompressedMip
    {
        uint32_t Width  = 0;
        uint32_t Height = 0;
        size_t   Offset = 0;
        size_t   Size   = 0;
    };

    // Mips[0] is the full size, each next one half the one before (at least 1), back to back in Data.
    struct CompressedTexture
    {
        CompressedFormat           Format = CompressedFormat::BC7Srgb;
        uint32_t                   Width  = 0;
        uint32_t                   Height = 0;
        std::vector<CompressedMip> Mips;
        std::vector<std::byte>     Data;
    };

    struct DdsReadResult
    {
        std::optional<CompressedTexture> Texture;
        std::string                      Error;
    };

    // Reads a DX10 DDS of one 2D texture in a format above. A file that is too short, has another magic,
    // no DX10 header, another format, dimension or array size, no size, more mips than its size has, or
    // too few bytes for its mips gives one Error naming it and no texture.
    CONTENT_LOADER_API DdsReadResult ParseDds(const std::vector<std::byte>& bytes);

    // Writes the texture as ParseDds reads it: its Mips must be the chain of its format and size, back
    // to back in Data, so ParseDds of the result gives the texture back.
    CONTENT_LOADER_API std::vector<std::byte> WriteDds(const CompressedTexture& texture);

    // Reads a .dds (under assets:// unless it names a mount); a file that cannot be read or parsed logs
    // one [Texture] error naming it and gives nullopt.
    CONTENT_LOADER_API std::optional<CompressedTexture> LoadDdsTexture(const std::string& path,
                                                                     const FS::FileSystemManager& fileSystem);

    // The compressed sibling of a source image, the one place it is named: FS::MakeAssetKey of the path
    // (assets:// dropped) plus .bc7.dds (Color), .bc7lin.dds (Data) or .bc5.dds (Normal), so
    // Textures/a.png's colour sibling is Textures/a.png.bc7.dds.
    CONTENT_LOADER_API std::string MakeCompressedTexturePath(const std::string& path, TextureEncoding encoding);

    // BC7 sRGB for Color, BC7 linear for Data, BC5 for Normal.
    CONTENT_LOADER_API CompressedFormat GetEncodingFormat(TextureEncoding encoding);
}
