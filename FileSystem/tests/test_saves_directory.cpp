#include "doctest/doctest/doctest.h"

#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"

#include "test_helpers.hpp"

#include <filesystem>
#include <optional>

TEST_CASE("Saves directory - per project under LocalAppData, with an Editor subfolder")
{
    const std::filesystem::path base = "C:/Users/someone/AppData/Local";

    const std::optional<std::filesystem::path> game = FS::MakeSavesDirectory(base, "MyGame", false);
    REQUIRE(game.has_value());
    CHECK(*game == base / "HedgehogEngine" / "MyGame" / "Saves");

    const std::optional<std::filesystem::path> editor = FS::MakeSavesDirectory(base, "MyGame", true);
    REQUIRE(editor.has_value());
    CHECK(*editor == base / "HedgehogEngine" / "MyGame" / "Saves" / "Editor");

    for (const char* bad : { "", ".", "..", "a/b", "a\b", "C:", "what?", "pipe|d" })
    {
        CAPTURE(bad);
        CHECK_FALSE(FS::MakeSavesDirectory(base, bad, false).has_value());
    }

    const std::optional<std::filesystem::path> real = FS::GetSavesDirectory("HedgehogEngine", true);
    REQUIRE(real.has_value()); // LOCALAPPDATA is always set on Windows
    CHECK(real->filename() == "Editor");
}

TEST_CASE("Saves directory - MountSaves creates the folder and mounts it as saves://")
{
    TempDir                     tmp;
    const std::filesystem::path saves = tmp.Path() / "HedgehogEngine" / "Test" / "Saves";
    FS::FileSystemManager       manager;

    REQUIRE(FS::MountSaves(manager, saves));
    CHECK(std::filesystem::is_directory(saves));
    REQUIRE(manager.WriteTextFile("saves://probe.txt", "hello"));
    CHECK(std::filesystem::is_regular_file(saves / "probe.txt"));

    CHECK_FALSE(FS::MountSaves(manager, saves)); // saves:// is taken
}
