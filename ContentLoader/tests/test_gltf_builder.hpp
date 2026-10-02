#pragma once

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"

#include "doctest/doctest/doctest.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Test-only helpers that build glTF files in memory, so the loader tests need no asset files.
namespace GltfTest
{
    constexpr int UNSIGNED_BYTE  = 5121;
    constexpr int UNSIGNED_SHORT = 5123;
    constexpr int UNSIGNED_INT   = 5125;
    constexpr int FLOAT          = 5126;

    template<typename T>
    std::string Bytes(const std::vector<T>& values)
    {
        std::string bytes(values.size() * sizeof(T), '\0');
        std::memcpy(bytes.data(), values.data(), bytes.size());
        return bytes;
    }

    inline std::string Base64(const std::string& bytes)
    {
        static constexpr char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string           encoded;
        for (size_t i = 0; i < bytes.size(); i += 3)
        {
            uint32_t  chunk = static_cast<uint8_t>(bytes[i]) << 16;
            const int count = static_cast<int>(std::min<size_t>(3, bytes.size() - i));
            if (count > 1)
                chunk |= static_cast<uint8_t>(bytes[i + 1]) << 8;
            if (count > 2)
                chunk |= static_cast<uint8_t>(bytes[i + 2]);
            for (int sextet = 0; sextet < 4; ++sextet)
                encoded += sextet <= count ? ALPHABET[(chunk >> (18 - 6 * sextet)) & 63] : '=';
        }
        return encoded;
    }

    // Builds a glTF file around one binary buffer: views and accessors are added in order and
    // the caller writes the meshes, nodes, skins and scenes.
    struct GltfBuilder
    {
        std::string              Bin;
        std::vector<std::string> Views;
        std::vector<std::string> Accessors;

        int AddView(const std::string& bytes, int stride = 0)
        {
            while (Bin.size() % 4 != 0)
                Bin += '\0';
            Views.push_back("{\"buffer\":0,\"byteOffset\":" + std::to_string(Bin.size()) + ",\"byteLength\":" +
                            std::to_string(bytes.size()) +
                            (stride > 0 ? ",\"byteStride\":" + std::to_string(stride) : std::string()) + "}");
            Bin += bytes;
            return static_cast<int>(Views.size()) - 1;
        }

        int AddAccessor(int view, int componentType, size_t count, const std::string& type, bool normalized = false)
        {
            Accessors.push_back("{\"bufferView\":" + std::to_string(view) + ",\"componentType\":" +
                                std::to_string(componentType) + ",\"count\":" + std::to_string(count) +
                                ",\"type\":\"" + type + "\"" + (normalized ? ",\"normalized\":true" : "") + "}");
            return static_cast<int>(Accessors.size()) - 1;
        }

        std::string Body(const std::string& rest, const std::string& uri) const
        {
            std::string json = "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"byteLength\":" +
                               std::to_string(Bin.size()) + uri + "}],\"bufferViews\":[";
            for (size_t i = 0; i < Views.size(); ++i)
                json += (i ? "," : "") + Views[i];
            json += "],\"accessors\":[";
            for (size_t i = 0; i < Accessors.size(); ++i)
                json += (i ? "," : "") + Accessors[i];
            return json + "]," + rest + "}";
        }

        std::string Gltf(const std::string& rest) const
        {
            return Body(rest, ",\"uri\":\"data:application/octet-stream;base64," + Base64(Bin) + "\"");
        }

        // The same file as binary glTF: a 12-byte header, the JSON chunk padded with spaces and
        // the BIN chunk padded with zeros.
        std::string Glb(const std::string& rest) const
        {
            std::string json = Body(rest, "");
            while (json.size() % 4 != 0)
                json += ' ';
            std::string bin = Bin;
            while (bin.size() % 4 != 0)
                bin += '\0';
            const auto word = [](uint32_t value) { return Bytes(std::vector<uint32_t>{ value }); };
            return word(0x46546C67) + word(2) + word(static_cast<uint32_t>(12 + 8 + json.size() + 8 + bin.size())) +
                   word(static_cast<uint32_t>(json.size())) + word(0x4E4F534A) + json +
                   word(static_cast<uint32_t>(bin.size())) + word(0x004E4942) + bin;
        }
    };

    inline void MountAssets(FS::FileSystemManager& manager, const std::filesystem::path& dir)
    {
        auto fs = std::make_unique<FS::FileSystem>();
        fs->RegisterPath("assets://", dir);
        REQUIRE(manager.Register(std::move(fs)));
    }
}
