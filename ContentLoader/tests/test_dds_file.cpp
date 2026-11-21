#include "doctest/doctest/doctest.h"

#include "ContentLoader/api/CompressedTexture.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"
#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using ContentLoader::CompressedFormat;
using ContentLoader::CompressedTexture;
using ContentLoader::TextureEncoding;

namespace
{
    // A texture whose mips are the full chain, each byte numbered so every mip's bytes differ.
    CompressedTexture MakeTexture(CompressedFormat format, uint32_t width, uint32_t height, uint32_t mipCount)
    {
        CompressedTexture texture;
        texture.Format = format;
        texture.Width  = width;
        texture.Height = height;
        for (uint32_t mip = 0; mip < mipCount; ++mip)
        {
            ContentLoader::CompressedMip level;
            level.Width  = std::max(width >> mip, 1u);
            level.Height = std::max(height >> mip, 1u);
            level.Offset = texture.Data.size();
            level.Size   = static_cast<size_t>(ContentLoader::GetCompressedMipSize(format, level.Width, level.Height));
            for (size_t i = 0; i < level.Size; ++i)
                texture.Data.push_back(static_cast<std::byte>((i * 7 + mip * 31) & 0xff));
            texture.Mips.push_back(level);
        }
        return texture;
    }

    void SetWord(std::vector<std::byte>& bytes, size_t word, uint32_t value)
    {
        std::memcpy(bytes.data() + word * 4, &value, 4);
    }

    std::string ParseError(const std::vector<std::byte>& bytes)
    {
        const auto result = ContentLoader::ParseDds(bytes);
        CHECK_FALSE(result.Texture.has_value());
        return result.Error;
    }
}

TEST_CASE("A written DDS parses back to the same format, size, mips and bytes")
{
    for (CompressedFormat format : { CompressedFormat::BC1Unorm, CompressedFormat::BC1Srgb, CompressedFormat::BC4Unorm,
                                     CompressedFormat::BC5Unorm, CompressedFormat::BC7Unorm, CompressedFormat::BC7Srgb })
    {
        const CompressedTexture texture = MakeTexture(format, 64, 16, 7);
        const auto              bytes   = ContentLoader::WriteDds(texture);
        CHECK(bytes.size() == 148 + texture.Data.size());

        const auto result = ContentLoader::ParseDds(bytes);
        REQUIRE(result.Texture.has_value());
        CHECK(result.Error.empty());
        CHECK(result.Texture->Format == format);
        CHECK(result.Texture->Width == 64);
        CHECK(result.Texture->Height == 16);
        CHECK(result.Texture->Data == texture.Data);
        REQUIRE(result.Texture->Mips.size() == 7);
        for (size_t mip = 0; mip < 7; ++mip)
        {
            CHECK(result.Texture->Mips[mip].Width == texture.Mips[mip].Width);
            CHECK(result.Texture->Mips[mip].Height == texture.Mips[mip].Height);
            CHECK(result.Texture->Mips[mip].Offset == texture.Mips[mip].Offset);
            CHECK(result.Texture->Mips[mip].Size == texture.Mips[mip].Size);
        }
    }
}

TEST_CASE("A 5 x 3 texture's three mips are sized in whole blocks")
{
    const auto result = ContentLoader::ParseDds(ContentLoader::WriteDds(MakeTexture(CompressedFormat::BC7Srgb, 5, 3, 3)));
    REQUIRE(result.Texture.has_value());
    const auto& mips = result.Texture->Mips;
    REQUIRE(mips.size() == 3);
    CHECK((mips[0].Width == 5 && mips[0].Height == 3 && mips[0].Size == 2 * 16));
    CHECK((mips[1].Width == 2 && mips[1].Height == 1 && mips[1].Size == 16));
    CHECK((mips[2].Width == 1 && mips[2].Height == 1 && mips[2].Size == 16));
    CHECK(mips[2].Offset == 3 * 16);
    CHECK(ContentLoader::GetCompressedMipSize(CompressedFormat::BC4Unorm, 5, 3) == 16);
    CHECK(ContentLoader::GetCompressedBlockBytes(CompressedFormat::BC1Srgb) == 8);
    CHECK(ContentLoader::GetCompressedBlockBytes(CompressedFormat::BC5Unorm) == 16);
}

TEST_CASE("A malformed DDS is one error naming what is wrong and no texture")
{
    const auto good = ContentLoader::WriteDds(MakeTexture(CompressedFormat::BC5Unorm, 8, 8, 4));

    CHECK(ParseError({}).find("shorter than a DDS header") != std::string::npos);

    auto truncated = good;
    truncated.pop_back();
    CHECK(ParseError(truncated).find("truncated") != std::string::npos);

    auto magic = good;
    SetWord(magic, 0, 0x12345678);
    CHECK(ParseError(magic).find("not a DDS file") != std::string::npos);

    auto legacy = good;
    SetWord(legacy, 21, 0x31545844); // "DXT1"
    CHECK(ParseError(legacy).find("no DX10 header") != std::string::npos);

    auto format = good;
    SetWord(format, 32, 28); // R8G8B8A8_UNORM
    CHECK(ParseError(format).find("DXGI format 28") != std::string::npos);

    auto mips = good;
    SetWord(mips, 7, 5); // 8 x 8 has four
    CHECK(ParseError(mips).find("5 mips") != std::string::npos);

    auto array = good;
    SetWord(array, 35, 6);
    CHECK(ParseError(array).find("not one 2D texture") != std::string::npos);

    auto empty = good;
    SetWord(empty, 4, 0);
    CHECK(ParseError(empty).find("size is 0 x 8") != std::string::npos);
}

TEST_CASE("A DDS with no mip count reads as one mip")
{
    auto bytes = ContentLoader::WriteDds(MakeTexture(CompressedFormat::BC1Unorm, 4, 4, 1));
    SetWord(bytes, 7, 0);
    const auto result = ContentLoader::ParseDds(bytes);
    REQUIRE(result.Texture.has_value());
    CHECK(result.Texture->Mips.size() == 1);
}

TEST_CASE("The compressed sibling is named by encoding, assets:// and slashes aside")
{
    CHECK(ContentLoader::MakeCompressedTexturePath("Textures/a.png", TextureEncoding::Color) == "Textures/a.png.bc7.dds");
    CHECK(ContentLoader::MakeCompressedTexturePath("Textures/a.png", TextureEncoding::Data) == "Textures/a.png.bc7lin.dds");
    CHECK(ContentLoader::MakeCompressedTexturePath("Textures/a.png", TextureEncoding::Normal) == "Textures/a.png.bc5.dds");
    CHECK(ContentLoader::MakeCompressedTexturePath("assets://Textures/a.png", TextureEncoding::Color) == "Textures/a.png.bc7.dds");
    CHECK(ContentLoader::MakeCompressedTexturePath("Textures\\a.png", TextureEncoding::Normal) == "Textures/a.png.bc5.dds");
    CHECK(ContentLoader::MakeCompressedTexturePath("engine://Content/b.png", TextureEncoding::Data) ==
          "engine://Content/b.png.bc7lin.dds");
    CHECK(ContentLoader::GetEncodingFormat(TextureEncoding::Color) == CompressedFormat::BC7Srgb);
    CHECK(ContentLoader::GetEncodingFormat(TextureEncoding::Data) == CompressedFormat::BC7Unorm);
    CHECK(ContentLoader::GetEncodingFormat(TextureEncoding::Normal) == CompressedFormat::BC5Unorm);
}

TEST_CASE("LoadDdsTexture reads a file and logs one error for a missing or malformed one")
{
    TempDir               root;
    FS::FileSystemManager files;
    auto                  fs = std::make_unique<FS::FileSystem>();
    REQUIRE(fs->RegisterPath("assets://", root.Path()));
    REQUIRE(files.Register(std::move(fs)));

    const CompressedTexture texture = MakeTexture(CompressedFormat::BC7Unorm, 16, 16, 5);
    REQUIRE(files.WriteFile("assets://a.png.bc7lin.dds", ContentLoader::WriteDds(texture)));
    const auto loaded = ContentLoader::LoadDdsTexture("a.png.bc7lin.dds", files);
    REQUIRE(loaded.has_value());
    CHECK(loaded->Data == texture.Data);

    {
        LogCapture capture;
        CHECK_FALSE(ContentLoader::LoadDdsTexture("missing.dds", files).has_value());
        const auto errors = capture.Lines("[ERROR]");
        REQUIRE(errors.size() == 1);
        CHECK(errors[0].find("assets://missing.dds") != std::string::npos);
        CHECK(errors[0].find("cannot be read") != std::string::npos);
    }
    {
        std::ofstream(root.Path() / "broken.dds", std::ios::binary) << "not a dds";
        LogCapture capture;
        CHECK_FALSE(ContentLoader::LoadDdsTexture("broken.dds", files).has_value());
        const auto errors = capture.Lines("[ERROR]");
        REQUIRE(errors.size() == 1);
        CHECK(errors[0].find("assets://broken.dds") != std::string::npos);
        CHECK(errors[0].find("the file is 9 bytes") != std::string::npos);
    }
}
