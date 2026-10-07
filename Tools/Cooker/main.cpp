// Cooker.exe: copies a project's referenced assets into a package folder, or, with --package,
// makes that folder a playable game.
//
//   Cooker.exe --project <folder> --out <folder> [--scene <assets://...yaml>]... [--glslc <glslc.exe>]
//              [--binaries <folder>] [--engine <folder>] [--package]
//
// --package (CookerCore's PackageGame) adds Game.exe and the runtime DLLs from --binaries and the
// engine's licences to the cook, and refuses a folder holding editor, test or debug files.
//
// The project is the folder holding Project.yaml, anywhere on disk; the engine (its render graphs
// and shaders, and the shader sources compiled first) is --engine, by default the engine this
// Cooker belongs to (the folder holding Engine.yaml above it). The package gets the closure of the startup
// scene (and of each --scene), the files the engine always loads, the DLLs of the plugins the
// project enables (from --binaries, by default the Cooker's own folder) and manifest.yaml; a second cook
// copies only what changed and removes what is no longer referenced. Stale engine shaders are
// compiled first. Exits nonzero, naming the file, when a reference is missing or a copy fails.

#include "CookerCore/CookPlan.hpp"
#include "CookerCore/Package.hpp"

#include "FileSystem/api/PathUtils.hpp"

#include "Logger/api/Logger.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace
{
    struct CookArguments
    {
        std::optional<std::filesystem::path> Project;
        std::optional<std::filesystem::path> Out;
        std::optional<std::filesystem::path> Glslc;
        std::optional<std::filesystem::path> Binaries;
        std::optional<std::filesystem::path> Engine;
        std::vector<std::string>             Scenes;
        bool                                 Package = false;
        bool                                 Valid   = true;
    };

    CookArguments ParseArguments(int argc, char* argv[])
    {
        CookArguments arguments;
        for (int i = 1; i < argc; ++i)
        {
            const bool hasValue = i + 1 < argc;
            if (std::strcmp(argv[i], "--project") == 0 && hasValue)
                arguments.Project = argv[++i];
            else if (std::strcmp(argv[i], "--out") == 0 && hasValue)
                arguments.Out = argv[++i];
            else if (std::strcmp(argv[i], "--scene") == 0 && hasValue)
                arguments.Scenes.emplace_back(argv[++i]);
            else if (std::strcmp(argv[i], "--glslc") == 0 && hasValue)
                arguments.Glslc = argv[++i];
            else if (std::strcmp(argv[i], "--binaries") == 0 && hasValue)
                arguments.Binaries = argv[++i];
            else if (std::strcmp(argv[i], "--engine") == 0 && hasValue)
                arguments.Engine = argv[++i];
            else if (std::strcmp(argv[i], "--package") == 0)
                arguments.Package = true;
            else
            {
                LOGERROR("Cooker: unknown argument '", argv[i], "'.");
                arguments.Valid = false;
            }
        }
        if (arguments.Valid && (!arguments.Project || !arguments.Out))
        {
            LOGERROR("Cooker: usage: Cooker.exe --project <folder> --out <folder> [--scene <assets://...>]... [--glslc <exe>] [--binaries <folder>] [--engine <folder>] [--package]");
            arguments.Valid = false;
        }
        return arguments;
    }

    // Logs a result's warnings and errors; true when it has no error.
    bool Report(const std::vector<std::string>& warnings, const std::vector<std::string>& errors)
    {
        for (const std::string& warning : warnings)
            LOGWARNING("Cooker: ", warning);
        for (const std::string& error : errors)
            LOGERROR("Cooker: ", error);
        if (!errors.empty())
            LOGERROR("Cooker: the cook failed; ", errors.size(), " error(s).");
        return errors.empty();
    }

    int Package(const Cooker::PackageDesc& desc)
    {
        const Cooker::PackageResult result = Cooker::PackageGame(desc);
        if (result.ShadersCompiled > 0)
            LOGINFO("Cooker: compiled ", result.ShadersCompiled, " stale shader(s).");
        if (!Report(result.Warnings, result.Errors))
            return EXIT_FAILURE;
        LOGINFO("Cooker: packaged ", result.Files, " file(s) in ", desc.OutDir.string(), ": ", result.Copied, " copied, ",
                result.Unchanged, " unchanged, ", result.Removed, " removed.");
        return EXIT_SUCCESS;
    }
}

int main(int argc, char* argv[])
{
    const CookArguments arguments = ParseArguments(argc, argv);
    if (!arguments.Valid)
        return EXIT_FAILURE;

    const std::filesystem::path project = std::filesystem::absolute(*arguments.Project);
    const std::filesystem::path out     = std::filesystem::absolute(*arguments.Out);
    // The engine the game is built with: the one this Cooker belongs to unless --engine names one.
    const std::filesystem::path engine =
        arguments.Engine ? std::filesystem::absolute(*arguments.Engine) : FS::GetEngineRootDirectory();
    // Plugin DLLs (and, with --package, Game.exe and the runtime DLLs) come from the folder the
    // engine was built into, where the Cooker is too.
    const std::filesystem::path binaries =
        arguments.Binaries ? std::filesystem::absolute(*arguments.Binaries) : std::filesystem::absolute(argv[0]).parent_path();
    if (arguments.Package)
        return Package({ project, engine, out, binaries, arguments.Scenes, arguments.Glslc.value_or(std::filesystem::path()) });

    const Cooker::ShaderCompileResult shaders = Cooker::CompileEngineShaders(engine, arguments.Glslc.value_or(std::filesystem::path()));
    if (shaders.Compiled > 0)
        LOGINFO("Cooker: compiled ", shaders.Compiled, " stale shader(s).");
    if (!Report(shaders.Warnings, shaders.Errors))
        return EXIT_FAILURE;

    const Cooker::CookPlan   plan   = Cooker::BuildCookPlan(project, arguments.Scenes, binaries, engine);
    const Cooker::CookResult result = Cooker::CookPackage(plan, out);
    if (!Report({}, result.Errors))
        return EXIT_FAILURE;

    LOGINFO("Cooker: ", plan.Files.size(), " file(s) in ", out.string(), ": ", result.Copied, " copied, ", result.Unchanged,
            " unchanged, ", result.Removed, " removed.");
    return EXIT_SUCCESS;
}
