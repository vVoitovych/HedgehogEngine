#include "doctest/doctest/doctest.h"

#include "HedgehogInput/api/DefaultInputActions.hpp"
#include "HedgehogInput/api/InputActionAsset.hpp"
#include "HedgehogInput/api/InputActionFile.hpp"

#include "HedgehogEngine/HedgehogWindow/api/InputCodes.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/tests/test_helpers.hpp"
#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

using namespace HInput;
using namespace std::chrono_literals;

namespace
{
    const std::string PATH = "assets://actions.yaml";

    // A temporary assets:// holding an actions file.
    struct ActionsFolder
    {
        TempDir               Dir;
        FS::FileSystemManager Files;

        explicit ActionsFolder(const std::string& text)
        {
            Dir.WriteFile("actions.yaml", text);
            auto fileSystem = std::make_unique<FS::FileSystem>();
            fileSystem->RegisterPath("assets://", Dir.Path());
            REQUIRE(Files.Register(std::move(fileSystem)));
        }

        // Rewrites the file and moves its write time on, so a poll sees a new version even when the
        // clock's resolution would not.
        void Rewrite(const std::string& text)
        {
            const std::filesystem::path path = Dir.Path() / "actions.yaml";
            const auto                  time = std::filesystem::last_write_time(path);
            Dir.WriteFile("actions.yaml", text);
            std::filesystem::last_write_time(path, time + 2s);
        }
    };

    std::string WithJumpOn(const char* key)
    {
        return std::string("Version: 1\nGame:\n  Jump:\n    - Key: ") + key + "\n";
    }

    uint16_t JumpKey(const InputActionSet& set)
    {
        return set.Game.Actions.at(0).Bindings.at(0).Code;
    }
}

TEST_CASE("Actions asset - a saved file is read again once the poll interval has passed")
{
    ActionsFolder folder(WithJumpOn("Space"));
    std::optional<InputActionSet> loaded = LoadInputActions(PATH, folder.Files);
    REQUIRE(loaded.has_value());
    InputActionSet    actions = *loaded;
    InputActionsWatch watch   = WatchInputActions(PATH, folder.Files);

    const auto start = std::chrono::steady_clock::now();
    CHECK_FALSE(PollInputActions(watch, folder.Files, start, actions)); // nothing new

    folder.Rewrite(WithJumpOn("W"));
    LogCapture log;
    CHECK_FALSE(PollInputActions(watch, folder.Files, start + 500ms, actions)); // too soon
    CHECK(JumpKey(actions) == static_cast<uint16_t>(HW::Key::Space));

    CHECK(PollInputActions(watch, folder.Files, start + 1500ms, actions));
    CHECK(JumpKey(actions) == static_cast<uint16_t>(HW::Key::W));
    CHECK(log.Lines("[Input] Reloaded assets://actions.yaml.").size() == 1);

    CHECK_FALSE(PollInputActions(watch, folder.Files, start + 3s, actions)); // the same version again
}

TEST_CASE("Actions asset - a broken save keeps the old actions, is reported once, and the next good save is read")
{
    ActionsFolder     folder(WithJumpOn("Space"));
    InputActionSet    actions = *LoadInputActions(PATH, folder.Files);
    InputActionsWatch watch   = WatchInputActions(PATH, folder.Files);
    const auto        start   = std::chrono::steady_clock::now();

    LogCapture log;
    folder.Rewrite(WithJumpOn("Spcae"));
    CHECK_FALSE(PollInputActions(watch, folder.Files, start, actions));
    CHECK(JumpKey(actions) == static_cast<uint16_t>(HW::Key::Space));
    CHECK(log.Lines("unknown key 'Spcae'").size() == 1);

    CHECK_FALSE(PollInputActions(watch, folder.Files, start + 2s, actions));
    CHECK(log.Lines("unknown key 'Spcae'").size() == 1); // not again for the same version

    folder.Rewrite(WithJumpOn("Enter"));
    CHECK(PollInputActions(watch, folder.Files, start + 4s, actions));
    CHECK(JumpKey(actions) == static_cast<uint16_t>(HW::Key::Enter));
}

TEST_CASE("Actions asset - a missing or unreadable file is reported and changes nothing")
{
    ActionsFolder folder(WithJumpOn("Space"));
    LogCapture    log;
    CHECK_FALSE(LoadInputActions("assets://missing.yaml", folder.Files).has_value());
    CHECK(log.Lines("[Input] assets://missing.yaml: the file cannot be read.").size() == 1);

    InputActionSet    actions = MakeDefaultInputActions();
    InputActionsWatch watch   = WatchInputActions("assets://missing.yaml", folder.Files);
    CHECK_FALSE(PollInputActions(watch, folder.Files, std::chrono::steady_clock::now(), actions));
    CHECK(actions == MakeDefaultInputActions());

    folder.Rewrite("Version: 1\nGame: [\n");
    CHECK_FALSE(LoadInputActions(PATH, folder.Files).has_value());
    CHECK(log.Lines("[Input] assets://actions.yaml: not valid YAML").size() == 1);
}
