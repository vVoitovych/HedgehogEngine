#include "doctest/doctest/doctest.h"

#include "FileSystem/api/PathUtils.hpp"

TEST_CASE("MakeAssetKey - one key per file under assets://, whatever the slashes, prefix or dots")
{
    CHECK(FS::MakeAssetKey("Models/a.png") == "Models/a.png");
    CHECK(FS::MakeAssetKey("Models\\a.png") == "Models/a.png");
    CHECK(FS::MakeAssetKey("assets://Models/a.png") == "Models/a.png");
    CHECK(FS::MakeAssetKey("assets://Models\\a.png") == "Models/a.png");
    CHECK(FS::MakeAssetKey("Models//./a.png") == "Models/a.png");
    CHECK(FS::MakeAssetKey("./Models/a.png") == "Models/a.png");
    CHECK(FS::MakeAssetKey("Models/A.png") == "Models/A.png");       // case kept
    CHECK(FS::MakeAssetKey("Models/../a.png") == "Models/../a.png"); // ".." is not folded
    CHECK(FS::MakeAssetKey("").empty());
    CHECK(FS::MakeAssetKey("assets://").empty());
}

TEST_CASE("MakeAssetKey - another mount is kept, its path normalized, and the key names the file")
{
    CHECK(FS::MakeAssetKey("engine://Content/Textures/Default/cells.png") == "engine://Content/Textures/Default/cells.png");
    CHECK(FS::MakeAssetKey("engine://Content\\Textures\\Default\\cells.png") == "engine://Content/Textures/Default/cells.png");
    CHECK(FS::MakeAssetKey("engine://Content//./Textures/cells.png") == "engine://Content/Textures/cells.png");

    CHECK(FS::ToAssetVirtualPath(FS::MakeAssetKey("Models\\a.png")) == "assets://Models/a.png");
    CHECK(FS::ToAssetVirtualPath(FS::MakeAssetKey("engine://Content\\a.png")) == "engine://Content/a.png");
}
