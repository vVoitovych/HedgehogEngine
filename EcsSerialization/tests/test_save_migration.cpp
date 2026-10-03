#include "doctest/doctest/doctest.h"

#include "EcsSerialization/api/SaveGame/SaveMigration.hpp"
#include "EcsSerialization/api/SaveGame/SaveSlotStore.hpp"

#include "yaml-cpp/yaml.h"

#include "FileSystem/tests/test_helpers.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <stdexcept>
#include <string>

using namespace EcsSerialization;

namespace
{
    // A save whose "Log" section lists the steps that ran.
    SaveGameFile MakeSave()
    {
        SaveGameFile save;
        save.Sections["Log"] = YAML::Node(YAML::NodeType::Sequence);
        return save;
    }

    SaveMigrationStep Append(const std::string& entry)
    {
        return [entry](SaveGameFile& save)
        {
            save.Sections["Log"].push_back(entry);
            return std::string();
        };
    }

    std::string Steps(const SaveGameFile& save)
    {
        std::string steps;
        for (const YAML::Node& entry : save.Sections.at("Log"))
            steps += entry.as<std::string>() + " ";
        return steps;
    }
}

TEST_CASE("Save migration registry - steps run in version order, and a version without one needs none")
{
    SaveMigrationRegistry registry;
    registry.Register(3, Append("3to4"));
    registry.Register(1, Append("1to2"));
    CHECK(registry.Has(1));
    CHECK_FALSE(registry.Has(2));

    SaveGameFile save = MakeSave();
    CHECK(registry.Migrate(save, 1, 4).empty());
    CHECK(Steps(save) == "1to2 3to4 ");

    // Only the steps between the versions run; nothing for an up-to-date or newer save.
    SaveGameFile later = MakeSave();
    CHECK(registry.Migrate(later, 2, 4).empty());
    CHECK(Steps(later) == "3to4 ");
    SaveGameFile current = MakeSave();
    CHECK(registry.Migrate(current, 4, 4).empty());
    CHECK(registry.Migrate(current, 5, 4).empty());
    CHECK(Steps(current).empty());

    // Registering a step again replaces it.
    registry.Register(1, Append("1to2b"));
    SaveGameFile replaced = MakeSave();
    CHECK(registry.Migrate(replaced, 1, 2).empty());
    CHECK(Steps(replaced) == "1to2b ");
}

TEST_CASE("Save migration registry - the first failing step stops the migration and names the versions")
{
    SaveMigrationRegistry registry;
    registry.Register(1, Append("1to2"));
    registry.Register(2, [](SaveGameFile&) { return std::string("no inventory"); });
    registry.Register(3, Append("3to4"));
    registry.Register(4, [](SaveGameFile&) -> std::string { throw std::runtime_error("bad node"); });

    SaveGameFile save = MakeSave();
    CHECK(registry.Migrate(save, 1, 4) == "version 2 to 3: no inventory");
    CHECK(Steps(save) == "1to2 ");

    SaveGameFile thrown = MakeSave();
    CHECK(registry.Migrate(thrown, 4, 5) == "version 4 to 5: bad node");

    LogCapture log;
    registry.Register(0, Append("0to1"));
    CHECK_FALSE(registry.Has(0));
    CHECK(log.Lines("[Save] A migration from version 0 is ignored").size() == 1);
}

TEST_CASE("Save slot store - ReadMetadata reads a slot's header, a newer format included")
{
    TempDir       dir;
    SaveSlotStore store(dir.Path());
    SaveGameFile  save;
    save.Metadata.SaveVersion     = SAVE_FORMAT_VERSION + 1;
    save.Metadata.GameDataVersion = 4;
    save.Sections["World"]        = YAML::Node(YAML::NodeType::Map);
    REQUIRE(store.Write("next", save));

    const auto metadata = store.ReadMetadata("next");
    REQUIRE(metadata);
    CHECK(metadata->SaveVersion == SAVE_FORMAT_VERSION + 1);
    CHECK(metadata->GameDataVersion == 4);

    LogCapture log;
    CHECK_FALSE(store.ReadMetadata("missing"));
    CHECK(log.Lines("[Save] Slot 'missing' does not exist.").size() == 1);
    CHECK_FALSE(store.ReadMetadata("../x"));
}
