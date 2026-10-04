// Cooker.exe: copies a project's referenced assets into a package folder.
//
//   Cooker.exe --project <folder> --out <folder> [--scene <assets://...yaml>]... [--glslc <glslc.exe>]
//
// The project is the folder holding Project.yaml. The package gets the closure of the startup
// scene (and of each --scene), the files the engine always loads, and manifest.yaml; a second cook
// copies only what changed and removes what is no longer referenced. Stale engine shaders are
// compiled first. Exits nonzero, naming the file, when a reference is missing or a copy fails.

#include "CookPlan.hpp"

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
        std::vector<std::string>             Scenes;
        bool                                 Valid = true;
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
            else
            {
                LOGERROR("Cooker: unknown argument '", argv[i], "'.");
                arguments.Valid = false;
            }
        }
        if (arguments.Valid && (!arguments.Project || !arguments.Out))
        {
            LOGERROR("Cooker: usage: Cooker.exe --project <folder> --out <folder> [--scene <assets://...>]... [--glslc <exe>]");
            arguments.Valid = false;
        }
        return arguments;
    }

    // The engine's shaders, compiled where the project carries their sources (a dev tree).
    bool CompileShaders(const std::filesystem::path& project, const std::optional<std::filesystem::path>& glslcArgument)
    {
        const std::filesystem::path shaders = project / "HedgehogEngine" / "HedgehogRenderer" / "assets" / "Shaders";
        if (!std::filesystem::is_directory(shaders))
            return true;
        const std::filesystem::path glslc = glslcArgument.value_or(project / "ThirdParty" / "glslc" / "glslc.exe");
        if (!std::filesystem::is_regular_file(glslc))
        {
            LOGWARNING("Cooker: no glslc at ", glslc.string(), "; shaders are packaged as they were last compiled.");
            return true;
        }
        size_t                         compiled = 0;
        const std::vector<std::string> failed   = Cooker::CompileStaleShaders(shaders, glslc, compiled);
        for (const std::string& source : failed)
            LOGERROR("Cooker: ", source, " does not compile.");
        if (compiled > 0)
            LOGINFO("Cooker: compiled ", compiled, " stale shader(s).");
        return failed.empty();
    }
}

int main(int argc, char* argv[])
{
    const CookArguments arguments = ParseArguments(argc, argv);
    if (!arguments.Valid)
        return EXIT_FAILURE;

    const std::filesystem::path project = std::filesystem::absolute(*arguments.Project);
    const std::filesystem::path out     = std::filesystem::absolute(*arguments.Out);
    if (!CompileShaders(project, arguments.Glslc))
        return EXIT_FAILURE;

    const Cooker::CookPlan   plan   = Cooker::BuildCookPlan(project, arguments.Scenes);
    const Cooker::CookResult result = Cooker::CookPackage(plan, out);
    for (const std::string& error : result.Errors)
        LOGERROR("Cooker: ", error);
    if (!result.Errors.empty())
    {
        LOGERROR("Cooker: the cook failed; ", result.Errors.size(), " error(s).");
        return EXIT_FAILURE;
    }

    LOGINFO("Cooker: ", plan.Files.size(), " file(s) in ", out.string(), ": ", result.Copied, " copied, ", result.Unchanged,
            " unchanged, ", result.Removed, " removed.");
    return EXIT_SUCCESS;
}
