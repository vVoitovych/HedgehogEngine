#include "TextureCompression.hpp"

#include "bc7enc.h"
#include "rgbcx.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <execution>
#include <mutex>
#include <numeric>
#include <vector>

namespace Cooker
{
    namespace
    {
        using ContentLoader::CompressedFormat;
        using ContentLoader::TextureEncoding;

        constexpr uint32_t BLOCK_SIZE = 4;

        struct Image
        {
            uint32_t             Width  = 0;
            uint32_t             Height = 0;
            std::vector<uint8_t> Texels; // RGBA8
        };

        void InitEncoders()
        {
            static std::once_flag once;
            std::call_once(once,
                           []
                           {
                               bc7enc_compress_block_init();
                               rgbcx::init();
                           });
        }

        float SrgbToLinear(float c)
        {
            return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
        }

        float LinearToSrgb(float c)
        {
            return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
        }

        uint8_t ToByte(float unit)
        {
            return static_cast<uint8_t>(std::lround(std::clamp(unit, 0.0f, 1.0f) * 255.0f));
        }

        const std::array<float, 256>& SrgbTable()
        {
            static const std::array<float, 256> table = []
            {
                std::array<float, 256> values{};
                for (size_t i = 0; i < values.size(); ++i)
                    values[i] = SrgbToLinear(static_cast<float>(i) / 255.0f);
                return values;
            }();
            return table;
        }

        // The next level down: each texel the average of up to 2 x 2 of the level above.
        Image Downsample(const Image& source, TextureEncoding encoding)
        {
            Image next;
            next.Width  = std::max(1u, source.Width / 2);
            next.Height = std::max(1u, source.Height / 2);
            next.Texels.resize(size_t(next.Width) * next.Height * 4);
            const auto& srgb = SrgbTable();
            for (uint32_t y = 0; y < next.Height; ++y)
                for (uint32_t x = 0; x < next.Width; ++x)
                {
                    const uint32_t xs[2] = { std::min(2 * x, source.Width - 1), std::min(2 * x + 1, source.Width - 1) };
                    const uint32_t ys[2] = { std::min(2 * y, source.Height - 1), std::min(2 * y + 1, source.Height - 1) };
                    std::array<float, 4> sum{};
                    for (uint32_t sy : ys)
                        for (uint32_t sx : xs)
                        {
                            const uint8_t* texel = &source.Texels[(size_t(sy) * source.Width + sx) * 4];
                            for (int c = 0; c < 4; ++c)
                            {
                                const float byte = static_cast<float>(texel[c]);
                                if (encoding == TextureEncoding::Color && c < 3)
                                    sum[c] += srgb[texel[c]];
                                else if (encoding == TextureEncoding::Normal && c < 3)
                                    sum[c] += byte / 127.5f - 1.0f;
                                else
                                    sum[c] += byte / 255.0f;
                            }
                        }
                    for (float& value : sum)
                        value *= 0.25f;
                    if (encoding == TextureEncoding::Normal)
                    {
                        const float length = std::sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
                        for (int c = 0; c < 3; ++c)
                            sum[c] = (length > 0.0f ? sum[c] / length : (c == 2 ? 1.0f : 0.0f)) * 0.5f + 0.5f;
                    }
                    else if (encoding == TextureEncoding::Color)
                    {
                        for (int c = 0; c < 3; ++c)
                            sum[c] = LinearToSrgb(sum[c]);
                    }
                    uint8_t* out = &next.Texels[(size_t(y) * next.Width + x) * 4];
                    for (int c = 0; c < 4; ++c)
                        out[c] = ToByte(sum[c]);
                }
            return next;
        }

        void EncodeLevel(const Image& level, CompressedFormat format, TextureEncoding encoding, std::byte* out)
        {
            bc7enc_compress_block_params bc7;
            bc7enc_compress_block_params_init(&bc7);
            bc7.m_max_partitions_mode = BC7_MAX_PARTITIONS;
            bc7.m_uber_level          = BC7_UBER_LEVEL;
            if (encoding != TextureEncoding::Color)
                bc7enc_compress_block_params_init_linear_weights(&bc7);

            const uint32_t blocksX    = (level.Width + BLOCK_SIZE - 1) / BLOCK_SIZE;
            const uint32_t blocksY    = (level.Height + BLOCK_SIZE - 1) / BLOCK_SIZE;
            const uint32_t blockBytes = ContentLoader::GetCompressedBlockBytes(format);

            std::vector<uint32_t> rows(blocksY);
            std::iota(rows.begin(), rows.end(), 0u);
            std::for_each(std::execution::par, rows.begin(), rows.end(),
                          [&](uint32_t by)
                          {
                              std::array<uint8_t, BLOCK_SIZE * BLOCK_SIZE * 4> pixels{};
                              for (uint32_t bx = 0; bx < blocksX; ++bx)
                              {
                                  for (uint32_t py = 0; py < BLOCK_SIZE; ++py)
                                      for (uint32_t px = 0; px < BLOCK_SIZE; ++px)
                                      {
                                          const uint32_t sx = std::min(bx * BLOCK_SIZE + px, level.Width - 1);
                                          const uint32_t sy = std::min(by * BLOCK_SIZE + py, level.Height - 1);
                                          std::memcpy(&pixels[(py * BLOCK_SIZE + px) * 4],
                                                      &level.Texels[(size_t(sy) * level.Width + sx) * 4], 4);
                                      }
                                  std::byte* block = out + (size_t(by) * blocksX + bx) * blockBytes;
                                  switch (format)
                                  {
                                  case CompressedFormat::BC4Unorm:
                                      rgbcx::encode_bc4(block, pixels.data(), 4);
                                      break;
                                  case CompressedFormat::BC5Unorm:
                                      rgbcx::encode_bc5(block, pixels.data(), 0, 1, 4);
                                      break;
                                  default:
                                      bc7enc_compress_block(block, pixels.data(), &bc7);
                                      break;
                                  }
                              }
                          });
        }

        bool TakesFormat(TextureEncoding encoding, CompressedFormat format)
        {
            switch (encoding)
            {
            case TextureEncoding::Color:
                return format == CompressedFormat::BC7Srgb;
            case TextureEncoding::Data:
                return format == CompressedFormat::BC7Unorm || format == CompressedFormat::BC4Unorm;
            case TextureEncoding::Normal:
                return format == CompressedFormat::BC5Unorm;
            }
            return false;
        }
    }

    std::optional<ContentLoader::CompressedTexture> CompressTexture(std::span<const uint8_t> rgba, uint32_t width,
                                                                    uint32_t height, TextureEncoding encoding,
                                                                    std::optional<CompressedFormat> format)
    {
        const CompressedFormat chosen = format.value_or(ContentLoader::GetEncodingFormat(encoding));
        if (width == 0 || height == 0 || rgba.size() != size_t(width) * height * 4 || !TakesFormat(encoding, chosen))
            return std::nullopt;
        InitEncoders();

        ContentLoader::CompressedTexture texture;
        texture.Format = chosen;
        texture.Width  = width;
        texture.Height = height;

        Image level{ width, height, std::vector<uint8_t>(rgba.begin(), rgba.end()) };
        while (true)
        {
            const size_t size   = ContentLoader::GetCompressedMipSize(chosen, level.Width, level.Height);
            const size_t offset = texture.Data.size();
            texture.Mips.push_back({ level.Width, level.Height, offset, size });
            texture.Data.resize(offset + size);
            EncodeLevel(level, chosen, encoding, texture.Data.data() + offset);
            if (level.Width == 1 && level.Height == 1)
                break;
            level = Downsample(level, encoding);
        }
        return texture;
    }
}
