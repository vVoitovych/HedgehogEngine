#include "doctest/doctest/doctest.h"

#include "Tools/NewProjectCheck.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    // A temp parent folder, removed at the end of the test.
    struct Parent
    {
        std::filesystem::path Root = std::filesystem::temp_directory_path() /
                                     ("new_project_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

        Parent() { std::filesystem::create_directories(Root); }
        ~Parent()
        {
            std::error_code error;
            std::filesystem::remove_all(Root, error);
        }
    };
}

TEST_CASE("NewProjectCheck - a valid name under an existing folder can be created")
{
    Parent parent;
    CHECK(Editor::CheckNewProject("MyGame", parent.Root).empty());
    CHECK(Editor::CheckNewProject("My Game-2", parent.Root).empty());
    CHECK(Editor::MakeNewProjectFolder("MyGame", parent.Root) == (parent.Root / "MyGame").lexically_normal());

    // An empty folder of that name is fine too.
    std::filesystem::create_directories(parent.Root / "Empty");
    CHECK(Editor::CheckNewProject("Empty", parent.Root).empty());
}

TEST_CASE("NewProjectCheck - a bad name, parent or folder is refused with its reason")
{
    Parent parent;
    for (const char* bad : { "", "a/b", "..", " Lead", "C:", "x\\y" })
    {
        CAPTURE(bad);
        CHECK(Editor::CheckNewProject(bad, parent.Root).find("Not a project name") != std::string::npos);
    }

    CHECK(Editor::CheckNewProject("MyGame", parent.Root / "Missing").find("parent folder does not exist") != std::string::npos);
    CHECK(Editor::CheckNewProject("MyGame", {}).find("parent folder does not exist") != std::string::npos);

    std::filesystem::create_directories(parent.Root / "Taken");
    std::ofstream(parent.Root / "Taken" / "notes.txt") << "mine";
    CHECK(Editor::CheckNewProject("Taken", parent.Root).find("is not an empty folder") != std::string::npos);

    std::ofstream(parent.Root / "AFile") << "x";
    CHECK(Editor::CheckNewProject("AFile", parent.Root).find("is not an empty folder") != std::string::npos);
}
