// Game.exe: runs a project's startup scene in Play mode, with no editor and no ImGui.
//
//   Game.exe [--frames N] [--project <folder or Project.yaml>]
//
// The project is the nearest Project.yaml above the executable (the repository in a dev tree,
// the game's own folder once packaged), or the one --project names. Its window opens as the
// project describes, and its startup scene plays until the window closes or N frames have run.
// Exits nonzero when the game cannot start or, in a build with Vulkan validation, on any
// validation error.

#include "HedgehogRuntime/api/GameRuntime.hpp"
#include "HedgehogRenderer/Renderer.hpp"

#include "FileSystem/api/PathUtils.hpp"

#include "Logger/api/Logger.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>

namespace
{
    struct GameArguments
    {
        uint32_t                             Frames = 0; // 0: until the window closes
        std::optional<std::filesystem::path> Project;
        bool                                 Valid = true;
    };

    GameArguments ParseArguments(int argc, char* argv[])
    {
        GameArguments arguments;
        for (int i = 1; i < argc; ++i)
        {
            const bool hasValue = i + 1 < argc;
            if (std::strcmp(argv[i], "--frames") == 0 && hasValue)
            {
                char*               end    = nullptr;
                const unsigned long frames = std::strtoul(argv[i + 1], &end, 10);
                if (end == argv[i + 1] || *end != '\0' || frames == 0)
                {
                    LOGERROR("Game: --frames takes a positive number, not '", argv[i + 1], "'.");
                    arguments.Valid = false;
                }
                arguments.Frames = static_cast<uint32_t>(frames);
                ++i;
            }
            else if (std::strcmp(argv[i], "--project") == 0 && hasValue)
            {
                arguments.Project = std::filesystem::path(argv[i + 1]);
                ++i;
            }
            else
            {
                LOGERROR("Game: unknown argument '", argv[i], "'. Usage: Game.exe [--frames N] [--project <folder>]");
                arguments.Valid = false;
            }
        }
        return arguments;
    }

    // The folder holding the project: --project names it or its Project.yaml.
    std::optional<std::filesystem::path> ProjectRoot(const std::filesystem::path& project)
    {
        std::error_code             error;
        const std::filesystem::path absolute = std::filesystem::absolute(project, error);
        const std::filesystem::path root =
            absolute.filename() == FS::PROJECT_FILE_NAME ? absolute.parent_path() : absolute;
        if (!std::filesystem::is_regular_file(root / FS::PROJECT_FILE_NAME, error))
            return std::nullopt;
        return root;
    }
}

int main(int argc, char* argv[])
{
    const GameArguments arguments = ParseArguments(argc, argv);
    if (!arguments.Valid)
        return EXIT_FAILURE;

    if (arguments.Project)
    {
        const std::optional<std::filesystem::path> root = ProjectRoot(*arguments.Project);
        if (!root)
        {
            LOGERROR("Game: no ", FS::PROJECT_FILE_NAME, " at ", arguments.Project->string(), ".");
            return EXIT_FAILURE;
        }
        FS::SetEngineRootDirectory(*root);
    }

    bool started = false;
    {
        Runtime::RuntimeDesc desc;
        desc.MaxFrames        = arguments.Frames;
        desc.UseProjectWindow = true;

        Runtime::GameRuntime runtime;
        started = runtime.Init(desc);
        runtime.Run();
        runtime.Shutdown();
    }

    // Counted after teardown, so errors such as leaked Vulkan objects are included.
    const uint32_t errors = Renderer::GetValidationErrorCount();
    if (!started)
    {
        LOGERROR("Game: the game did not start. See the log above for details.");
        return EXIT_FAILURE;
    }
    if (errors > 0)
    {
        LOGERROR("Game: ", errors, " Vulkan validation error(s). See the log above for details.");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
