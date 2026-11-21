#include "api/CompressedTexture.hpp"

#include "FileSystem/api/PathUtils.hpp"
#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace ContentLoader
{
    namespace
    {
        // DDS's layout (Microsoft's DDS_HEADER, DDS_PIXELFORMAT and DDS_HEADER_DXT10) as 32-bit words.
        constexpr uint32_t DDS_MAGIC           = 0x20534444; // "DDS "
        constexpr uint32_t DX10_FOURCC         = 0x30315844; // "DX10"
        constexpr uint32_t HEADER_SIZE         = 124;
        constexpr uint32_t PIXEL_FORMAT_SIZE   = 32;
        constexpr size_t   HEADER_WORDS        = 1 + HEADER_SIZE / 4 + 5; // magic, header, DX10 header
        constexpr size_t   DATA_OFFSET         = HEADER_WORDS * 4;
        constexpr uint32_t DDPF_FOURCC         = 0x4;
        constexpr uint32_t DDSD_REQUIRED       = 0x1 | 0x2 | 0x4 | 0x1000; // caps, height, width, pixel format
        constexpr uint32_t DDSD_MIPMAPCOUNT    = 0x20000;
        constexpr uint32_t DDSD_LINEARSIZE     = 0x80000;
        constexpr uint32_t DDSCAPS_COMPLEX     = 0x8;
        constexpr uint32_t DDSCAPS_TEXTURE     = 0x1000;
        constexpr uint32_t DDSCAPS_MIPMAP      = 0x400000;
        constexpr uint32_t DIMENSION_TEXTURE2D = 3;

        // Word indices from the start of the file.
        constexpr size_t W_SIZE       = 1;
        constexpr size_t W_FLAGS      = 2;
        constexpr size_t W_HEIGHT     = 3;
        constexpr size_t W_WIDTH      = 4;
        constexpr size_t W_PITCH      = 5;
        constexpr size_t W_MIPS       = 7;
        constexpr size_t W_PF_SIZE    = 19;
        constexpr size_t W_PF_FLAGS   = 20;
        constexpr size_t W_PF_FOURCC  = 21;
        constexpr size_t W_CAPS       = 27;
        constexpr size_t W_DXGI       = 32;
        constexpr size_t W_DIMENSION  = 33;
        constexpr size_t W_ARRAY_SIZE = 35;

        struct DxgiEntry
        {
            CompressedFormat Format;
            uint32_t         Dxgi;
        };

        constexpr std::array<DxgiEntry, 6> DXGI_FORMATS{ {
            { CompressedFormat::BC1Unorm, 71 },
            { CompressedFormat::BC1Srgb, 72 },
            { CompressedFormat::BC4Unorm, 80 },
            { CompressedFormat::BC5Unorm, 83 },
            { CompressedFormat::BC7Unorm, 98 },
            { CompressedFormat::BC7Srgb, 99 },
        } };

        uint32_t ToDxgi(CompressedFormat format)
        {
            for (const DxgiEntry& entry : DXGI_FORMATS)
                if (entry.Format == format)
                    return entry.Dxgi;
            return 0;
        }

        uint32_t FullMipCount(uint32_t width, uint32_t height)
        {
            uint32_t count = 1;
            for (uint32_t size = std::max(width, height); size > 1; size >>= 1)
                ++count;
            return count;
        }

        uint32_t ReadWord(const std::vector<std::byte>& bytes, size_t word)
        {
            uint32_t value = 0;
            std::memcpy(&value, bytes.data() + word * 4, 4); // DDS is little-endian, as x64 is
            return value;
        }
    }

    uint32_t GetCompressedBlockBytes(CompressedFormat format)
    {
        switch (format)
        {
            case CompressedFormat::BC1Unorm:
            case CompressedFormat::BC1Srgb:
            case CompressedFormat::BC4Unorm:
                return 8;
            default:
                return 16;
        }
    }

    uint64_t GetCompressedMipSize(CompressedFormat format, uint32_t width, uint32_t height)
    {
        return static_cast<uint64_t>((width + 3) / 4) * ((height + 3) / 4) * GetCompressedBlockBytes(format);
    }

    DdsReadResult ParseDds(const std::vector<std::byte>& bytes)
    {
        DdsReadResult result;
        if (bytes.size() < DATA_OFFSET)
        {
            result.Error = "the file is " + std::to_string(bytes.size()) + " bytes, shorter than a DDS header";
            return result;
        }
        if (ReadWord(bytes, 0) != DDS_MAGIC || ReadWord(bytes, W_SIZE) != HEADER_SIZE ||
            ReadWord(bytes, W_PF_SIZE) != PIXEL_FORMAT_SIZE)
        {
            result.Error = "the file is not a DDS file";
            return result;
        }
        if ((ReadWord(bytes, W_PF_FLAGS) & DDPF_FOURCC) == 0 || ReadWord(bytes, W_PF_FOURCC) != DX10_FOURCC)
        {
            result.Error = "the DDS file has no DX10 header";
            return result;
        }

        const uint32_t dxgi  = ReadWord(bytes, W_DXGI);
        const auto     entry = std::find_if(DXGI_FORMATS.begin(), DXGI_FORMATS.end(),
                                            [dxgi](const DxgiEntry& candidate) { return candidate.Dxgi == dxgi; });
        if (entry == DXGI_FORMATS.end())
        {
            result.Error = "the DDS file's DXGI format " + std::to_string(dxgi) + " is not BC1, BC4, BC5 or BC7";
            return result;
        }
        if (ReadWord(bytes, W_DIMENSION) != DIMENSION_TEXTURE2D || ReadWord(bytes, W_ARRAY_SIZE) != 1)
        {
            result.Error = "the DDS file is not one 2D texture";
            return result;
        }

        CompressedTexture texture;
        texture.Format = entry->Format;
        texture.Width  = ReadWord(bytes, W_WIDTH);
        texture.Height = ReadWord(bytes, W_HEIGHT);
        if (texture.Width == 0 || texture.Height == 0)
        {
            result.Error = "the DDS file's size is " + std::to_string(texture.Width) + " x " + std::to_string(texture.Height);
            return result;
        }
        const uint32_t mipCount = std::max(ReadWord(bytes, W_MIPS), 1u);
        if (mipCount > FullMipCount(texture.Width, texture.Height))
        {
            result.Error = "the DDS file has " + std::to_string(mipCount) + " mips, more than its " +
                           std::to_string(texture.Width) + " x " + std::to_string(texture.Height) + " size has";
            return result;
        }

        size_t offset = 0;
        for (uint32_t mip = 0; mip < mipCount; ++mip)
        {
            CompressedMip level;
            level.Width  = std::max(texture.Width >> mip, 1u);
            level.Height = std::max(texture.Height >> mip, 1u);
            level.Offset = offset;
            level.Size   = static_cast<size_t>(GetCompressedMipSize(texture.Format, level.Width, level.Height));
            offset += level.Size;
            texture.Mips.push_back(level);
        }
        if (bytes.size() - DATA_OFFSET < offset)
        {
            result.Error = "the DDS file is truncated: its mips need " + std::to_string(offset) + " bytes, it holds " +
                           std::to_string(bytes.size() - DATA_OFFSET);
            return result;
        }
        texture.Data.assign(bytes.begin() + DATA_OFFSET, bytes.begin() + static_cast<std::ptrdiff_t>(DATA_OFFSET + offset));
        result.Texture = std::move(texture);
        return result;
    }

    std::vector<std::byte> WriteDds(const CompressedTexture& texture)
    {
        std::array<uint32_t, HEADER_WORDS> words{};
        const auto mipCount = static_cast<uint32_t>(texture.Mips.size());
        words[0]            = DDS_MAGIC;
        words[W_SIZE]       = HEADER_SIZE;
        words[W_FLAGS]      = DDSD_REQUIRED | DDSD_MIPMAPCOUNT | DDSD_LINEARSIZE;
        words[W_HEIGHT]     = texture.Height;
        words[W_WIDTH]      = texture.Width;
        words[W_PITCH]      = mipCount > 0 ? static_cast<uint32_t>(texture.Mips[0].Size) : 0;
        words[W_MIPS]       = mipCount;
        words[W_PF_SIZE]    = PIXEL_FORMAT_SIZE;
        words[W_PF_FLAGS]   = DDPF_FOURCC;
        words[W_PF_FOURCC]  = DX10_FOURCC;
        words[W_CAPS]       = DDSCAPS_TEXTURE | (mipCount > 1 ? DDSCAPS_COMPLEX | DDSCAPS_MIPMAP : 0);
        words[W_DXGI]       = ToDxgi(texture.Format);
        words[W_DIMENSION]  = DIMENSION_TEXTURE2D;
        words[W_ARRAY_SIZE] = 1;

        std::vector<std::byte> bytes(DATA_OFFSET);
        std::memcpy(bytes.data(), words.data(), DATA_OFFSET);
        bytes.insert(bytes.end(), texture.Data.begin(), texture.Data.end());
        return bytes;
    }

    std::optional<CompressedTexture> LoadDdsTexture(const std::string& path, const FS::FileSystemManager& fileSystem)
    {
        const std::string virtualPath = FS::ToAssetVirtualPath(path);
        const auto        bytes       = fileSystem.ReadFile(virtualPath);
        if (!bytes)
        {
            LOGERROR("[Texture] ", virtualPath, " cannot be read.");
            return std::nullopt;
        }
        DdsReadResult result = ParseDds(*bytes);
        if (!result.Texture)
            LOGERROR("[Texture] ", virtualPath, ": ", result.Error, ".");
        return std::move(result.Texture);
    }

    std::string MakeCompressedTexturePath(const std::string& path, TextureEncoding encoding)
    {
        switch (encoding)
        {
            case TextureEncoding::Data:
                return FS::MakeAssetKey(path) + ".bc7lin.dds";
            case TextureEncoding::Normal:
                return FS::MakeAssetKey(path) + ".bc5.dds";
            default:
                return FS::MakeAssetKey(path) + ".bc7.dds";
        }
    }

    CompressedFormat GetEncodingFormat(TextureEncoding encoding)
    {
        switch (encoding)
        {
            case TextureEncoding::Data:
                return CompressedFormat::BC7Unorm;
            case TextureEncoding::Normal:
                return CompressedFormat::BC5Unorm;
            default:
                return CompressedFormat::BC7Srgb;
        }
    }
}
