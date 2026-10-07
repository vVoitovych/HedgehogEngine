#include "CookPlan.hpp"

#include "HedgehogEngine/api/Assets/EngineAssetDependencies.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"

#include "EcsSerialization/api/Assets/AssetDependencyCollector.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"

#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <string_view>

namespace Cooker
{
    namespace
    {
        constexpr std::string_view ENGINE_MOUNT     = "engine://";
        constexpr std::string_view PROJECT_MOUNT    = "project://";
        constexpr std::string_view ASSETS_MOUNT     = "assets://";
        constexpr const char*      ASSETS_FOLDER    = "Assets";
        constexpr int              MANIFEST_VERSION = 1;

        constexpr std::array<std::string_view, 3> SHADER_EXTENSIONS = { ".vert", ".frag", ".comp" };

        struct ManifestEntry
        {
            uintmax_t   Size = 0;
            std::string Hash;
        };

        // The previous cook's files by target path; empty when there is no readable manifest.
        std::map<std::string, ManifestEntry> ReadManifest(const std::filesystem::path& outDir)
        {
            std::map<std::string, ManifestEntry> entries;
            try
            {
                const YAML::Node manifest = YAML::LoadFile((outDir / MANIFEST_FILE_NAME).string());
                if (manifest["version"].as<int>() != MANIFEST_VERSION)
                    return entries;
                for (const YAML::Node& file : manifest["files"])
                    entries[file["path"].as<std::string>()] = { file["size"].as<uintmax_t>(), file["hash"].as<std::string>() };
            }
            catch (const YAML::Exception&)
            {
                entries.clear();
            }
            return entries;
        }

        bool WriteManifest(const std::filesystem::path& outDir, const std::map<std::string, ManifestEntry>& entries)
        {
            YAML::Emitter out;
            out << YAML::BeginMap << YAML::Key << "version" << YAML::Value << MANIFEST_VERSION;
            out << YAML::Key << "files" << YAML::Value << YAML::BeginSeq;
            for (const auto& [path, entry] : entries)
            {
                out << YAML::Flow << YAML::BeginMap << YAML::Key << "path" << YAML::Value << path << YAML::Key << "size"
                    << YAML::Value << entry.Size << YAML::Key << "hash" << YAML::Value << entry.Hash << YAML::EndMap;
            }
            out << YAML::EndSeq << YAML::EndMap;

            std::ofstream file(outDir / MANIFEST_FILE_NAME, std::ios::binary);
            file << out.c_str() << '\n';
            return static_cast<bool>(file);
        }
    }

    std::optional<std::filesystem::path> ToPackagePath(const std::string& virtualPath)
    {
        // A package's root is both the engine root and the project root.
        std::string_view mount;
        for (const std::string_view candidate : { ENGINE_MOUNT, PROJECT_MOUNT, ASSETS_MOUNT })
            if (virtualPath.starts_with(candidate))
                mount = candidate;
        if (mount.empty())
            return std::nullopt;

        // Folded inside its mount, so a ".." cannot reach out of it.
        const std::filesystem::path relative = std::filesystem::path(virtualPath.substr(mount.size())).lexically_normal();
        if (relative.empty() || relative.is_absolute() || relative.has_root_name() || *relative.begin() == "..")
            return std::nullopt;
        return mount == ASSETS_MOUNT ? std::filesystem::path(ASSETS_FOLDER) / relative : relative;
    }

    CookPlan BuildCookPlan(const std::filesystem::path& projectRoot, const std::vector<std::string>& extraScenes,
                           const std::filesystem::path& binariesDir, const std::filesystem::path& engineRoot)
    {
        CookPlan plan;

        // The engine's own files (graphs, shaders) from the engine, the game's from the project;
        // with no engine root given the project is its own engine, as a package or the dev tree is.
        FS::FileSystemManager files;
        auto                  fs = std::make_unique<FS::FileSystem>();
        fs->RegisterPath(std::string(ENGINE_MOUNT), engineRoot.empty() ? projectRoot : engineRoot);
        fs->RegisterPath(std::string(PROJECT_MOUNT), projectRoot);
        fs->RegisterPath(std::string(ASSETS_MOUNT), projectRoot / ASSETS_FOLDER);
        files.Register(std::move(fs));

        HedgehogSettings::ProjectSettings project;
        if (!project.Load(HedgehogSettings::ProjectSettings::PATH, files))
        {
            plan.Errors.push_back((projectRoot / "Project.yaml").string() + " is missing or malformed.");
            return plan;
        }

        std::vector<std::string> scenes = extraScenes;
        if (project.GetStartupScene().empty())
            plan.Errors.push_back("The project '" + project.GetName() + "' has no startup scene.");
        else
            scenes.insert(scenes.begin(), project.GetStartupScene());

        // A file the engine does without is packaged only when the project has it.
        std::vector<std::string> runtimeAssets;
        for (const HedgehogEngine::EngineRuntimeAsset& asset : HedgehogEngine::GetEngineRuntimeAssets())
            if (asset.Required || files.Exists(asset.Path))
                runtimeAssets.push_back(asset.Path);

        EcsSerialization::AssetDependencyCollector collector;
        HedgehogEngine::RegisterEngineAssetDependencies(collector);
        const EcsSerialization::AssetDependencies closure = collector.Collect(scenes, runtimeAssets, files);
        plan.Errors.insert(plan.Errors.end(), closure.Warnings.begin(), closure.Warnings.end());

        for (const std::string& asset : closure.Assets)
        {
            const std::optional<std::filesystem::path> target   = ToPackagePath(asset);
            const std::optional<std::filesystem::path> physical = files.ResolvePhysical(asset);
            if (!target || !physical)
            {
                plan.Errors.push_back(asset + " cannot be packaged: only engine://, project:// and assets:// files can.");
                continue;
            }
            plan.Files.push_back({ asset, *physical, *target });
        }

        // A package lays engine://X and project://X both at X: two different files with one
        // target cannot both ship. One file reached by two names is packaged once.
        std::map<std::string, const CookFile*> byTarget;
        std::vector<CookFile>                  unique;
        for (const CookFile& file : plan.Files)
        {
            const std::string key   = file.Target.lexically_normal().generic_string();
            const auto        found = byTarget.find(key);
            std::error_code   error;
            if (found == byTarget.end())
                byTarget.emplace(key, &file);
            else if (!std::filesystem::equivalent(found->second->Source, file.Source, error))
                plan.Errors.push_back(key + " is both " + found->second->VirtualPath + " and " + file.VirtualPath +
                                      ", which are different files.");
        }
        for (const CookFile& file : plan.Files)
            if (byTarget.at(file.Target.lexically_normal().generic_string()) == &file)
                unique.push_back(file);
        plan.Files = std::move(unique);

        // The enabled plugins' DLLs, at the package root where the engine looks for them.
        for (const HedgehogSettings::PluginEntry& plugin : project.GetPlugins())
        {
            if (!plugin.Enabled)
                continue;
            const std::filesystem::path dll = binariesDir / (plugin.Name + ".dll");
            std::error_code             error;
            if (binariesDir.empty() || !std::filesystem::is_regular_file(dll, error))
            {
                plan.Errors.push_back("The plugin '" + plugin.Name + "' is enabled, but " +
                                      (binariesDir.empty() ? std::string("no binaries folder was given")
                                                           : dll.string() + " does not exist") + ".");
                continue;
            }
            plan.Files.push_back({ PLUGIN_PATH_PREFIX + plugin.Name, dll, dll.filename() });
        }
        return plan;
    }

    std::optional<std::string> HashFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return std::nullopt;

        constexpr uint64_t FNV_OFFSET = 14695981039346656037ull;
        constexpr uint64_t FNV_PRIME  = 1099511628211ull;
        uint64_t           hash       = FNV_OFFSET;
        std::array<char, 64 * 1024> buffer{};
        while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0)
        {
            for (std::streamsize i = 0; i < file.gcount(); ++i)
                hash = (hash ^ static_cast<unsigned char>(buffer[static_cast<size_t>(i)])) * FNV_PRIME;
        }

        char text[17] = {};
        std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(hash));
        return std::string(text);
    }

    CookResult CookPackage(const CookPlan& plan, const std::filesystem::path& outDir)
    {
        CookResult result;
        if (!plan.Errors.empty())
        {
            result.Errors = plan.Errors;
            return result;
        }

        const std::map<std::string, ManifestEntry> previous = ReadManifest(outDir);
        std::map<std::string, ManifestEntry>       written;
        for (const CookFile& file : plan.Files)
        {
            const std::string           key    = file.Target.generic_string();
            const std::filesystem::path target = outDir / file.Target;
            std::error_code             error;
            const uintmax_t             size = std::filesystem::file_size(file.Source, error);
            const std::optional<std::string> hash = HashFile(file.Source);
            if (error || !hash)
            {
                result.Errors.push_back(file.VirtualPath + " cannot be read.");
                continue;
            }

            const auto old = previous.find(key);
            if (old != previous.end() && old->second.Size == size && old->second.Hash == *hash &&
                std::filesystem::is_regular_file(target, error) && std::filesystem::file_size(target, error) == size)
            {
                ++result.Unchanged;
            }
            else
            {
                std::filesystem::create_directories(target.parent_path(), error);
                if (!std::filesystem::copy_file(file.Source, target, std::filesystem::copy_options::overwrite_existing, error))
                {
                    result.Errors.push_back(file.VirtualPath + " cannot be copied to " + target.string() + ": " + error.message());
                    continue;
                }
                ++result.Copied;
            }
            written[key] = { size, *hash };
        }

        // Files an earlier cook packaged that the project no longer references.
        for (const auto& [path, entry] : previous)
        {
            std::error_code error;
            if (!written.contains(path) && std::filesystem::remove(outDir / path, error))
                ++result.Removed;
        }

        if (!WriteManifest(outDir, written))
            result.Errors.push_back((outDir / MANIFEST_FILE_NAME).string() + " cannot be written.");
        return result;
    }

    bool IsShaderStale(const std::filesystem::path& source)
    {
        std::filesystem::path spirv = source;
        spirv += ".spv";
        std::error_code error;
        if (!std::filesystem::exists(spirv, error))
            return true;
        return std::filesystem::last_write_time(spirv, error) < std::filesystem::last_write_time(source, error);
    }

    std::vector<std::string> CompileStaleShaders(const std::filesystem::path& shaderDirectory,
                                                 const std::filesystem::path& glslc, size_t& compiled)
    {
        std::vector<std::string> failed;
        compiled = 0;
        std::error_code error;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(shaderDirectory, error))
        {
            const std::string extension = entry.path().extension().string();
            if (!entry.is_regular_file() ||
                std::find(SHADER_EXTENSIONS.begin(), SHADER_EXTENSIONS.end(), extension) == SHADER_EXTENSIONS.end() ||
                !IsShaderStale(entry.path()))
                continue;

            // cmd.exe strips the outer quotes, keeping the quoted paths inside.
            const std::string command = "\"\"" + glslc.string() + "\" -I \"" + shaderDirectory.string() + "\" \"" +
                                        entry.path().string() + "\" -o \"" + entry.path().string() + ".spv\"\"";
            if (std::system(command.c_str()) != 0)
                failed.push_back(entry.path().string());
            else
                ++compiled;
        }
        return failed;
    }
}
