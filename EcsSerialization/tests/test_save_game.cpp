#include "doctest/doctest/doctest.h"

#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"
#include "EcsSerialization/api/EcsSerializer.hpp"
#include "EcsSerialization/api/SaveGame/SaveGameFile.hpp"
#include "EcsSerialization/api/SaveGame/SaveSlotStore.hpp"

#include "yaml-cpp/yaml.h"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "FileSystem/tests/test_helpers.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace EcsSerialization;

namespace
{
    ECS::ECS MakeWorld(const std::string& childName)
    {
        ECS::ECS ecs;
        ecs.Init();
        ecs.RegisterComponent<ECS::HierarchyComponent>();
        const ECS::Entity root = ecs.CreateEntity();
        ecs.AddComponent(root, ECS::HierarchyComponent{ "Root", ECS::INVALID_ENTITY, {} });
        ecs.SetRoot(root);
        const ECS::Entity child = ecs.CreateEntity();
        ecs.AddComponent(child, ECS::HierarchyComponent{ childName, root, {} });
        ecs.GetComponent<ECS::HierarchyComponent>(root).Children.push_back(child);
        return ecs;
    }

    SaveGameFile MakeSave(const std::string& childName, double playTime)
    {
        ComponentSerializerRegistry registry;
        const ECS::ECS              world = MakeWorld(childName);

        SaveGameFile save;
        save.Metadata.GameDataVersion = 3;
        save.Metadata.ScenePath       = "assets://Scenes/Default.yaml";
        save.Metadata.Timestamp       = "2026-10-03T21:30:00Z";
        save.Metadata.PlayTime        = playTime;
        save.Sections[SAVE_SECTION_WORLD] = EcsSerializer::SerializeToNode(registry, world, "Default");
        save.Sections[SAVE_SECTION_SCRIPTS] = YAML::Load("{ Player: { score: 42, name: Hedgehog } }");
        return save;
    }

    // The world section's child entity's name, after loading it into a fresh ECS.
    std::string WorldChildName(const SaveGameFile& save)
    {
        ComponentSerializerRegistry registry;
        ECS::ECS                    ecs;
        ecs.Init();
        ecs.RegisterComponent<ECS::HierarchyComponent>();
        std::string sceneName;
        REQUIRE(EcsSerializer::DeserializeFromNode(registry, ecs, sceneName, save.Sections.at(SAVE_SECTION_WORLD), "save"));
        const auto& children = ecs.GetComponent<ECS::HierarchyComponent>(ecs.GetRoot()).Children;
        REQUIRE(children.size() == 1);
        return ecs.GetComponent<ECS::HierarchyComponent>(children[0]).Name;
    }

    std::string ReadFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), {});
    }
}

TEST_CASE("Save game - a save round-trips through its text, metadata and sections")
{
    const SaveGameFile save = MakeSave("Player", 125.5);
    const std::string  text = WriteSaveGame(save);
    CHECK(text.starts_with("Metadata:\n  SaveVersion: 1\n"));

    const SaveGameReadResult<SaveGameFile> read = ReadSaveGame(text);
    REQUIRE_MESSAGE(read.Value.has_value(), read.Error);
    CHECK(read.Value->Metadata == save.Metadata);
    CHECK(read.Value->Sections.size() == 2);
    CHECK(WorldChildName(*read.Value) == "Player");
    CHECK(read.Value->Sections.at(SAVE_SECTION_SCRIPTS)["Player"]["score"].as<int>() == 42);
    CHECK(WriteSaveGame(*read.Value) == text);

    const SaveGameReadResult<SaveGameMetadata> header = ReadSaveGameMetadata(text);
    REQUIRE(header.Value.has_value());
    CHECK(*header.Value == save.Metadata);
}

TEST_CASE("Save game - reading names what is wrong, and a newer format is refused but still listed")
{
    const std::string text = WriteSaveGame(MakeSave("Player", 1.0));

    std::string missingScene = text;
    missingScene.replace(missingScene.find("  Scene:"), 7, "  Level:");
    CHECK(ReadSaveGame(missingScene).Error == "Metadata.Scene is missing or is not a string");
    CHECK(ReadSaveGameMetadata(missingScene).Error == "Metadata.Scene is missing or is not a string");

    std::string badTime = text;
    badTime.replace(badTime.find("PlayTime: 1"), 11, "PlayTime: soon");
    CHECK(ReadSaveGame(badTime).Error == "Metadata.PlayTime is missing or is not a number");

    std::string newer = text;
    newer.replace(newer.find("SaveVersion: 1"), 14, "SaveVersion: 2");
    CHECK(ReadSaveGame(newer).Error == "the save is format version 2, but this build reads versions 1 to 1");
    REQUIRE(ReadSaveGameMetadata(newer).Value.has_value());
    CHECK(ReadSaveGameMetadata(newer).Value->SaveVersion == 2);

    CHECK(ReadSaveGame(text.substr(0, text.find("\n---\n"))).Error == "a save holds 2 YAML documents, not 1");
    CHECK_FALSE(ReadSaveGame("Metadata: [unclosed").Error.empty());
    CHECK(ReadSaveGame("Metadata: 5\n---\nSections: {}\n").Error == "the save has no Metadata map");
}

TEST_CASE("Save slots - write, list, read and delete in a directory")
{
    TempDir             tmp;
    const SaveSlotStore store(tmp.Path() / "Saves" / "Editor"); // created on the first write

    CHECK(store.List().empty());
    REQUIRE(store.Write("slot_2", MakeSave("Second", 20.0)));
    REQUIRE(store.Write("Slot-1", MakeSave("First", 10.0)));
    CHECK(std::filesystem::is_regular_file(tmp.Path() / "Saves" / "Editor" / "slot_2.save"));
    CHECK(store.Exists("slot_2"));

    const std::vector<SaveSlotInfo> slots = store.List();
    REQUIRE(slots.size() == 2);
    CHECK(slots[0].Name == "Slot-1");
    CHECK(slots[0].Metadata.PlayTime == 10.0);
    CHECK(slots[1].Name == "slot_2");
    CHECK(slots[1].Metadata.ScenePath == "assets://Scenes/Default.yaml");

    const std::optional<SaveGameFile> second = store.Read("slot_2");
    REQUIRE(second.has_value());
    CHECK(WorldChildName(*second) == "Second");

    // Overwriting replaces the slot.
    REQUIRE(store.Write("slot_2", MakeSave("Replaced", 30.0)));
    CHECK(WorldChildName(*store.Read("slot_2")) == "Replaced");

    CHECK(store.Delete("slot_2"));
    CHECK_FALSE(store.Exists("slot_2"));
    CHECK_FALSE(store.Delete("slot_2"));
    LogCapture log;
    CHECK_FALSE(store.Read("slot_2").has_value());
    CHECK(log.Lines("[Save] Slot 'slot_2' does not exist.").size() == 1);
    CHECK(store.List().size() == 1);
}

TEST_CASE("Save slots - a listing reads headers only, and skips files it cannot read")
{
    TempDir             tmp;
    const SaveSlotStore store(tmp.Path());
    REQUIRE(store.Write("good", MakeSave("Player", 5.0)));

    // A save whose world is broken: listed from its header, refused when read.
    std::string broken = ReadFile(tmp.Path() / "good.save");
    broken = broken.substr(0, broken.find("\n---\n")) + "\n---\nSections: [unclosed\n";
    tmp.WriteFile("broken_world.save", broken);
    tmp.WriteFile("garbage.save", "not: [a save");
    tmp.WriteFile("notes.txt", "ignored");
    tmp.WriteFile("bad name.save", ReadFile(tmp.Path() / "good.save"));

    LogCapture                      log;
    const std::vector<SaveSlotInfo> slots = store.List();
    REQUIRE(slots.size() == 2);
    CHECK(slots[0].Name == "broken_world");
    CHECK(slots[0].Metadata.PlayTime == 5.0);
    CHECK(slots[1].Name == "good");
    CHECK(log.Lines("[Save] Slot 'garbage' is skipped").size() == 1);

    CHECK_FALSE(store.Read("broken_world").has_value());
    CHECK(log.Lines("[Save] Slot 'broken_world' cannot be read").size() == 1);
}

TEST_CASE("Save slots - a failed write leaves the previous save readable")
{
    TempDir             tmp;
    const SaveSlotStore store(tmp.Path());
    REQUIRE(store.Write("slot", MakeSave("Before", 1.0)));

    // A crash mid-write leaves a partial temp file: it is never listed or read as the slot.
    tmp.WriteFile("slot.tmp", "Metadata:\n  SaveVersion: 1\n  Game");
    CHECK(store.List().size() == 1);
    CHECK(WorldChildName(*store.Read("slot")) == "Before");

    // A write that cannot create its temp file fails without touching the slot.
    std::filesystem::remove(tmp.Path() / "slot.tmp");
    std::filesystem::create_directory(tmp.Path() / "slot.tmp");
    {
        LogCapture log;
        CHECK_FALSE(store.Write("slot", MakeSave("After", 2.0)));
        CHECK(log.Lines("[Save] Slot 'slot': cannot write").size() == 1);
    }
    CHECK(WorldChildName(*store.Read("slot")) == "Before");

    // Once the obstacle is gone the next write replaces it.
    std::filesystem::remove(tmp.Path() / "slot.tmp");
    REQUIRE(store.Write("slot", MakeSave("After", 2.0)));
    CHECK(WorldChildName(*store.Read("slot")) == "After");
    CHECK_FALSE(std::filesystem::exists(tmp.Path() / "slot.tmp"));
}

TEST_CASE("Save slots - names that could leave the directory are rejected")
{
    TempDir             tmp;
    const SaveSlotStore store(tmp.Path() / "Saves");

    CHECK(SaveSlotStore::IsValidSlotName("a"));
    CHECK(SaveSlotStore::IsValidSlotName(std::string(64, 'x')));
    CHECK(SaveSlotStore::IsValidSlotName("Quick_Save-01"));

    const std::vector<std::string> badNames = { "", "..", "../escape", "a/b", "a\\b", "C:slot", "C:\\slot", "/root",
                                                "with space", "dot.ted", std::string(65, 'x'), std::string("nul\0l", 5) };
    for (const std::string& bad : badNames)
    {
        CAPTURE(bad);
        CHECK_FALSE(SaveSlotStore::IsValidSlotName(bad));
        LogCapture log;
        CHECK_FALSE(store.Write(bad, MakeSave("Player", 1.0)));
        CHECK_FALSE(store.Read(bad).has_value());
        CHECK_FALSE(store.Delete(bad));
        CHECK_FALSE(store.Exists(bad));
        CHECK(log.Lines("is not a slot name").size() == 4);
    }
    CHECK_FALSE(std::filesystem::exists(tmp.Path() / "Saves"));
    CHECK_FALSE(std::filesystem::exists(tmp.Path() / "escape.save"));
}
