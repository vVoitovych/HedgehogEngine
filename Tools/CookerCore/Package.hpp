#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

// Packaging a game (epic HE-290): everything that makes a project a playable folder but building
// the engine and running the result. Cooker.exe --package, PackageGame.bat (through it) and the
// Editor's Build Game share it. It logs nothing: problems come back as text.
namespace Cooker
{
    // What Game.exe loads from beside itself, copied from the binaries folder into the package
    // root. A new DLL the game links must be added here.
    inline constexpr std::array<const char*, 13> GAME_RUNTIME_FILES = {
        "Game.exe",         "glfw.dll",         "Logger.dll",          "FileSystem.dll",   "HedgehogMath.dll",
        "HedgehogCommon.dll", "ECS.dll",        "EcsSerialization.dll", "ContentLoader.dll", "HedgehogSettings.dll",
        "HedgehogWindow.dll", "HedgehogAudio.dll", "HedgehogEngine.dll",
    };

    // The engine's licence and the third-party notices, from the engine root: every game ships them.
    inline constexpr std::array<const char*, 2> LICENCE_FILES = { "LICENSE.txt", "THIRD_PARTY_NOTICES.md" };

    // The prefixes of the runtime files' and licences' CookFile::VirtualPath.
    constexpr const char* RUNTIME_PATH_PREFIX = "runtime:";
    constexpr const char* LICENCE_PATH_PREFIX = "licence:";

    struct ShaderCompileResult
    {
        size_t                   Compiled = 0;
        std::vector<std::string> Errors;   // a source that does not compile
        std::vector<std::string> Warnings; // no glslc: shaders packaged as last compiled
    };

    // Compiles every stale engine shader (under <engineRoot>/HedgehogEngine/HedgehogRenderer/
    // assets/Shaders) with glslc, by default <engineRoot>/ThirdParty/glslc/glslc.exe. An engine
    // without the sources (a package) compiles nothing; one without glslc gives a warning.
    [[nodiscard]] ShaderCompileResult CompileEngineShaders(const std::filesystem::path& engineRoot,
                                                           const std::filesystem::path& glslc = {});

    struct PackageDesc
    {
        std::filesystem::path    ProjectRoot; // the folder holding Project.yaml
        std::filesystem::path    EngineRoot;  // the engine the game is built with
        std::filesystem::path    OutDir;      // the package folder
        std::filesystem::path    BinariesDir; // Game.exe, the runtime DLLs and the plugins' DLLs
        std::vector<std::string> ExtraScenes; // virtual paths cooked beside the startup scene
        std::filesystem::path    Glslc;       // empty: the engine's own
    };

    struct PackageResult
    {
        size_t                   Files           = 0; // in the package, the manifest left out
        size_t                   Copied          = 0;
        size_t                   Unchanged       = 0;
        size_t                   Removed         = 0;
        size_t                   ShadersCompiled = 0;
        std::vector<std::string> Errors; // empty when the package is complete
        std::vector<std::string> Warnings;
    };

    // Makes OutDir a playable game: compiles stale engine shaders, builds the cook plan with
    // GAME_RUNTIME_FILES from BinariesDir and LICENCE_FILES from EngineRoot added (a missing one is
    // an error naming it), cooks it incrementally (the manifest covers every file, so a second
    // package copies only what changed and removes what is gone, a disabled plugin's DLL
    // included), then refuses a package holding anything FindForbiddenPackageFiles finds. Any
    // error before the cook leaves OutDir as it was.
    [[nodiscard]] PackageResult PackageGame(const PackageDesc& desc);

    // Files under outDir no game may ship: the editor, the Cooker, a test executable, debug files
    // (.pdb, .ilk), static libraries, DialogueWindows.dll, anything named imgui, and GLSL sources.
    [[nodiscard]] std::vector<std::filesystem::path> FindForbiddenPackageFiles(const std::filesystem::path& outDir);
}
