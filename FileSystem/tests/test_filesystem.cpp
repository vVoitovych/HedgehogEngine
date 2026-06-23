#include "doctest/doctest/doctest.h"

#include "FileSystem/api/FileSystem.hpp"
#include "test_helpers.hpp"

// ---------------------------------------------------------------------------
// FileSystem::RegisterPath
// ---------------------------------------------------------------------------

TEST_CASE("FileSystem::RegisterPath - valid alias and existing directory succeeds")
{
    TempDir tmp;
    FS::FileSystem fs;

    bool result = fs.RegisterPath("engine://", tmp.Path());

    CHECK(result == true);
    CHECK(fs.GetMountPoints().size() == 1);
    CHECK(fs.GetMountPoints().count("engine://") == 1);
}

TEST_CASE("FileSystem::RegisterPath - alias without '://' is rejected")
{
    TempDir tmp;
    FS::FileSystem fs;

    SUBCASE("bare name")
    {
        CHECK(fs.RegisterPath("engine", tmp.Path()) == false);
    }
    SUBCASE("single colon only")
    {
        CHECK(fs.RegisterPath("engine:", tmp.Path()) == false);
    }
    SUBCASE("double slash but no colon")
    {
        CHECK(fs.RegisterPath("engine//", tmp.Path()) == false);
    }

    // Nothing was registered.
    CHECK(fs.GetMountPoints().empty());
}

TEST_CASE("FileSystem::RegisterPath - duplicate alias is rejected")
{
    TempDir tmp;
    FS::FileSystem fs;

    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    // Registering the same alias a second time must fail.
    CHECK(fs.RegisterPath("engine://", tmp.Path()) == false);
    CHECK(fs.GetMountPoints().size() == 1);
}

TEST_CASE("FileSystem::RegisterPath - non-existent path is rejected")
{
    FS::FileSystem fs;
    std::filesystem::path missing = std::filesystem::temp_directory_path() / "does_not_exist_xyzzy";

    CHECK(fs.RegisterPath("engine://", missing) == false);
    CHECK(fs.GetMountPoints().empty());
}

TEST_CASE("FileSystem::RegisterPath - path that is a file (not a directory) is rejected")
{
    TempDir tmp;
    FS::FileSystem fs;
    auto filePath = tmp.WriteFile("not_a_dir.txt", "hello");

    CHECK(fs.RegisterPath("engine://", filePath) == false);
    CHECK(fs.GetMountPoints().empty());
}

TEST_CASE("FileSystem::RegisterPath - multiple distinct aliases are all accepted")
{
    TempDir tmpA;
    TempDir tmpB;
    FS::FileSystem fs;

    CHECK(fs.RegisterPath("engine://", tmpA.Path()) == true);
    CHECK(fs.RegisterPath("assets://", tmpB.Path()) == true);
    CHECK(fs.GetMountPoints().size() == 2);
}

// ---------------------------------------------------------------------------
// FileSystem::OwnsAlias
// ---------------------------------------------------------------------------

TEST_CASE("FileSystem::OwnsAlias - returns true only for registered aliases")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    CHECK(fs.OwnsAlias("engine://") == true);
    CHECK(fs.OwnsAlias("assets://") == false);
    CHECK(fs.OwnsAlias("engine")    == false);   // no :// suffix
}

// ---------------------------------------------------------------------------
// FileSystem::OwnsPath
// ---------------------------------------------------------------------------

TEST_CASE("FileSystem::OwnsPath - returns true when alias portion is registered")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    CHECK(fs.OwnsPath("engine://textures/foo.png") == true);
    CHECK(fs.OwnsPath("engine://")                 == true);  // alias with empty relative part
    CHECK(fs.OwnsPath("assets://textures/foo.png") == false);
}

TEST_CASE("FileSystem::OwnsPath - virtual path without '://' returns false")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    CHECK(fs.OwnsPath("engine/textures/foo.png") == false);
    CHECK(fs.OwnsPath("plain_string")            == false);
}

// ---------------------------------------------------------------------------
// FileSystem::ReadFile / ReadTextFile
// ---------------------------------------------------------------------------

TEST_CASE("FileSystem::ReadFile - reads existing file and returns correct bytes")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    const std::string content = "Hello, FileSystem!";
    tmp.WriteFile("hello.txt", content);

    auto result = fs.ReadFile("engine://hello.txt");

    REQUIRE(result.has_value());
    CHECK(result->size() == content.size());

    // Byte-by-byte compare.
    for (std::size_t i = 0; i < content.size(); ++i)
        CHECK(static_cast<char>((*result)[i]) == content[i]);
}

TEST_CASE("FileSystem::ReadFile - returns nullopt for unregistered alias")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    auto result = fs.ReadFile("assets://missing.txt");
    CHECK_FALSE(result.has_value());
}

TEST_CASE("FileSystem::ReadFile - returns nullopt for missing file under registered alias")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    auto result = fs.ReadFile("engine://does_not_exist.bin");
    CHECK_FALSE(result.has_value());
}

TEST_CASE("FileSystem::ReadFile - returns nullopt for virtual path without '://'")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    auto result = fs.ReadFile("engine/hello.txt");
    CHECK_FALSE(result.has_value());
}

TEST_CASE("FileSystem::ReadFile - reads binary content including null bytes correctly")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    std::string binary;
    binary.push_back('\x00');
    binary.push_back('\xFF');
    binary.push_back('\x42');
    tmp.WriteFile("binary.bin", binary);

    auto result = fs.ReadFile("engine://binary.bin");

    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3u);
    CHECK(static_cast<unsigned char>((*result)[0]) == 0x00u);
    CHECK(static_cast<unsigned char>((*result)[1]) == 0xFFu);
    CHECK(static_cast<unsigned char>((*result)[2]) == 0x42u);
}

TEST_CASE("FileSystem::ReadFile - reads empty file and returns empty vector")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    tmp.WriteFile("empty.txt", "");

    auto result = fs.ReadFile("engine://empty.txt");

    REQUIRE(result.has_value());
    CHECK(result->empty());
}

TEST_CASE("FileSystem::ReadTextFile - content matches what was written")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    const std::string text = "line one\nline two\n";
    tmp.WriteFile("doc.txt", text);

    auto result = fs.ReadTextFile("engine://doc.txt");

    REQUIRE(result.has_value());
    CHECK(*result == text);
}

TEST_CASE("FileSystem::ReadTextFile - returns nullopt for missing file")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    auto result = fs.ReadTextFile("engine://ghost.txt");
    CHECK_FALSE(result.has_value());
}

TEST_CASE("FileSystem::ReadFile - file in subdirectory is resolved correctly")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    const std::string content = "deep content";
    tmp.WriteFile("subdir/nested.txt", content);

    auto result = fs.ReadTextFile("engine://subdir/nested.txt");

    REQUIRE(result.has_value());
    CHECK(*result == content);
}

// ---------------------------------------------------------------------------
// FileSystem::WriteFile / WriteTextFile
// ---------------------------------------------------------------------------

TEST_CASE("FileSystem::WriteTextFile then ReadTextFile round-trips identical content")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    const std::string content = "Hello, write round-trip!";
    REQUIRE(fs.WriteTextFile("engine://roundtrip.txt", content) == true);

    auto result = fs.ReadTextFile("engine://roundtrip.txt");
    REQUIRE(result.has_value());
    CHECK(*result == content);
}

TEST_CASE("FileSystem::WriteFile then ReadFile round-trips identical bytes including embedded nulls")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    std::vector<std::byte> data = {
        std::byte{0x00}, std::byte{0x01}, std::byte{0xFF}, std::byte{0x42}
    };
    REQUIRE(fs.WriteFile("engine://binary.bin", data) == true);

    auto result = fs.ReadFile("engine://binary.bin");
    REQUIRE(result.has_value());
    REQUIRE(result->size() == data.size());
    for (std::size_t i = 0; i < data.size(); ++i)
        CHECK((*result)[i] == data[i]);
}

TEST_CASE("FileSystem::WriteFile creates parent directories when they do not exist")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    const std::string content = "nested file";
    REQUIRE(fs.WriteTextFile("engine://a/b/c/nested.txt", content) == true);

    auto result = fs.ReadTextFile("engine://a/b/c/nested.txt");
    REQUIRE(result.has_value());
    CHECK(*result == content);
}

TEST_CASE("FileSystem::WriteTextFile returns false for unrecognised alias")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    CHECK(fs.WriteTextFile("assets://missing.txt", "data") == false);
}

// ---------------------------------------------------------------------------
// FileSystem::Exists
// ---------------------------------------------------------------------------

TEST_CASE("FileSystem::Exists returns true for a file written through FileSystem")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    REQUIRE(fs.WriteTextFile("engine://exists.txt", "some content") == true);
    CHECK(fs.Exists("engine://exists.txt") == true);
}

TEST_CASE("FileSystem::Exists returns false for a missing file under a registered alias")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    CHECK(fs.Exists("engine://ghost.txt") == false);
}

TEST_CASE("FileSystem::Exists returns false for an unrecognised alias")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    CHECK(fs.Exists("assets://something.txt") == false);
}

// ---------------------------------------------------------------------------
// FileSystem::ResolvePhysical
// ---------------------------------------------------------------------------

TEST_CASE("FileSystem::ResolvePhysical returns the expected physical path for a known alias")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    auto result = fs.ResolvePhysical("engine://textures/foo.png");
    REQUIRE(result.has_value());
    // The resolved path must start with the canonical mount directory.
    const auto canonical = std::filesystem::canonical(tmp.Path());
    CHECK(result->string().find(canonical.string()) == 0);
}

TEST_CASE("FileSystem::ResolvePhysical returns nullopt for unrecognised alias")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    CHECK(fs.ResolvePhysical("assets://missing.png") == std::nullopt);
}

// ---------------------------------------------------------------------------
// FileSystem::GetMountPoints
// ---------------------------------------------------------------------------

TEST_CASE("FileSystem::GetMountPoints - returns empty map on fresh instance")
{
    FS::FileSystem fs;
    CHECK(fs.GetMountPoints().empty());
}

TEST_CASE("FileSystem::GetMountPoints - returned paths are canonical")
{
    TempDir tmp;
    FS::FileSystem fs;
    REQUIRE(fs.RegisterPath("engine://", tmp.Path()) == true);

    const auto& mounts = fs.GetMountPoints();
    REQUIRE(mounts.count("engine://") == 1);

    // canonical() on the same path must produce an identical result.
    auto expected = std::filesystem::canonical(tmp.Path());
    CHECK(mounts.at("engine://") == expected);
}
