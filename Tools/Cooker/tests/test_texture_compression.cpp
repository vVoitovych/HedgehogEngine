#include "TextureCompression.hpp"

#include "ContentLoader/api/CompressedTexture.hpp"
#include "ContentLoader/api/TextureLoader.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"

#include "bc7decomp.h"
#include "rgbcx.h"

#include "doctest/doctest/doctest.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

using ContentLoader::CompressedFormat;
using ContentLoader::CompressedTexture;
using ContentLoader::TextureEncoding;

namespace
{
    std::vector<uint8_t> MakeGradient(uint32_t w, uint32_t h)
    {
        std::vector<uint8_t> texels(size_t(w) * h * 4);
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x)
            {
                uint8_t* t = &texels[(size_t(y) * w + x) * 4];
                t[0]       = static_cast<uint8_t>(x * 255 / std::max(1u, w - 1));
                t[1]       = static_cast<uint8_t>(y * 255 / std::max(1u, h - 1));
                t[2]       = static_cast<uint8_t>((x + y) * 255 / std::max(1u, w + h - 2));
                t[3]       = 255;
            }
        return texels;
    }

    std::vector<uint8_t> MakeChecker(uint32_t w, uint32_t h, uint32_t square)
    {
        std::vector<uint8_t> texels(size_t(w) * h * 4);
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x)
            {
                const uint8_t v = ((x / square + y / square) % 2) ? 230 : 20;
                uint8_t*      t = &texels[(size_t(y) * w + x) * 4];
                t[0] = t[1] = t[2] = v;
                t[3]               = 255;
            }
        return texels;
    }

    // A smooth bump: unit normals tilting away from the centre, encoded as n * 0.5 + 0.5.
    std::vector<uint8_t> MakeNormalMap(uint32_t size)
    {
        std::vector<uint8_t> texels(size_t(size) * size * 4);
        for (uint32_t y = 0; y < size; ++y)
            for (uint32_t x = 0; x < size; ++x)
            {
                const float nx  = (static_cast<float>(x) / static_cast<float>(size - 1) - 0.5f) * 1.2f;
                const float ny  = (static_cast<float>(y) / static_cast<float>(size - 1) - 0.5f) * 1.2f;
                const float nz  = std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));
                uint8_t*    t   = &texels[(size_t(y) * size + x) * 4];
                t[0]            = static_cast<uint8_t>(std::lround((nx * 0.5f + 0.5f) * 255.0f));
                t[1]            = static_cast<uint8_t>(std::lround((ny * 0.5f + 0.5f) * 255.0f));
                t[2]            = static_cast<uint8_t>(std::lround((nz * 0.5f + 0.5f) * 255.0f));
                t[3]            = 255;
            }
        return texels;
    }

    // Decodes one mip of the texture back to RGBA8 (channels a format lacks left as 0, alpha 255).
    std::vector<uint8_t> Decode(const CompressedTexture& texture, size_t mip)
    {
        const ContentLoader::CompressedMip& level = texture.Mips[mip];
        const uint32_t                      bx    = (level.Width + 3) / 4;
        const uint32_t                      by    = (level.Height + 3) / 4;
        const uint32_t blockBytes = ContentLoader::GetCompressedBlockBytes(texture.Format);
        std::vector<uint8_t> texels(size_t(level.Width) * level.Height * 4);
        for (uint32_t y = 0; y < by; ++y)
            for (uint32_t x = 0; x < bx; ++x)
            {
                const std::byte* block = texture.Data.data() + level.Offset + (size_t(y) * bx + x) * blockBytes;
                uint8_t          pixels[64]{};
                for (int i = 0; i < 16; ++i)
                    pixels[i * 4 + 3] = 255;
                if (texture.Format == CompressedFormat::BC5Unorm)
                    rgbcx::unpack_bc5(block, pixels, 0, 1, 4);
                else if (texture.Format == CompressedFormat::BC4Unorm)
                    rgbcx::unpack_bc4(block, pixels, 4);
                else
                    bc7decomp::unpack_bc7(block, reinterpret_cast<bc7decomp::color_rgba*>(pixels));
                for (uint32_t py = 0; py < 4; ++py)
                    for (uint32_t px = 0; px < 4; ++px)
                    {
                        const uint32_t tx = x * 4 + px;
                        const uint32_t ty = y * 4 + py;
                        if (tx < level.Width && ty < level.Height)
                            std::memcpy(&texels[(size_t(ty) * level.Width + tx) * 4], &pixels[(py * 4 + px) * 4], 4);
                    }
            }
        return texels;
    }

    // PSNR over the given channels, in dB (99 for an exact match).
    double Psnr(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, int channels)
    {
        double error = 0.0;
        size_t count = 0;
        for (size_t i = 0; i < a.size(); i += 4)
            for (int c = 0; c < channels; ++c)
            {
                const double d = double(a[i + c]) - double(b[i + c]);
                error += d * d;
                ++count;
            }
        if (error == 0.0)
            return 99.0;
        return 10.0 * std::log10(255.0 * 255.0 / (error / double(count)));
    }

    void CheckMipChain(const CompressedTexture& texture, uint32_t width, uint32_t height)
    {
        size_t offset = 0;
        for (const ContentLoader::CompressedMip& mip : texture.Mips)
        {
            CHECK(mip.Width == width);
            CHECK(mip.Height == height);
            CHECK(mip.Offset == offset);
            CHECK(mip.Size == ContentLoader::GetCompressedMipSize(texture.Format, width, height));
            offset += mip.Size;
            width  = std::max(1u, width / 2);
            height = std::max(1u, height / 2);
        }
        CHECK(texture.Data.size() == offset);
        CHECK(texture.Mips.back().Width == 1);
        CHECK(texture.Mips.back().Height == 1);
    }
}

TEST_CASE("A colour gradient and a checker encode to BC7 sRGB with a full mip chain")
{
    for (const auto& [name, texels] : { std::pair{ "gradient", MakeGradient(64, 32) },
                                        std::pair{ "checker", MakeChecker(64, 32, 8) } })
    {
        CAPTURE(name);
        const auto texture = Cooker::CompressTexture(texels, 64, 32, TextureEncoding::Color);
        REQUIRE(texture.has_value());
        CHECK(texture->Format == CompressedFormat::BC7Srgb);
        CHECK(texture->Mips.size() == 7); // 64x32 .. 1x1
        CheckMipChain(*texture, 64, 32);
        CHECK(Psnr(texels, Decode(*texture, 0), 4) > 40.0);
    }
}

TEST_CASE("Data encodes to BC7 linear or, red only, BC4")
{
    const std::vector<uint8_t> texels = MakeGradient(32, 32);
    const auto bc7 = Cooker::CompressTexture(texels, 32, 32, TextureEncoding::Data);
    REQUIRE(bc7.has_value());
    CHECK(bc7->Format == CompressedFormat::BC7Unorm);
    CHECK(Psnr(texels, Decode(*bc7, 0), 4) > 40.0);

    const auto bc4 = Cooker::CompressTexture(texels, 32, 32, TextureEncoding::Data, CompressedFormat::BC4Unorm);
    REQUIRE(bc4.has_value());
    CheckMipChain(*bc4, 32, 32);
    CHECK(bc4->Data.size() == bc7->Data.size() / 2); // 8 bytes a block against 16
    CHECK(Psnr(texels, Decode(*bc4, 0), 1) > 40.0);
}

TEST_CASE("A normal map encodes to BC5 and decodes back to unit vectors near the source")
{
    const std::vector<uint8_t> texels  = MakeNormalMap(32);
    const auto                 texture = Cooker::CompressTexture(texels, 32, 32, TextureEncoding::Normal);
    REQUIRE(texture.has_value());
    CHECK(texture->Format == CompressedFormat::BC5Unorm);
    CheckMipChain(*texture, 32, 32);
    CHECK(Psnr(texels, Decode(*texture, 0), 2) > 35.0);

    // Every level's x and y, as the shaders rebuild z from them, stay a unit vector's.
    for (size_t mip = 0; mip < texture->Mips.size(); ++mip)
    {
        const std::vector<uint8_t> decoded = Decode(*texture, mip);
        for (size_t i = 0; i < decoded.size(); i += 4)
        {
            const float x = decoded[i] / 127.5f - 1.0f;
            const float y = decoded[i + 1] / 127.5f - 1.0f;
            CHECK(x * x + y * y <= 1.02f);
        }
    }
}

TEST_CASE("A downsampled normal is unit again, not the shorter average")
{
    // Two columns tilted opposite ways average to straight up, not to a short vector.
    std::vector<uint8_t> texels(2 * 2 * 4);
    for (uint32_t i = 0; i < 4; ++i)
    {
        const bool left = i % 2 == 0;
        texels[i * 4 + 0] = left ? 37 : 218; // x = -/+0.71
        texels[i * 4 + 1] = 128;
        texels[i * 4 + 2] = 218;             // z = 0.71
        texels[i * 4 + 3] = 255;
    }
    const auto texture = Cooker::CompressTexture(texels, 2, 2, TextureEncoding::Normal);
    REQUIRE(texture.has_value());
    REQUIRE(texture->Mips.size() == 2);
    const std::vector<uint8_t> top = Decode(*texture, 1);
    CHECK(std::abs(top[0] / 127.5f - 1.0f) < 0.03f);
    CHECK(std::abs(top[1] / 127.5f - 1.0f) < 0.03f);
}

TEST_CASE("Colour mips average in linear light")
{
    // Black and white columns: the 1 x 1 mip is 50 % linear, sRGB 188, not the byte average 128.
    std::vector<uint8_t> texels(2 * 2 * 4);
    for (uint32_t i = 0; i < 4; ++i)
    {
        const uint8_t v = i % 2 == 0 ? 0 : 255;
        texels[i * 4 + 0] = texels[i * 4 + 1] = texels[i * 4 + 2] = v;
        texels[i * 4 + 3]                                         = 255;
    }
    const auto color = Cooker::CompressTexture(texels, 2, 2, TextureEncoding::Color);
    REQUIRE(color.has_value());
    CHECK(std::abs(int(Decode(*color, 1)[0]) - 188) <= 3);

    const auto data = Cooker::CompressTexture(texels, 2, 2, TextureEncoding::Data);
    REQUIRE(data.has_value());
    CHECK(std::abs(int(Decode(*data, 1)[0]) - 128) <= 3);
}

TEST_CASE("Images smaller than a block or of odd sizes encode with edge-padded blocks")
{
    const std::vector<uint8_t> one = { 200, 100, 50, 255 };
    const auto                 tiny = Cooker::CompressTexture(one, 1, 1, TextureEncoding::Color);
    REQUIRE(tiny.has_value());
    REQUIRE(tiny->Mips.size() == 1);
    CHECK(tiny->Data.size() == 16);
    const std::vector<uint8_t> decoded = Decode(*tiny, 0);
    for (int c = 0; c < 4; ++c)
        CHECK(std::abs(int(decoded[c]) - int(one[c])) <= 2);

    // The top-left 5 x 3 of a smooth gradient: an odd size, content a block can hold.
    const std::vector<uint8_t> large = MakeGradient(64, 32);
    std::vector<uint8_t>       odd;
    for (uint32_t y = 0; y < 3; ++y)
        odd.insert(odd.end(), large.begin() + y * 64 * 4, large.begin() + (y * 64 + 5) * 4);
    const auto                 texture = Cooker::CompressTexture(odd, 5, 3, TextureEncoding::Data);
    REQUIRE(texture.has_value());
    CheckMipChain(*texture, 5, 3); // 5x3, 2x1, 1x1
    CHECK(texture->Mips.size() == 3);
    CHECK(texture->Mips[0].Size == 2 * 16); // 2 x 1 blocks
    CHECK(Psnr(odd, Decode(*texture, 0), 4) > 40.0);
}

TEST_CASE("Bad input and formats an encoding does not take give nothing")
{
    const std::vector<uint8_t> texels = MakeGradient(8, 8);
    CHECK_FALSE(Cooker::CompressTexture(texels, 0, 8, TextureEncoding::Color));
    CHECK_FALSE(Cooker::CompressTexture(texels, 8, 4, TextureEncoding::Color)); // size mismatch
    CHECK_FALSE(Cooker::CompressTexture(texels, 8, 8, TextureEncoding::Color, CompressedFormat::BC7Unorm));
    CHECK_FALSE(Cooker::CompressTexture(texels, 8, 8, TextureEncoding::Normal, CompressedFormat::BC7Unorm));
    CHECK_FALSE(Cooker::CompressTexture(texels, 8, 8, TextureEncoding::Data, CompressedFormat::BC1Unorm));
}

TEST_CASE("An encoded texture round-trips through a DDS file")
{
    const auto texture = Cooker::CompressTexture(MakeGradient(16, 8), 16, 8, TextureEncoding::Color);
    REQUIRE(texture.has_value());
    const ContentLoader::DdsReadResult read = ContentLoader::ParseDds(ContentLoader::WriteDds(*texture));
    REQUIRE(read.Texture.has_value());
    CHECK(read.Texture->Format == texture->Format);
    CHECK(read.Texture->Mips.size() == texture->Mips.size());
    CHECK(read.Texture->Data == texture->Data);
}

TEST_CASE("DamagedHelmet's 2K albedo encodes, timed")
{
    // tests/ -> Cooker/ -> Tools/ -> repository root.
    const std::filesystem::path assets = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()
                                             .parent_path() / "Projects/FeatureTest/Assets";
    FS::FileSystemManager fileSystem;
    auto                  fs = std::make_unique<FS::FileSystem>();
    REQUIRE(fs->RegisterPath("assets://", assets));
    REQUIRE(fileSystem.Register(std::move(fs)));

    ContentLoader::TextureLoader loader;
    REQUIRE(loader.LoadTexture("Models/DamagedHelmet/Default_albedo.jpg", fileSystem));
    const auto  width  = static_cast<uint32_t>(loader.GetWidth());
    const auto  height = static_cast<uint32_t>(loader.GetHeight());
    const auto* data   = static_cast<const uint8_t*>(loader.GetData());

    const auto start   = std::chrono::steady_clock::now();
    const auto texture = Cooker::CompressTexture(std::span(data, size_t(width) * height * 4), width, height,
                                                 TextureEncoding::Color);
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    REQUIRE(texture.has_value());
    CheckMipChain(*texture, width, height);
    std::cout << "DamagedHelmet albedo " << width << " x " << height << ": BC7 with " << texture->Mips.size()
              << " mips in " << ms << " ms\n";
}
