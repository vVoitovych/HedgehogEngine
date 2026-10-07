#include "Tools/GameBinaries.hpp"

#include <system_error>

namespace Editor
{
    namespace
    {
        constexpr const char* GAME_EXECUTABLE = "Game.exe";
    }

    GameBinariesChoice ChooseGameBinaries(const std::filesystem::path& engineRoot, const std::filesystem::path& ownDir)
    {
        const std::filesystem::path release = (engineRoot / RELEASE_BINARIES_DIRECTORY).lexically_normal();
        std::error_code             error;
        if (std::filesystem::is_regular_file(release / GAME_EXECUTABLE, error))
            return { release, {} };
        if (std::filesystem::equivalent(release, ownDir, error))
            return { ownDir, "The Release build has no Game.exe yet; build Release (Scripts\\Build.bat Release) first." };
        return { ownDir, "No Release Game.exe in " + release.string() + "; packaging the editor's own binaries, " +
                             "which need the debug C++ runtime wherever the game runs if they are a Debug build." };
    }
}
