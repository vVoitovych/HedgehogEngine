#include "doctest/doctest/doctest.h"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "test_helpers.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Creates a FileSystem pre-registered with one alias pointing at dir.
static std::unique_ptr<FS::FileSystem> MakeFS(const std::string& alias,
                                               const std::filesystem::path& dir)
{
    auto fs = std::make_unique<FS::FileSystem>();
    fs->RegisterPath(alias, dir);
    return fs;
}

// ---------------------------------------------------------------------------
// FileSystemManager::Register
// ---------------------------------------------------------------------------

TEST_CASE("FileSystemManager::Register - empty FileSystem (no mount points) is rejected")
{
    FS::FileSystemManager manager;
    auto fs = std::make_unique<FS::FileSystem>();  // no RegisterPath calls

    CHECK(manager.Register(std::move(fs)) == false);
    CHECK(manager.GetFileSystems().empty());
}

TEST_CASE("FileSystemManager::Register - valid FileSystem is accepted")
{
    TempDir tmp;
    FS::FileSystemManager manager;

    CHECK(manager.Register(MakeFS("engine://", tmp.Path())) == true);
    CHECK(manager.GetFileSystems().size() == 1u);
}

TEST_CASE("FileSystemManager::Register - alias collision across two FileSystems is rejected")
{
    TempDir tmpA;
    TempDir tmpB;
    FS::FileSystemManager manager;

    REQUIRE(manager.Register(MakeFS("engine://", tmpA.Path())) == true);

    // Second FS tries to claim the same alias.
    auto fsB = MakeFS("engine://", tmpB.Path());
    CHECK(manager.Register(std::move(fsB)) == false);

    // Manager still has exactly one FS.
    CHECK(manager.GetFileSystems().size() == 1u);
}

TEST_CASE("FileSystemManager::Register - two FileSystems with distinct aliases are both accepted")
{
    TempDir tmpA;
    TempDir tmpB;
    FS::FileSystemManager manager;

    CHECK(manager.Register(MakeFS("engine://", tmpA.Path())) == true);
    CHECK(manager.Register(MakeFS("assets://", tmpB.Path())) == true);
    CHECK(manager.GetFileSystems().size() == 2u);
}

TEST_CASE("FileSystemManager::Register - collision detected when new FS has multiple aliases and one conflicts")
{
    TempDir tmpA;
    TempDir tmpB;
    TempDir tmpC;
    FS::FileSystemManager manager;

    REQUIRE(manager.Register(MakeFS("engine://", tmpA.Path())) == true);

    // Build a FS with two aliases: one new ("assets://") and one colliding ("engine://").
    auto fsMulti = std::make_unique<FS::FileSystem>();
    fsMulti->RegisterPath("assets://", tmpB.Path());
    fsMulti->RegisterPath("engine://", tmpC.Path());

    CHECK(manager.Register(std::move(fsMulti)) == false);
    // The "assets://" alias should NOT have leaked into the manager.
    CHECK(manager.GetFileSystems().size() == 1u);
}

// ---------------------------------------------------------------------------
// FileSystemManager::FindOwner
// ---------------------------------------------------------------------------

TEST_CASE("FileSystemManager::FindOwner - finds the correct owner for a registered alias")
{
    TempDir tmp;
    FS::FileSystemManager manager;
    REQUIRE(manager.Register(MakeFS("engine://", tmp.Path())) == true);

    const FS::FileSystem* owner = manager.FindOwner("engine://texture.png");
    REQUIRE(owner != nullptr);
    CHECK(owner->OwnsAlias("engine://"));
}

TEST_CASE("FileSystemManager::FindOwner - returns nullptr for unregistered alias")
{
    TempDir tmp;
    FS::FileSystemManager manager;
    REQUIRE(manager.Register(MakeFS("engine://", tmp.Path())) == true);

    CHECK(manager.FindOwner("assets://texture.png") == nullptr);
}

TEST_CASE("FileSystemManager::FindOwner - returns nullptr when manager is empty")
{
    FS::FileSystemManager manager;
    CHECK(manager.FindOwner("engine://anything") == nullptr);
}

TEST_CASE("FileSystemManager::FindOwner - returns nullptr for path without '://'")
{
    TempDir tmp;
    FS::FileSystemManager manager;
    REQUIRE(manager.Register(MakeFS("engine://", tmp.Path())) == true);

    CHECK(manager.FindOwner("engine/texture.png") == nullptr);
}

TEST_CASE("FileSystemManager::FindOwner - distinguishes between two registered FileSystems")
{
    TempDir tmpA;
    TempDir tmpB;
    FS::FileSystemManager manager;
    REQUIRE(manager.Register(MakeFS("engine://", tmpA.Path())) == true);
    REQUIRE(manager.Register(MakeFS("assets://", tmpB.Path())) == true);

    const FS::FileSystem* engineOwner = manager.FindOwner("engine://shaders/vert.spv");
    const FS::FileSystem* assetsOwner = manager.FindOwner("assets://meshes/cube.obj");

    REQUIRE(engineOwner != nullptr);
    REQUIRE(assetsOwner != nullptr);
    CHECK(engineOwner != assetsOwner);
    CHECK(engineOwner->OwnsAlias("engine://"));
    CHECK(assetsOwner->OwnsAlias("assets://"));
}

// ---------------------------------------------------------------------------
// FileSystemManager::ReadFile / ReadTextFile
// ---------------------------------------------------------------------------

TEST_CASE("FileSystemManager::ReadFile - reads file through correct owner")
{
    TempDir tmp;
    FS::FileSystemManager manager;
    REQUIRE(manager.Register(MakeFS("engine://", tmp.Path())) == true);

    const std::string content = "manager read test";
    tmp.WriteFile("data.bin", content);

    auto result = manager.ReadFile("engine://data.bin");

    REQUIRE(result.has_value());
    CHECK(result->size() == content.size());
    for (std::size_t i = 0; i < content.size(); ++i)
        CHECK(static_cast<char>((*result)[i]) == content[i]);
}

TEST_CASE("FileSystemManager::ReadFile - returns nullopt for unknown alias")
{
    TempDir tmp;
    FS::FileSystemManager manager;
    REQUIRE(manager.Register(MakeFS("engine://", tmp.Path())) == true);

    CHECK_FALSE(manager.ReadFile("assets://data.bin").has_value());
}

TEST_CASE("FileSystemManager::ReadFile - returns nullopt for missing file under known alias")
{
    TempDir tmp;
    FS::FileSystemManager manager;
    REQUIRE(manager.Register(MakeFS("engine://", tmp.Path())) == true);

    CHECK_FALSE(manager.ReadFile("engine://ghost.bin").has_value());
}

TEST_CASE("FileSystemManager::ReadTextFile - returns correct string content")
{
    TempDir tmp;
    FS::FileSystemManager manager;
    REQUIRE(manager.Register(MakeFS("engine://", tmp.Path())) == true);

    const std::string text = "hello from manager";
    tmp.WriteFile("note.txt", text);

    auto result = manager.ReadTextFile("engine://note.txt");

    REQUIRE(result.has_value());
    CHECK(*result == text);
}

TEST_CASE("FileSystemManager::ReadTextFile - routes to the right FileSystem among multiple")
{
    TempDir tmpEngine;
    TempDir tmpAssets;
    FS::FileSystemManager manager;
    REQUIRE(manager.Register(MakeFS("engine://", tmpEngine.Path())) == true);
    REQUIRE(manager.Register(MakeFS("assets://", tmpAssets.Path())) == true);

    tmpEngine.WriteFile("shader.glsl", "engine shader source");
    tmpAssets.WriteFile("texture.meta", "asset metadata");

    auto shader  = manager.ReadTextFile("engine://shader.glsl");
    auto texture = manager.ReadTextFile("assets://texture.meta");

    REQUIRE(shader.has_value());
    REQUIRE(texture.has_value());
    CHECK(*shader  == "engine shader source");
    CHECK(*texture == "asset metadata");
}

// ---------------------------------------------------------------------------
// FileSystemManager::Unregister
// ---------------------------------------------------------------------------

TEST_CASE("FileSystemManager::Unregister - removes and returns the matching FileSystem")
{
    TempDir tmp;
    FS::FileSystemManager manager;
    REQUIRE(manager.Register(MakeFS("engine://", tmp.Path())) == true);

    auto removed = manager.Unregister("engine://");

    REQUIRE(removed != nullptr);
    CHECK(removed->OwnsAlias("engine://"));
    CHECK(manager.GetFileSystems().empty());
}

TEST_CASE("FileSystemManager::Unregister - returns nullptr for unknown alias")
{
    TempDir tmp;
    FS::FileSystemManager manager;
    REQUIRE(manager.Register(MakeFS("engine://", tmp.Path())) == true);

    auto removed = manager.Unregister("assets://");

    CHECK(removed == nullptr);
    // Original FS still present.
    CHECK(manager.GetFileSystems().size() == 1u);
}

TEST_CASE("FileSystemManager::Unregister - after unregister, alias can be re-registered")
{
    TempDir tmpA;
    TempDir tmpB;
    FS::FileSystemManager manager;
    REQUIRE(manager.Register(MakeFS("engine://", tmpA.Path())) == true);

    auto removed = manager.Unregister("engine://");
    REQUIRE(removed != nullptr);
    CHECK(manager.GetFileSystems().empty());

    // Now the alias is free — registering it again must succeed.
    CHECK(manager.Register(MakeFS("engine://", tmpB.Path())) == true);
    CHECK(manager.GetFileSystems().size() == 1u);
}

TEST_CASE("FileSystemManager::Unregister - removes only the targeted FileSystem, leaves others intact")
{
    TempDir tmpA;
    TempDir tmpB;
    FS::FileSystemManager manager;
    REQUIRE(manager.Register(MakeFS("engine://", tmpA.Path())) == true);
    REQUIRE(manager.Register(MakeFS("assets://", tmpB.Path())) == true);

    auto removed = manager.Unregister("engine://");

    REQUIRE(removed != nullptr);
    CHECK(manager.GetFileSystems().size() == 1u);
    // The remaining FS must still own "assets://".
    CHECK(manager.FindOwner("assets://something") != nullptr);
    CHECK(manager.FindOwner("engine://something") == nullptr);
}

TEST_CASE("FileSystemManager::Unregister - unregisters by any alias owned by the FS")
{
    // A single FileSystem may own multiple aliases.
    // Unregister with any one of them must remove the whole FS.
    TempDir tmpA;
    TempDir tmpB;
    FS::FileSystemManager manager;

    auto fs = std::make_unique<FS::FileSystem>();
    REQUIRE(fs->RegisterPath("engine://", tmpA.Path()) == true);
    REQUIRE(fs->RegisterPath("shaders://", tmpB.Path()) == true);
    REQUIRE(manager.Register(std::move(fs)) == true);

    // Unregister via the second alias.
    auto removed = manager.Unregister("shaders://");

    REQUIRE(removed != nullptr);
    CHECK(manager.GetFileSystems().empty());
    // Both aliases are gone.
    CHECK(manager.FindOwner("engine://x")  == nullptr);
    CHECK(manager.FindOwner("shaders://x") == nullptr);
}

// ---------------------------------------------------------------------------
// FileSystemManager::GetFileSystems
// ---------------------------------------------------------------------------

TEST_CASE("FileSystemManager::GetFileSystems - empty on construction")
{
    FS::FileSystemManager manager;
    CHECK(manager.GetFileSystems().empty());
}

TEST_CASE("FileSystemManager::GetFileSystems - count reflects register/unregister sequence")
{
    TempDir tmpA;
    TempDir tmpB;
    FS::FileSystemManager manager;

    CHECK(manager.GetFileSystems().size() == 0u);

    REQUIRE(manager.Register(MakeFS("engine://", tmpA.Path())) == true);
    CHECK(manager.GetFileSystems().size() == 1u);

    REQUIRE(manager.Register(MakeFS("assets://", tmpB.Path())) == true);
    CHECK(manager.GetFileSystems().size() == 2u);

    manager.Unregister("engine://");
    CHECK(manager.GetFileSystems().size() == 1u);

    manager.Unregister("assets://");
    CHECK(manager.GetFileSystems().size() == 0u);
}
