#pragma once

#include "ContentLoader/api/LoadedEnvironment.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"

#include "doctest/doctest/doctest.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

// Equirect images and flat Radiance .hdr files built in the tests, shared by the environment tests.
namespace HdrTest
{
    using ContentLoader::HdrImage;

    // A width x height equirect where every texel is colour.
    inline HdrImage ConstantImage(uint32_t width, uint32_t height, const HM::Vector3& colour)
    {
        HdrImage image;
        image.Width  = width;
        image.Height = height;
        for (uint32_t i = 0; i < width * height; ++i)
            image.Pixels.insert(image.Pixels.end(), { colour.x(), colour.y(), colour.z() });
        return image;
    }

    inline void SetTexel(HdrImage& image, uint32_t x, uint32_t y, const HM::Vector3& colour)
    {
        const size_t i = (static_cast<size_t>(y) * image.Width + x) * 3;
        image.Pixels[i]     = colour.x();
        image.Pixels[i + 1] = colour.y();
        image.Pixels[i + 2] = colour.z();
    }

    // One RGBE texel of a Radiance file: a shared exponent and three 8-bit mantissas.
    inline void AppendRgbe(std::string& out, float r, float g, float b)
    {
        const float largest = std::max({ r, g, b });
        if (largest < 1e-32f)
        {
            out.append(4, '\0');
            return;
        }
        int         exponent = 0;
        const float mantissa = std::frexp(largest, &exponent) * 256.0f / largest;
        out.push_back(static_cast<char>(static_cast<unsigned char>(r * mantissa)));
        out.push_back(static_cast<char>(static_cast<unsigned char>(g * mantissa)));
        out.push_back(static_cast<char>(static_cast<unsigned char>(b * mantissa)));
        out.push_back(static_cast<char>(static_cast<unsigned char>(exponent + 128)));
    }

    // A flat (not run-length encoded) Radiance .hdr of the image.
    inline std::string WriteRadiance(const HdrImage& image)
    {
        std::string out = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y " + std::to_string(image.Height) + " +X "
                        + std::to_string(image.Width) + "\n";
        for (size_t i = 0; i < image.Pixels.size(); i += 3)
            AppendRgbe(out, image.Pixels[i], image.Pixels[i + 1], image.Pixels[i + 2]);
        return out;
    }

    inline void WriteFile(const std::filesystem::path& path, const std::string& bytes)
    {
        std::ofstream file(path, std::ios::binary);
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    inline void MountAssets(FS::FileSystemManager& manager, const std::filesystem::path& dir)
    {
        auto fs = std::make_unique<FS::FileSystem>();
        fs->RegisterPath("assets://", dir);
        REQUIRE(manager.Register(std::move(fs)));
    }

}
