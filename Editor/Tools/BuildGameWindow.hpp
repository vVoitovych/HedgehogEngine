#pragma once

#include "CookerCore/Package.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace Editor
{
    // File > Build Game...: packages the open project with Cooker::PackageGame, the implementation
    // PackageGame.bat and Cooker.exe --package share. The output folder (Browse...; by default
    // <engine root>/Build/<project name>), the binaries folder ChooseGameBinaries picks and its
    // warning, Build, then the result: its counts, warnings and errors (also in the Console), Show
    // in Explorer and Run (Game.exe, detached, from the output folder). The build runs on the main
    // thread: the editor waits for it. Allowed in Edit and Play: it reads only files on disk.
    class BuildGameWindow
    {
    public:
        bool Open = false;

        // Shows the window for the project at projectRoot, named projectName.
        void Show(const std::filesystem::path& projectRoot, const std::string& projectName);

        void Draw();

    private:
        void Build();

        std::filesystem::path                m_ProjectRoot;
        std::string                          m_Output;
        std::filesystem::path                m_Binaries;
        std::string                          m_BinariesWarning;
        std::optional<Cooker::PackageResult> m_Result;     // the last build's
        std::filesystem::path                m_BuiltFolder; // where it went
    };
}
