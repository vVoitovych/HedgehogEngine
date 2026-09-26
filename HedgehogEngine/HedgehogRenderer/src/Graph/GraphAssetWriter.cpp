#include "HedgehogRenderer/Graph/GraphAssetWriter.hpp"

#include "HedgehogRenderer/Graph/GraphAssetVocabulary.hpp"

#include "yaml-cpp/yaml.h"

#include <fstream>
#include <system_error>

namespace Renderer
{
    namespace
    {
        constexpr const char* UNDEFINED_FORMAT_NAME = "Undefined";

        std::string FormatName(RHI::Format format)
        {
            const std::optional<std::string_view> name = GetFormatName(format);
            return std::string(name ? *name : UNDEFINED_FORMAT_NAME);
        }

        void EmitFormatAndSize(YAML::Emitter& out, RHI::Format format, const RGSizePolicy& size)
        {
            out << YAML::Key << "format" << YAML::Value << FormatName(format);
            out << YAML::Key << "size" << YAML::Value << SizePolicyToString(size);
        }

        template<typename Entry, typename EmitFields>
        void EmitSequence(YAML::Emitter& out, const char* key, const std::vector<Entry>& entries,
                          EmitFields&& emitFields)
        {
            if (entries.empty())
                return;
            out << YAML::Key << key << YAML::Value << YAML::BeginSeq;
            for (const Entry& entry : entries)
            {
                out << YAML::BeginMap;
                emitFields(entry);
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;
        }

        template<typename Entry, typename KeyOf, typename ValueOf>
        void EmitScalarMap(YAML::Emitter& out, const char* key, const std::vector<Entry>& entries,
                           KeyOf&& keyOf, ValueOf&& valueOf)
        {
            if (entries.empty())
                return;
            out << YAML::Key << key << YAML::Value << YAML::BeginMap;
            for (const Entry& entry : entries)
                out << YAML::Key << keyOf(entry) << YAML::Value << valueOf(entry);
            out << YAML::EndMap;
        }
    }

    std::string WriteGraphAsset(const GraphAsset& asset)
    {
        // Version 1 has no imports key, so an asset that declares imports is written as the current
        // schema whatever its Version says.
        const uint32_t version = asset.Imports.empty() ? asset.Version : GRAPH_ASSET_SCHEMA_VERSION;

        YAML::Emitter out;
        out.SetIndent(2);
        out << YAML::BeginMap;
        out << YAML::Key << "version" << YAML::Value << version;

        EmitSequence(out, "outputs", asset.Outputs, [&](const GraphAssetOutput& output)
        {
            out << YAML::Key << "slot" << YAML::Value << output.Slot;
            out << YAML::Key << "name" << YAML::Value << output.Name;
            EmitFormatAndSize(out, output.Format, output.Size);
        });
        EmitSequence(out, "resources", asset.Resources, [&](const GraphAssetResource& resource)
        {
            out << YAML::Key << "name" << YAML::Value << resource.Name;
            EmitFormatAndSize(out, resource.Format, resource.Size);
        });
        EmitSequence(out, "imports", asset.Imports, [&](const GraphAssetImport& import)
        {
            out << YAML::Key << "name" << YAML::Value << import.Name;
            out << YAML::Key << "format" << YAML::Value << FormatName(import.Format);
        });
        EmitSequence(out, "passes", asset.Passes, [&](const GraphAssetPass& pass)
        {
            out << YAML::Key << "type" << YAML::Value << pass.Type;
            out << YAML::Key << "name" << YAML::Value << pass.Name;
            EmitScalarMap(out, "bindings", pass.Bindings,
                          [](const GraphAssetBinding& binding) { return binding.Slot; },
                          [](const GraphAssetBinding& binding) { return binding.Resource; });
            EmitScalarMap(out, "parameters", pass.Parameters,
                          [](const GraphAssetParameter& parameter) { return parameter.Name; },
                          [](const GraphAssetParameter& parameter) { return parameter.Value; });
        });

        out << YAML::EndMap;
        return std::string(out.c_str()) + "\n";
    }

    bool WriteGraphAssetFile(const GraphAsset& asset, const std::filesystem::path& file, std::string* error)
    {
        const auto fail = [&](const std::string& message)
        {
            if (error)
                *error = message;
            return false;
        };

        std::filesystem::path temporary = file;
        temporary += ".tmp";
        {
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            if (!stream)
                return fail("cannot open '" + temporary.string() + "' for writing");
            stream << WriteGraphAsset(asset);
            if (!stream.flush())
                return fail("cannot write '" + temporary.string() + "'");
        }

        std::error_code renameError;
        std::filesystem::rename(temporary, file, renameError);
        if (renameError)
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return fail("cannot replace '" + file.string() + "': " + renameError.message());
        }
        return true;
    }
}
