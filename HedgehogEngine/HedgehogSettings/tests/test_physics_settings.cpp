#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/PhysicsSettings.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>

using HedgehogSettings::PhysicsSettings;
using HedgehogSettings::Settings;

namespace
{
    constexpr const char* PATH = "project://engine_settings.yaml";

    // project:// on a folder, a fresh temp one by default.
    struct SettingsFiles
    {
        TempDir               Dir;
        FS::FileSystemManager Files;

        SettingsFiles() : SettingsFiles(std::filesystem::path()) {}

        explicit SettingsFiles(const std::filesystem::path& root)
        {
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("project://", root.empty() ? Dir.Path() : root);
            Files.Register(std::move(fs));
        }

        void Write(const std::string& text) { REQUIRE(Files.WriteTextFile(PATH, text)); }

        std::string Read() const
        {
            std::string text = Files.ReadTextFile(PATH).value_or("");
            text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
            return text;
        }
    };

    // Every row lists every layer but those a test leaves out.
    std::string CollisionsSection(uint32_t skipRow = 99, uint32_t skipColumn = 99)
    {
        std::string text = "physics:\n  collisions:\n";
        for (uint32_t row = 0; row < PhysicsSettings::LAYER_COUNT; ++row)
        {
            text += "    " + std::to_string(row) + ": [";
            for (uint32_t column = 0; column < PhysicsSettings::LAYER_COUNT; ++column)
                if (row != skipRow || column != skipColumn)
                    text += std::to_string(column) + ", ";
            text += "]\n";
        }
        return text;
    }

    std::filesystem::path RepositoryRoot()
    {
        // tests/ -> HedgehogSettings/ -> HedgehogEngine/ -> the repository.
        return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
    }
}

TEST_CASE("Physics settings - the defaults: Z-up gravity, layer 0 Default, every layer colliding")
{
    const PhysicsSettings physics;
    CHECK(physics.Gravity == std::array<float, 3>{ 0.0f, 0.0f, -9.81f });
    CHECK(physics.LayerNames[0] == "Default");
    for (uint32_t layer = 1; layer < PhysicsSettings::LAYER_COUNT; ++layer)
        CHECK(physics.LayerNames[layer].empty());
    for (uint32_t a = 0; a < PhysicsSettings::LAYER_COUNT; ++a)
        for (uint32_t b = 0; b < PhysicsSettings::LAYER_COUNT; ++b)
            CHECK(physics.Collides(a, b));
    CHECK(physics.WorkerThreads == PhysicsSettings::AUTO_WORKER_THREADS);
    CHECK(physics.GetLayerDisplayName(0) == "Default");
    CHECK(physics.GetLayerDisplayName(3) == "Layer 3");
}

TEST_CASE("Physics settings - SetCollides writes both directions and ignores layers out of range")
{
    PhysicsSettings physics;
    physics.SetCollides(2, 5, false);
    CHECK_FALSE(physics.Collides(2, 5));
    CHECK_FALSE(physics.Collides(5, 2));
    CHECK(physics.Collides(2, 4));
    physics.SetCollides(3, 3, false);
    CHECK_FALSE(physics.Collides(3, 3));
    physics.SetCollides(5, 2, true);
    CHECK(physics.Collides(2, 5));

    const PhysicsSettings before = physics;
    physics.SetCollides(16, 0, false);
    physics.SetCollides(0, 99, false);
    CHECK(physics == before);
    CHECK_FALSE(physics.Collides(16, 0));
}

TEST_CASE("Physics settings - every field round-trips through engine_settings.yaml and saves the same text twice")
{
    SettingsFiles files;
    Settings      settings;
    PhysicsSettings& physics = settings.GetPhysicsSettings();
    physics.Gravity          = { 0.5f, -1.25f, -20.0f };
    physics.LayerNames[1]    = "Player";
    physics.LayerNames[15]   = "Triggers";
    physics.SetCollides(1, 15, false);
    physics.SetCollides(4, 4, false);
    physics.WorkerThreads = 3;
    REQUIRE(settings.Save(PATH, files.Files));
    const std::string first = files.Read();
    CHECK(first.find("physics:") != std::string::npos);

    Settings loaded;
    REQUIRE(loaded.Load(PATH, files.Files));
    CHECK(loaded.GetPhysicsSettings() == physics);

    REQUIRE(loaded.Save(PATH, files.Files));
    CHECK(files.Read() == first);
}

TEST_CASE("Physics settings - a file without a physics section loads the defaults")
{
    SettingsFiles files;
    files.Write("shadowmap:\n  size: 1024\n");
    Settings fresh;
    REQUIRE(fresh.Load(PATH, files.Files));
    CHECK(fresh.GetPhysicsSettings() == PhysicsSettings{});
}

TEST_CASE("Physics settings - each bad value keeps its default with one warning naming it")
{
    struct Case
    {
        const char* Text;
        const char* Named;
    };
    const Case cases[] = {
        { "physics:\n  gravity: [0, .nan, -9.81]\n", "physics.gravity" },
        { "physics:\n  gravity: [0, -9.81]\n", "physics.gravity" },
        { "physics:\n  gravity: down\n", "physics.gravity" },
        { "physics:\n  layers: { 16: Far }\n", "'16'" },
        { "physics:\n  layers: { -1: Below }\n", "'-1'" },
        { "physics:\n  layers: { one: Player }\n", "'one'" },
        { "physics:\n  collisions: { 0: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 20] }\n", "'20'" },
        { "physics:\n  worker_threads: -2\n", "physics.worker_threads" },
        { "physics:\n  worker_threads: many\n", "physics.worker_threads" },
        { "physics: 3\n", "physics" },
    };
    for (const Case& bad : cases)
    {
        CAPTURE(bad.Text);
        SettingsFiles files;
        files.Write(bad.Text);
        Settings   settings;
        LogCapture log;
        REQUIRE(settings.Load(PATH, files.Files));
        const auto warnings = log.Lines("[WARN");
        REQUIRE(warnings.size() == 1);
        CHECK(warnings[0].find(bad.Named) != std::string::npos);
        CHECK(settings.GetPhysicsSettings() == PhysicsSettings{});
    }
}

TEST_CASE("Physics settings - a pair listed on one side only collides, with one warning naming both layers")
{
    SettingsFiles files;
    files.Write(CollisionsSection(3, 7));
    Settings   settings;
    LogCapture log;
    REQUIRE(settings.Load(PATH, files.Files));
    const auto warnings = log.Lines("[WARN");
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0].find("layers 3 and 7") != std::string::npos);
    CHECK(settings.GetPhysicsSettings().Collides(3, 7));
    CHECK(settings.GetPhysicsSettings().Collides(7, 3));

    // Listed on neither side, the pair is kept apart without a warning.
    std::string apart = CollisionsSection(3, 7);
    apart.replace(apart.find("    7: [0, 1, 2, 3, "), 20, "    7: [0, 1, 2, ");
    files.Write(apart);
    Settings   both;
    LogCapture quiet;
    REQUIRE(both.Load(PATH, files.Files));
    CHECK(quiet.Lines("[WARN").empty());
    CHECK_FALSE(both.GetPhysicsSettings().Collides(3, 7));
    CHECK(both.GetPhysicsSettings().Collides(3, 6));
}

TEST_CASE("Physics settings - the shipped FeatureTest and template files hold the default section and save as written")
{
    for (const char* folder : { "Projects/FeatureTest", "Templates/Empty" })
    {
        CAPTURE(folder);
        SettingsFiles shipped(RepositoryRoot() / folder);
        const std::string text = shipped.Read();
        REQUIRE(text.find("physics:") != std::string::npos);

        Settings   settings;
        LogCapture log;
        REQUIRE(settings.Load(PATH, shipped.Files));
        CHECK(log.Lines("[WARN").empty());
        CHECK(settings.GetPhysicsSettings() == PhysicsSettings{});

        SettingsFiles copy;
        REQUIRE(settings.Save(PATH, copy.Files));
        CHECK(copy.Read() == text);
    }
}
