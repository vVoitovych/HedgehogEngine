#include "HedgehogEngine/api/Terrain/TerrainData.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <initializer_list>

namespace HedgehogEngine
{
    namespace
    {
        constexpr float HEIGHT_STEPS = 65535.0f;

        constexpr std::string_view BASE64_ALPHABET =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        // A parse failure, carrying the message up to ParseTerrain.
        struct ParseError
        {
            std::string Message;
        };

        std::string EncodeBase64(const std::vector<uint8_t>& bytes)
        {
            std::string text;
            text.reserve((bytes.size() + 2) / 3 * 4);
            for (size_t i = 0; i < bytes.size(); i += 3)
            {
                const size_t   count = std::min<size_t>(3, bytes.size() - i);
                const uint32_t block = (uint32_t{ bytes[i] } << 16) |
                                       (count > 1 ? uint32_t{ bytes[i + 1] } << 8 : 0u) |
                                       (count > 2 ? uint32_t{ bytes[i + 2] } : 0u);
                text += BASE64_ALPHABET[(block >> 18) & 63];
                text += BASE64_ALPHABET[(block >> 12) & 63];
                text += count > 1 ? BASE64_ALPHABET[(block >> 6) & 63] : '=';
                text += count > 2 ? BASE64_ALPHABET[block & 63] : '=';
            }
            return text;
        }

        // Padded base64 only; nullopt for any other character, a length that is not a multiple of
        // four or padding before the end.
        std::optional<std::vector<uint8_t>> DecodeBase64(std::string_view text)
        {
            if (text.size() % 4 != 0)
                return std::nullopt;
            std::vector<uint8_t> bytes;
            bytes.reserve(text.size() / 4 * 3);
            for (size_t i = 0; i < text.size(); i += 4)
            {
                uint32_t block   = 0;
                int      padding = 0;
                for (size_t c = 0; c < 4; ++c)
                {
                    const char character = text[i + c];
                    if (character == '=' && i + 4 == text.size() && c >= 2)
                    {
                        ++padding;
                        block <<= 6;
                        continue;
                    }
                    const size_t value = BASE64_ALPHABET.find(character);
                    if (value == std::string_view::npos || padding > 0)
                        return std::nullopt;
                    block = (block << 6) | static_cast<uint32_t>(value);
                }
                bytes.push_back(static_cast<uint8_t>(block >> 16));
                if (padding < 2)
                    bytes.push_back(static_cast<uint8_t>(block >> 8));
                if (padding < 1)
                    bytes.push_back(static_cast<uint8_t>(block));
            }
            return bytes;
        }

        std::string Scalar(const YAML::Node& node, const std::string& where)
        {
            if (!node.IsScalar())
                throw ParseError{ where + ": expected a single value" };
            return node.Scalar();
        }

        float Number(const YAML::Node& node, const std::string& where)
        {
            const std::string text = Scalar(node, where);
            try
            {
                const float value = node.as<float>();
                if (!std::isfinite(value))
                    throw ParseError{ where + ": '" + text + "' is not a finite number" };
                return value;
            }
            catch (const YAML::Exception&)
            {
                throw ParseError{ where + ": '" + text + "' is not a number" };
            }
        }

        uint32_t WholeNumber(const YAML::Node& node, const std::string& where)
        {
            const float value = Number(node, where);
            if (value < 0.0f || value != std::floor(value) || value > 1.0e9f)
                throw ParseError{ where + ": '" + node.Scalar() + "' is not a whole number" };
            return static_cast<uint32_t>(value);
        }

        // Every key of node is one of keys, and every one of them is present.
        void CheckKeys(const YAML::Node& node, std::initializer_list<const char*> keys, const std::string& where)
        {
            if (!node.IsMap())
                throw ParseError{ where + ": expected a map" };
            for (const auto& entry : node)
            {
                const std::string key = entry.first.Scalar();
                if (std::none_of(keys.begin(), keys.end(), [&](const char* known) { return key == known; }))
                    throw ParseError{ where + ": unknown key '" + key + "'" };
            }
            for (const char* key : keys)
                if (!node[key])
                    throw ParseError{ where + ": " + key + " is missing" };
        }

        std::vector<uint8_t> Grid(const YAML::Node& node, size_t expectedBytes, const char* key)
        {
            const std::optional<std::vector<uint8_t>> bytes = DecodeBase64(Scalar(node, key));
            if (!bytes)
                throw ParseError{ std::string(key) + ": not base64" };
            if (bytes->size() != expectedBytes)
                throw ParseError{ std::string(key) + ": holds " + std::to_string(bytes->size()) + " bytes, but the grid needs " +
                                  std::to_string(expectedBytes) };
            return *bytes;
        }

        TerrainLayer ReadLayer(const YAML::Node& node, const std::string& where)
        {
            CheckKeys(node, { "BaseColor", "NormalMap", "Tiling", "Metallic", "Roughness" }, where);
            TerrainLayer layer;
            layer.BaseColor = node["BaseColor"].IsNull() ? std::string{} : Scalar(node["BaseColor"], where + ", BaseColor");
            layer.NormalMap = node["NormalMap"].IsNull() ? std::string{} : Scalar(node["NormalMap"], where + ", NormalMap");
            layer.Tiling    = Number(node["Tiling"], where + ", Tiling");
            layer.Metallic  = Number(node["Metallic"], where + ", Metallic");
            layer.Roughness = Number(node["Roughness"], where + ", Roughness");
            return layer;
        }

        void WriteLayer(YAML::Emitter& out, const TerrainLayer& layer)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "BaseColor" << YAML::Value << layer.BaseColor;
            out << YAML::Key << "NormalMap" << YAML::Value << layer.NormalMap;
            out << YAML::Key << "Tiling" << YAML::Value << layer.Tiling;
            out << YAML::Key << "Metallic" << YAML::Value << layer.Metallic;
            out << YAML::Key << "Roughness" << YAML::Value << layer.Roughness;
            out << YAML::EndMap;
        }

        size_t SampleCount(uint32_t resolution)
        {
            return static_cast<size_t>(resolution) * resolution;
        }
    }

    bool IsValidTerrainResolution(uint32_t resolution)
    {
        return resolution >= MIN_TERRAIN_RESOLUTION && resolution <= MAX_TERRAIN_RESOLUTION &&
               std::has_single_bit(resolution - 1);
    }

    bool IsValidSplatResolution(uint32_t resolution)
    {
        return resolution >= MIN_SPLAT_RESOLUTION && resolution <= MAX_SPLAT_RESOLUTION && std::has_single_bit(resolution);
    }

    TerrainData MakeDefaultTerrain()
    {
        TerrainData data;
        data.Heights.assign(SampleCount(data.Resolution), 0.0f);
        data.Splat.assign(SampleCount(data.SplatResolution) * 4, 0);
        for (size_t texel = 0; texel < data.Splat.size(); texel += 4)
            data.Splat[texel] = 255;
        return data;
    }

    TerrainReadResult ParseTerrain(std::string_view text)
    {
        TerrainReadResult result;
        try
        {
            const YAML::Node root = YAML::Load(std::string(text));
            if (!root.IsMap())
                throw ParseError{ "the file is not a map of Version, Size, Resolution, HeightScale, SplatResolution, "
                                  "Layers, Heights and Splat" };
            if (!root["Version"])
                throw ParseError{ "the file: Version is missing" };
            const std::string version = Scalar(root["Version"], "Version");
            if (version !=std::to_string(TERRAIN_FILE_VERSION))
                throw ParseError{ "Version " + version + " is not supported (this build reads version " +
                                  std::to_string(TERRAIN_FILE_VERSION) + ")" };
            CheckKeys(root, { "Version", "Size", "Resolution", "HeightScale", "SplatResolution", "Layers", "Heights", "Splat" },
                      "the file");

            TerrainData data;
            data.Size = Number(root["Size"], "Size");
            if (data.Size <= 0.0f || data.Size > MAX_TERRAIN_SIZE)
                throw ParseError{ "Size: " + root["Size"].Scalar() + " is not above 0 and at most " +
                                  std::to_string(static_cast<int>(MAX_TERRAIN_SIZE)) };
            data.HeightScale = Number(root["HeightScale"], "HeightScale");
            if (data.HeightScale <= 0.0f)
                throw ParseError{ "HeightScale: " + root["HeightScale"].Scalar() + " is not above 0" };
            data.Resolution = WholeNumber(root["Resolution"], "Resolution");
            if (!IsValidTerrainResolution(data.Resolution))
                throw ParseError{ "Resolution: " + std::to_string(data.Resolution) + " is not 2^n + 1 from " +
                                  std::to_string(MIN_TERRAIN_RESOLUTION) + " to " + std::to_string(MAX_TERRAIN_RESOLUTION) };
            data.SplatResolution = WholeNumber(root["SplatResolution"], "SplatResolution");
            if (!IsValidSplatResolution(data.SplatResolution))
                throw ParseError{ "SplatResolution: " + std::to_string(data.SplatResolution) + " is not a power of two from " +
                                  std::to_string(MIN_SPLAT_RESOLUTION) + " to " + std::to_string(MAX_SPLAT_RESOLUTION) };

            const YAML::Node layers = root["Layers"];
            if (!layers.IsSequence() || layers.size() != TERRAIN_LAYER_COUNT)
                throw ParseError{ "Layers: expected a list of exactly " + std::to_string(TERRAIN_LAYER_COUNT) + " layers" };
            for (uint32_t i = 0; i < TERRAIN_LAYER_COUNT; ++i)
                data.Layers[i] = ReadLayer(layers[i], "layer " + std::to_string(i));

            const std::vector<uint8_t> heights = Grid(root["Heights"], SampleCount(data.Resolution) * 2, "Heights");
            data.Heights.resize(SampleCount(data.Resolution));
            for (size_t i = 0; i < data.Heights.size(); ++i)
            {
                const uint32_t step = uint32_t{ heights[i * 2] } | (uint32_t{ heights[i * 2 + 1] } << 8);
                data.Heights[i]     = static_cast<float>(step) / HEIGHT_STEPS * data.HeightScale;
            }
            data.Splat = Grid(root["Splat"], SampleCount(data.SplatResolution) * 4, "Splat");
            result.Data = std::move(data);
        }
        catch (const ParseError& error)
        {
            result.Error = error.Message;
        }
        catch (const YAML::Exception& error)
        {
            result.Error = error.what();
        }
        return result;
    }

    std::string WriteTerrain(const TerrainData& data)
    {
        const size_t         sampleCount = SampleCount(data.Resolution);
        std::vector<uint8_t> heights(sampleCount * 2, 0);
        if (data.Heights.size() == sampleCount && data.HeightScale > 0.0f)
        {
            for (size_t i = 0; i < sampleCount; ++i)
            {
                const float    fraction = std::clamp(data.Heights[i] / data.HeightScale, 0.0f, 1.0f);
                const uint32_t step     = static_cast<uint32_t>(std::lround(fraction * HEIGHT_STEPS));
                heights[i * 2]          = static_cast<uint8_t>(step);
                heights[i * 2 + 1]      = static_cast<uint8_t>(step >> 8);
            }
        }

        std::vector<uint8_t> splat = data.Splat;
        if (splat.size() != SampleCount(data.SplatResolution) * 4)
        {
            splat.assign(SampleCount(data.SplatResolution) * 4, 0);
            for (size_t texel = 0; texel < splat.size(); texel += 4)
                splat[texel] = 255;
        }

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Version" << YAML::Value << TERRAIN_FILE_VERSION;
        out << YAML::Key << "Size" << YAML::Value << data.Size;
        out << YAML::Key << "Resolution" << YAML::Value << data.Resolution;
        out << YAML::Key << "HeightScale" << YAML::Value << data.HeightScale;
        out << YAML::Key << "SplatResolution" << YAML::Value << data.SplatResolution;
        out << YAML::Key << "Layers" << YAML::Value << YAML::BeginSeq;
        for (const TerrainLayer& layer : data.Layers)
            WriteLayer(out, layer);
        out << YAML::EndSeq;
        out << YAML::Key << "Heights" << YAML::Value << EncodeBase64(heights);
        out << YAML::Key << "Splat" << YAML::Value << EncodeBase64(splat);
        out << YAML::EndMap;
        return std::string(out.c_str()) + "\n";
    }
}
