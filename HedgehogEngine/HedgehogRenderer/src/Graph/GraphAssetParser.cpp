#include "HedgehogRenderer/Graph/GraphAssetParser.hpp"

#include "HedgehogRenderer/Graph/GraphAssetVocabulary.hpp"

#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <charconv>
#include <initializer_list>
#include <optional>
#include <string>
#include <unordered_map>

namespace Renderer
{
    namespace
    {
        using Errors = std::vector<GraphAssetError>;

        std::string LineSuffix(const YAML::Node& node)
        {
            if (!node)
                return {};
            const YAML::Mark mark = node.Mark();
            if (mark.is_null())
                return {};
            return " (line " + std::to_string(mark.line + 1) + ")";
        }

        void AddError(Errors& errors, const std::string& path, const std::string& message,
                      const YAML::Node& node)
        {
            errors.push_back({ path + ": " + message + LineSuffix(node), path });
        }

        std::optional<uint32_t> ParseUnsigned(const std::string& text)
        {
            uint32_t value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size())
                return std::nullopt;
            return value;
        }

        // Every map in the schema is closed: a misspelled key is an error, never silently ignored.
        void RejectUnknownKeys(const YAML::Node& map, std::initializer_list<std::string_view> allowed,
                               const std::string& path, Errors& errors)
        {
            for (const auto& entry : map)
            {
                const std::string key = entry.first.IsScalar() ? entry.first.Scalar() : std::string{};
                if (std::find(allowed.begin(), allowed.end(), key) == allowed.end())
                    AddError(errors, path, "unknown key '" + key + "'", entry.first);
            }
        }

        // Returns the scalar under key, or std::nullopt after recording why it is unusable.
        std::optional<std::string> RequireScalar(const YAML::Node& map, const char* key,
                                                 const std::string& path, Errors& errors)
        {
            const YAML::Node value = map[key];
            if (!value)
            {
                AddError(errors, path, std::string("missing required key '") + key + "'", map);
                return std::nullopt;
            }
            if (!value.IsScalar() || value.Scalar().empty())
            {
                AddError(errors, path + "." + key, "must be a non-empty scalar", value);
                return std::nullopt;
            }
            return value.Scalar();
        }

        // Absent means empty; present but not a sequence is an error.
        std::optional<YAML::Node> OptionalSequence(const YAML::Node& root, const char* key, Errors& errors)
        {
            const YAML::Node value = root[key];
            if (!value || value.IsNull())
                return std::nullopt;
            if (!value.IsSequence())
            {
                AddError(errors, key, "must be a sequence", value);
                return std::nullopt;
            }
            return value;
        }

        // Reads the scalar under key and maps it through a vocabulary resolver; an unresolvable
        // string is reported with what was expected, never replaced by a default.
        template<typename Resolver>
        auto ParseVocabularyField(const YAML::Node& map, const char* key, const std::string& path,
                                  Resolver resolve, const char* noun, const char* hint, Errors& errors)
            -> decltype(resolve(std::string_view{}))
        {
            const std::optional<std::string> text = RequireScalar(map, key, path, errors);
            if (!text)
                return std::nullopt;
            auto value = resolve(*text);
            if (!value)
            {
                AddError(errors, path + "." + key,
                         std::string("unknown ") + noun + " '" + *text + "'" + hint, map[key]);
            }
            return value;
        }

        std::string IndexedPath(const char* section, size_t index)
        {
            return std::string(section) + "[" + std::to_string(index) + "]";
        }

        // The name/format/size triple outputs and resources share. Every field is checked, so one
        // entry reports all of its problems at once.
        std::optional<GraphAssetResource> ParseResourceFields(const YAML::Node& entry, const std::string& path,
                                                              Errors& errors)
        {
            const std::optional<std::string>  name   = RequireScalar(entry, "name", path, errors);
            const std::optional<RHI::Format>  format =
                ParseVocabularyField(entry, "format", path, ResolveFormat, "format", "", errors);
            const std::optional<RGSizePolicy> size   = ParseVocabularyField(entry, "size", path, ResolveSizePolicy,
                "size policy", " (expected Absolute(w, h), RelativeToResult(scale) or RelativeToSwapchain(scale))",
                errors);
            if (!name || !format || !size)
                return std::nullopt;
            return GraphAssetResource{ *name, *format, *size };
        }

        // slotOwner maps each slot already claimed to the index of the entry that claimed it.
        std::optional<uint32_t> ParseSlotField(const YAML::Node& entry, const std::string& path, size_t index,
                                               std::unordered_map<uint32_t, size_t>& slotOwner, Errors& errors)
        {
            const std::optional<std::string> text = RequireScalar(entry, "slot", path, errors);
            if (!text)
                return std::nullopt;
            const std::optional<uint32_t> slot = ParseUnsigned(*text);
            if (!slot)
            {
                AddError(errors, path + ".slot",
                         "malformed output slot: '" + *text + "' is not a non-negative integer", entry["slot"]);
                return std::nullopt;
            }
            if (const auto owner = slotOwner.find(*slot); owner != slotOwner.end())
            {
                AddError(errors, path + ".slot", "duplicate output slot " + *text + ", already declared by "
                         + IndexedPath("outputs", owner->second), entry["slot"]);
                return std::nullopt;
            }
            slotOwner.emplace(*slot, index);
            return slot;
        }

        void ParseOutputs(const YAML::Node& sequence, GraphAsset& asset, Errors& errors)
        {
            std::unordered_map<uint32_t, size_t> slotOwner;
            for (size_t i = 0; i < sequence.size(); ++i)
            {
                const YAML::Node entry = sequence[i];
                const std::string path = IndexedPath("outputs", i);
                if (!entry.IsMap())
                {
                    AddError(errors, path, "malformed output slot: expected a map", entry);
                    continue;
                }
                RejectUnknownKeys(entry, { "slot", "name", "format", "size" }, path, errors);

                const std::optional<uint32_t>           slot   = ParseSlotField(entry, path, i, slotOwner, errors);
                const std::optional<GraphAssetResource> fields = ParseResourceFields(entry, path, errors);
                if (slot && fields)
                    asset.Outputs.push_back({ *slot, fields->Name, fields->Format, fields->Size });
            }

            // Views bind by slot index, so the slots must be exactly 0..N-1 (RENDERING.md 5.2). Only
            // checked once every entry has a valid, unique slot; otherwise the problem is already
            // reported against the entry itself.
            if (slotOwner.size() != sequence.size())
                return;
            for (uint32_t slot = 0; slot < static_cast<uint32_t>(sequence.size()); ++slot)
            {
                if (slotOwner.find(slot) == slotOwner.end())
                {
                    AddError(errors, "outputs", "malformed output slot list: slots must run contiguously from 0, "
                             "but slot " + std::to_string(slot) + " is not declared", sequence);
                }
            }

            std::sort(asset.Outputs.begin(), asset.Outputs.end(),
                      [](const GraphAssetOutput& lhs, const GraphAssetOutput& rhs) { return lhs.Slot < rhs.Slot; });
        }

        void ParseResources(const YAML::Node& sequence, GraphAsset& asset, Errors& errors)
        {
            for (size_t i = 0; i < sequence.size(); ++i)
            {
                const YAML::Node entry = sequence[i];
                const std::string path = IndexedPath("resources", i);
                if (!entry.IsMap())
                {
                    AddError(errors, path, "malformed resource: expected a map", entry);
                    continue;
                }
                RejectUnknownKeys(entry, { "name", "format", "size" }, path, errors);

                if (const std::optional<GraphAssetResource> resource = ParseResourceFields(entry, path, errors))
                    asset.Resources.push_back(*resource);
            }
        }

        void ParseImports(const YAML::Node& sequence, GraphAsset& asset, Errors& errors)
        {
            for (size_t i = 0; i < sequence.size(); ++i)
            {
                const YAML::Node entry = sequence[i];
                const std::string path = IndexedPath("imports", i);
                if (!entry.IsMap())
                {
                    AddError(errors, path, "malformed import: expected a map", entry);
                    continue;
                }
                RejectUnknownKeys(entry, { "name", "format" }, path, errors);

                const std::optional<std::string> name   = RequireScalar(entry, "name", path, errors);
                const std::optional<RHI::Format> format =
                    ParseVocabularyField(entry, "format", path, ResolveFormat, "format", "", errors);
                if (name && format)
                    asset.Imports.push_back({ *name, *format });
            }
        }

        // A flat map of non-empty scalar keys to non-empty scalar values, in document order.
        // Nested values are rejected: parameters and bindings are never expressions. Duplicate keys
        // never reach here: yaml-cpp rejects them while loading ("map keys must be unique").
        template<typename Entry>
        std::vector<Entry> ParseScalarMap(const YAML::Node& map, const std::string& path,
                                          const char* what, Errors& errors, bool& isValid)
        {
            std::vector<Entry> result;
            if (!map || map.IsNull())
                return result;
            if (!map.IsMap())
            {
                AddError(errors, path, std::string("malformed pass: ") + what + " must be a map", map);
                isValid = false;
                return result;
            }

            for (const auto& entry : map)
            {
                const bool keyIsValid   = entry.first.IsScalar() && !entry.first.Scalar().empty();
                const bool valueIsValid = entry.second.IsScalar() && !entry.second.Scalar().empty();
                const std::string key   = keyIsValid ? entry.first.Scalar() : std::string{};
                if (!keyIsValid || !valueIsValid)
                {
                    AddError(errors, path,
                             std::string("malformed pass: ") + what + " entry '" + key
                             + "' must map a non-empty scalar to a non-empty scalar",
                             entry.first);
                    isValid = false;
                    continue;
                }
                result.push_back({ key, entry.second.Scalar() });
            }
            return result;
        }

        void ParsePasses(const YAML::Node& sequence, GraphAsset& asset, Errors& errors)
        {
            std::unordered_map<std::string, size_t> nameOwner;
            for (size_t i = 0; i < sequence.size(); ++i)
            {
                const YAML::Node entry = sequence[i];
                std::string path = IndexedPath("passes", i);
                if (!entry.IsMap())
                {
                    AddError(errors, path, "malformed pass: expected a map", entry);
                    continue;
                }

                // Name the pass in every later message when it has a usable name.
                const YAML::Node nameNode = entry["name"];
                if (nameNode && nameNode.IsScalar() && !nameNode.Scalar().empty())
                    path += " ('" + nameNode.Scalar() + "')";

                RejectUnknownKeys(entry, { "type", "name", "bindings", "parameters" }, path, errors);

                const std::optional<std::string> type = RequireScalar(entry, "type", path, errors);
                const std::optional<std::string> name = RequireScalar(entry, "name", path, errors);

                bool isValid = type.has_value() && name.has_value();
                if (name)
                {
                    if (const auto owner = nameOwner.find(*name); owner != nameOwner.end())
                    {
                        AddError(errors, path, "duplicate pass name '" + *name + "', already used by "
                                 + IndexedPath("passes", owner->second), nameNode);
                        isValid = false;
                    }
                    else
                    {
                        nameOwner.emplace(*name, i);
                    }
                }

                GraphAssetPass pass;
                pass.Bindings   = ParseScalarMap<GraphAssetBinding>(entry["bindings"], path + ".bindings",
                                                                    "binding", errors, isValid);
                pass.Parameters = ParseScalarMap<GraphAssetParameter>(entry["parameters"], path + ".parameters",
                                                                      "parameter", errors, isValid);
                if (!isValid)
                    continue;

                pass.Type = *type;
                pass.Name = *name;
                asset.Passes.push_back(std::move(pass));
            }
        }

        // yaml-cpp reports syntax errors by throwing; they are caught here so no exception leaves
        // the parser (CODING_CONVENTIONS.md 9.3).
        std::optional<YAML::Node> LoadDocument(std::string_view yamlText, Errors& errors)
        {
            try
            {
                return YAML::Load(std::string(yamlText));
            }
            catch (const YAML::Exception& exception)
            {
                errors.push_back({ std::string("graph asset is not valid YAML: ") + exception.what(), "" });
                return std::nullopt;
            }
        }

        // Outputs, resources and imports share one namespace: a binding names any of them, so an
        // ambiguous name could never be resolved by instantiation. The error has no line: it
        // involves two entries.
        void RejectDuplicateResourceNames(const GraphAsset& asset, Errors& errors)
        {
            std::unordered_map<std::string, std::string> owner; // name -> path that declared it
            const auto claim = [&](const std::string& name, const std::string& path)
            {
                if (const auto existing = owner.find(name); existing != owner.end())
                {
                    AddError(errors, path, "duplicate resource name '" + name + "', already declared by "
                             + existing->second, YAML::Node{});
                    return;
                }
                owner.emplace(name, path);
            };

            for (const auto& output : asset.Outputs)
                claim(output.Name, "outputs (slot " + std::to_string(output.Slot) + ")");
            for (size_t i = 0; i < asset.Resources.size(); ++i)
                claim(asset.Resources[i].Name, IndexedPath("resources", i));
            for (size_t i = 0; i < asset.Imports.size(); ++i)
                claim(asset.Imports[i].Name, IndexedPath("imports", i));
        }
    }

    GraphAssetParseResult GraphAssetParser::Parse(std::string_view yamlText) const
    {
        GraphAssetParseResult result;

        const std::optional<YAML::Node> document = LoadDocument(yamlText, result.Errors);
        if (!document)
            return result;

        // Held const throughout: yaml-cpp's non-const operator[] may insert the key it looks up.
        const YAML::Node root = *document;
        if (!root.IsMap())
        {
            result.Errors.push_back({ "graph asset must be a YAML map with a 'version' key", "" });
            return result;
        }

        // Nothing else in the document can be interpreted under an unknown version, so a version
        // problem is reported alone.
        const std::optional<std::string> versionText = RequireScalar(root, "version", "graph asset", result.Errors);
        if (!versionText)
            return result;
        const std::optional<uint32_t> version = ParseUnsigned(*versionText);
        if (!version || *version < GRAPH_ASSET_MIN_SCHEMA_VERSION || *version > GRAPH_ASSET_SCHEMA_VERSION)
        {
            AddError(result.Errors, "version",
                     "unsupported graph asset schema version '" + *versionText + "' (this build reads versions "
                     + std::to_string(GRAPH_ASSET_MIN_SCHEMA_VERSION) + " to "
                     + std::to_string(GRAPH_ASSET_SCHEMA_VERSION) + ")",
                     root["version"]);
            return result;
        }

        // Version 1 predates imports, so under it `imports` is an unknown key like any other.
        const bool hasImports = *version >= 2;
        if (hasImports)
            RejectUnknownKeys(root, { "version", "outputs", "resources", "imports", "passes" }, "graph asset",
                              result.Errors);
        else
            RejectUnknownKeys(root, { "version", "outputs", "resources", "passes" }, "graph asset", result.Errors);

        GraphAsset asset;
        asset.Version = *version;
        if (const std::optional<YAML::Node> outputs = OptionalSequence(root, "outputs", result.Errors))
            ParseOutputs(*outputs, asset, result.Errors);
        if (const std::optional<YAML::Node> resources = OptionalSequence(root, "resources", result.Errors))
            ParseResources(*resources, asset, result.Errors);
        if (hasImports)
        {
            if (const std::optional<YAML::Node> imports = OptionalSequence(root, "imports", result.Errors))
                ParseImports(*imports, asset, result.Errors);
        }
        if (const std::optional<YAML::Node> passes = OptionalSequence(root, "passes", result.Errors))
            ParsePasses(*passes, asset, result.Errors);

        RejectDuplicateResourceNames(asset, result.Errors);

        if (!result.Errors.empty())
            return result;

        result.Success = true;
        result.Asset   = std::move(asset);
        return result;
    }
}
