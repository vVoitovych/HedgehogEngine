#include "doctest/doctest/doctest.h"

#include "Tools/GameBinaries.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    // A temp engine root with a Debug folder, removed at the end of the test.
    struct Engine
    {
        std::filesystem::path Root = std::filesystem::temp_directory_path() /
                                     ("game_binaries_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::path Debug   = Root / "Binaries" / "windows-x86_64" / "Debug";
        std::filesystem::path Release = Root / "Binaries" / "windows-x86_64" / "Release";

        Engine()
        {
            std::filesystem::create_directories(Debug);
            std::ofstream(Debug / "Game.exe") << "debug";
        }
        ~Engine()
        {
            std::error_code error;
            std::filesystem::remove_all(Root, error);
        }
    };
}

TEST_CASE("GameBinaries - the Release folder when it holds Game.exe")
{
    Engine engine;
    std::filesystem::create_directories(engine.Release);
    std::ofstream(engine.Release / "Game.exe") << "release";

    const Editor::GameBinariesChoice choice = Editor::ChooseGameBinaries(engine.Root, engine.Debug);
    CHECK(std::filesystem::equivalent(choice.Path, engine.Release));
    CHECK(choice.Warning.empty());
}

TEST_CASE("GameBinaries - else the editor's own folder, with a warning")
{
    Engine engine;
    // No Release folder at all, then one without Game.exe.
    for (int pass = 0; pass < 2; ++pass)
    {
        CAPTURE(pass);
        const Editor::GameBinariesChoice choice = Editor::ChooseGameBinaries(engine.Root, engine.Debug);
        CHECK(choice.Path == engine.Debug);
        CHECK(choice.Warning.find("debug C++ runtime") != std::string::npos);
        std::filesystem::create_directories(engine.Release);
    }

    // The editor itself running from a Release folder without Game.exe.
    const Editor::GameBinariesChoice own = Editor::ChooseGameBinaries(engine.Root, engine.Release);
    CHECK(own.Path == engine.Release);
    CHECK(own.Warning.find("build Release") != std::string::npos);
}
