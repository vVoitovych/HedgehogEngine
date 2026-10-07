#pragma once

#include <filesystem>
#include <string>

namespace Editor
{
    // The engine's Release binaries, relative to the engine root: what a game should ship.
    inline constexpr const char* RELEASE_BINARIES_DIRECTORY = "Binaries/windows-x86_64/Release";

    struct GameBinariesChoice
    {
        std::filesystem::path Path;
        std::string           Warning; // empty for the Release folder
    };

    // Where File > Build Game... takes Game.exe and the runtime DLLs from: the engine's Release
    // folder when it holds Game.exe, else ownDir (the editor's own folder) with a warning, since a
    // Debug package needs the debug CRT wherever it runs. ImGui-free, so EditorTest checks it.
    [[nodiscard]] GameBinariesChoice ChooseGameBinaries(const std::filesystem::path& engineRoot,
                                                        const std::filesystem::path& ownDir);
}
