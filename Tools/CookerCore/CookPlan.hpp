#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// The cook (epic HE-173): a project's referenced assets copied into a package laid out as the
// engine mounts it from its root (EngineContext::InitFileSystem), so Game.exe placed beside them
// finds the project with no other files.
namespace Cooker
{
    // Where a virtual path goes in a package: engine://X and project://X at X (a package's root is
    // both roots), assets://X at Assets/X; nullopt for any other path (another mount, an absolute
    // file), which cannot be packaged.
    [[nodiscard]] std::optional<std::filesystem::path> ToPackagePath(const std::string& virtualPath);

    struct CookFile
    {
        std::string           VirtualPath;
        std::filesystem::path Source; // in the project
        std::filesystem::path Target; // relative to the package root
    };

    // What a cook copies, by target path, or why it cannot run.
    struct CookPlan
    {
        std::vector<CookFile>    Files;
        std::vector<std::string> Errors;
    };

    // The project at projectRoot (the folder holding Project.yaml): the closure of its startup
    // scene and of extraScenes (virtual paths), plus every file the engine loads whatever the
    // scene (GetEngineRuntimeAssets) and what those reference. A reference that does not exist or
    // cannot be read, a project without a startup scene and a path that cannot be packaged are
    // errors naming the file.
    //
    // Each plugin Project.yaml enables is packaged too: <binariesDir>/<name>.dll at the package
    // root, beside Game.exe and HedgehogEngine.dll, where the engine looks for plugins (virtual
    // path "plugin:<name>"). An enabled plugin whose DLL is not there is an error naming it, so a
    // package never ships without one; a disabled plugin is not packaged.
    //
    // engine:// files (the render graphs and their shaders) come from engineRoot, the engine the
    // game is built with; empty means the project root, as in the dev tree today. engine://X and
    // project://X both land at X, so two different files with one target are an error naming both.
    [[nodiscard]] CookPlan BuildCookPlan(const std::filesystem::path& projectRoot, const std::vector<std::string>& extraScenes = {},
                                         const std::filesystem::path& binariesDir = {},
                                         const std::filesystem::path& engineRoot  = {});

    // Every scene directly in the project's Assets/Scenes folder, as assets://Scenes/<file>.yaml,
    // sorted: what a package of all its scenes cooks beside the startup scene.
    [[nodiscard]] std::vector<std::string> ListProjectScenes(const std::filesystem::path& projectRoot);

    // The prefix of a plugin DLL's CookFile::VirtualPath.
    constexpr const char* PLUGIN_PATH_PREFIX = "plugin:";

    struct CookResult
    {
        size_t                   Copied    = 0;
        size_t                   Unchanged = 0;
        size_t                   Removed   = 0;
        std::vector<std::string> Errors;
    };

    // The manifest a cook writes into the package root: each file's target, size and hash.
    constexpr const char* MANIFEST_FILE_NAME = "manifest.yaml";

    // Copies the plan into outDir and writes the manifest. A file whose source has the size and
    // hash the previous manifest records, and whose copy is still there, is left alone; a file the
    // previous manifest lists and the plan no longer has is removed, so the package holds the
    // closure and nothing else.
    [[nodiscard]] CookResult CookPackage(const CookPlan& plan, const std::filesystem::path& outDir);

    // FNV-1a 64 of a file's bytes, as 16 hex digits; nullopt when it cannot be read.
    [[nodiscard]] std::optional<std::string> HashFile(const std::filesystem::path& path);

    // A GLSL source (.vert, .frag, .comp) whose .spv beside it is missing or older than it.
    [[nodiscard]] bool IsShaderStale(const std::filesystem::path& source);

    // Compiles every stale GLSL source under shaderDirectory with glslc, as the Shaders project's
    // pre-build step does (include root: shaderDirectory). Returns the sources that failed.
    [[nodiscard]] std::vector<std::string> CompileStaleShaders(const std::filesystem::path& shaderDirectory,
                                                               const std::filesystem::path& glslc, size_t& compiled);
}
