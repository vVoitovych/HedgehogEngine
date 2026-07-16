#include "Application.hpp"

#include "HedgehogRenderer/Renderer.hpp"
#include "Logger/api/Logger.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
    inline constexpr uint32_t DEFAULT_SMOKE_TEST_FRAMES = 120;

    // Returns the frame count if --smoke-test was passed (with an optional
    // numeric frame-count argument), or 0 for a normal editor run.
    uint32_t ParseSmokeTestFrames(int argc, char* argv[])
    {
        for (int i = 1; i < argc; ++i)
        {
            if (std::strcmp(argv[i], "--smoke-test") != 0)
                continue;

            if (i + 1 < argc)
            {
                char* end = nullptr;
                const unsigned long frames = std::strtoul(argv[i + 1], &end, 10);
                if (end != argv[i + 1] && *end == '\0' && frames > 0)
                    return static_cast<uint32_t>(frames);
            }
            return DEFAULT_SMOKE_TEST_FRAMES;
        }
        return 0;
    }

    int RunSmokeTest(uint32_t frames)
    {
        LOGINFO("Smoke test: rendering ", frames, " frame(s)...");
        if (!Renderer::AreValidationLayersEnabled())
            LOGWARNING("Smoke test: Vulkan validation layers are disabled in this build; "
                       "only a crash-free run is being verified. Use a Debug build for full coverage.");

        {
            // Scoped so teardown validation errors (e.g. leaked Vulkan objects)
            // are counted before the final verdict.
            Editor::EditorApplication app{};
            app.Run(frames);
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
}

int main(int argc, char* argv[])
{
    const uint32_t smokeTestFrames = ParseSmokeTestFrames(argc, argv);
    if (smokeTestFrames > 0)
        return RunSmokeTest(smokeTestFrames);

    Editor::EditorApplication app{};
    app.Run();
    return EXIT_SUCCESS;
}
