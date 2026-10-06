#include "Application.hpp"
#include "GameMode.hpp"

#include "Project/StartupProject.hpp"

#include "HedgehogRenderer/Renderer.hpp"
#include "FileSystem/api/PathUtils.hpp"
#include "Logger/api/Logger.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
    inline constexpr uint32_t DEFAULT_SMOKE_TEST_FRAMES  = 120;
    inline constexpr uint32_t DEFAULT_BENCHMARK_FRAMES   = 600;
    inline constexpr uint32_t DEFAULT_GAME_MODE_FRAMES   = 120;
    inline constexpr uint32_t BENCHMARK_WARMUP_FRAMES    = 120;
    inline constexpr const char* DEFAULT_BENCHMARK_SCENE = "benchmark.yaml";
    // None: --game-mode plays the project's startup scene, else Default.yaml.
    inline constexpr const char* DEFAULT_GAME_MODE_SCENE = "";

    // Returns the frame count if the flag was passed (with an optional numeric
    // frame-count argument), or 0 when the flag is absent.
    uint32_t ParseFrameCountFlag(int argc, char* argv[], const char* flag, uint32_t defaultFrames)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (std::strcmp(argv[i], flag) != 0)
                continue;

            if (i + 1 < argc)
            {
                char* end = nullptr;
                const unsigned long frames = std::strtoul(argv[i + 1], &end, 10);
                if (end != argv[i + 1] && *end == '\0' && frames > 0)
                    return static_cast<uint32_t>(frames);
            }
            return defaultFrames;
        }
        return 0;
    }

    // The scene file after flag: the first later argument that ends in ".yaml" and is not the
    // value of --project ("--game-mode 60 --project Game/Project.yaml Hud.yaml"), or defaultScene.
    std::string ParseSceneArgument(int argc, char* argv[], const char* flag, const char* defaultScene)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (std::strcmp(argv[i], flag) != 0)
                continue;
            for (int j = i + 1; j < argc; ++j)
            {
                const std::string argument = argv[j];
                if (std::strcmp(argv[j - 1], "--project") == 0)
                    continue;
                if (argument.size() > 5 && argument.ends_with(".yaml"))
                    return argument;
            }
        }
        return defaultScene;
    }

    // The value after flag ("--project <folder>"), or empty when the flag is absent or last.
    std::string ParseValueFlag(int argc, char* argv[], const char* flag)
    {
        for (int i = 1; i + 1 < argc; ++i)
            if (std::strcmp(argv[i], flag) == 0)
                return argv[i + 1];
        return {};
    }

    // Round trips --smoke-test makes with --switch-project: each rebuilds the editor on the other
    // project and back, as File > Open Project... does.
    inline constexpr int SMOKE_TEST_SWITCH_ROUND_TRIPS = 3;

    int RunSmokeTest(uint32_t frames, const std::string& switchProject)
    {
        LOGINFO("Smoke test: rendering ", frames, " frame(s)...");
        if (!Renderer::AreValidationLayersEnabled())
            LOGWARNING("Smoke test: Vulkan validation layers are disabled in this build; "
                       "only a crash-free run is being verified. Use a Debug build for full coverage.");

        // The editor on its project, then, with --switch-project, rebuilt in this process on the
        // other project and back, round trip after round trip, exactly as a project switch does.
        std::vector<std::filesystem::path> projects = { FS::GetProjectRootDirectory() };
        if (!switchProject.empty())
        {
            const std::filesystem::path other = Editor::NormalizeProjectPath(switchProject);
            if (!Editor::IsProjectFolder(other))
            {
                LOGERROR("Smoke test: --switch-project ", switchProject, " holds no Project.yaml.");
                return EXIT_FAILURE;
            }
            for (int trip = 0; trip < SMOKE_TEST_SWITCH_ROUND_TRIPS; ++trip)
                projects.insert(projects.end(), { other, projects.front() });
        }
        for (const std::filesystem::path& project : projects)
        {
            // Scoped so teardown validation errors (e.g. leaked Vulkan objects)
            // are counted before the final verdict.
            FS::SetProjectRootDirectory(project);
            Editor::EditorApplication app{};
            (void)app.Run(frames);
        }

        const uint32_t errors   = Renderer::GetValidationErrorCount();
        const uint32_t warnings = Renderer::GetValidationWarningCount();
        if (errors > 0)
        {
            LOGERROR("Smoke test FAILED: ", errors, " Vulkan validation error(s), ",
                     warnings, " warning(s). See the log above for details.");
            return EXIT_FAILURE;
        }

        LOGINFO("Smoke test PASSED: 0 validation errors, ", warnings, " warning(s).");
        return EXIT_SUCCESS;
    }

    int RunBenchmark(uint32_t frames, const std::string& sceneFile)
    {
        {
            Editor::EditorApplication app{};
            app.RunBenchmark(BENCHMARK_WARMUP_FRAMES, frames, sceneFile);
        }

        const uint32_t errors = Renderer::GetValidationErrorCount();
        if (errors > 0)
        {
            LOGERROR("Benchmark run had ", errors, " Vulkan validation error(s); "
                     "results are not trustworthy.");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
}

int main(int argc, char* argv[])
{
    // The project: --project <folder or Project.yaml> in every mode; the interactive editor alone
    // falls back on the recent list, so automated runs stay reproducible.
    const std::string projectArgument = ParseValueFlag(argc, argv, "--project");

    const uint32_t smokeTestFrames = ParseFrameCountFlag(
        argc, argv, "--smoke-test", DEFAULT_SMOKE_TEST_FRAMES);
    const uint32_t gameModeFrames = ParseFrameCountFlag(
        argc, argv, "--game-mode", DEFAULT_GAME_MODE_FRAMES);
    const uint32_t benchmarkFrames = ParseFrameCountFlag(
        argc, argv, "--benchmark", DEFAULT_BENCHMARK_FRAMES);
    const bool interactive = smokeTestFrames == 0 && gameModeFrames == 0 && benchmarkFrames == 0;
    if (!Editor::SelectStartupProject(projectArgument, interactive))
        return EXIT_FAILURE;

    if (smokeTestFrames > 0)
        return RunSmokeTest(smokeTestFrames, ParseValueFlag(argc, argv, "--switch-project"));

    if (gameModeFrames > 0)
        return Editor::RunGameMode(gameModeFrames,
                                   ParseSceneArgument(argc, argv, "--game-mode", DEFAULT_GAME_MODE_SCENE));

    if (benchmarkFrames > 0)
        return RunBenchmark(benchmarkFrames,
                            ParseSceneArgument(argc, argv, "--benchmark", DEFAULT_BENCHMARK_SCENE));

    // Only the interactive editor opens maximized, and only it records its project as the most
    // recent; the automated runs above keep the fixed-size window their results are defined at
    // (PERFORMANCE.md). A project chosen from the File menu ends the run: the editor is torn down
    // as on exit and built again on it, in this process (a relaunch would detach a debugger).
    for (;;)
    {
        Editor::RunResult result;
        {
            Editor::EditorApplication app{ HedgehogEngine::WindowMode::Maximized, true };
            result = app.Run();
        }
        if (!result.RequestedProject)
            return EXIT_SUCCESS;
        FS::SetProjectRootDirectory(*result.RequestedProject);
        LOGINFO("[Editor] Reopening on the project at ", result.RequestedProject->string(), ".");
    }
}
