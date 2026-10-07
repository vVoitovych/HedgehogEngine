#pragma once

// The fixture project the Cooker's tests cook and package. Test code only.

#include "FileSystem/tests/test_helpers.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace CookTest
{
    inline constexpr const char* LEVEL = R"(Version: 1
Scene name: Level
Scene:
  - Entity: 0
    Name: Root
    Parent: 0
    Children:
      - Entity: 1
        Name: Crate
        Parent: 0
        MeshComponent:
          MeshPath: Models\crate.obj
        RenderComponent:
          Material: Materials\Crate.material
        Children: []
)";

    // The engine files every cook packages: the default meshes and texture in Content, and engine
    // graphs whose only pass draws with no shader of its own.
    inline void WriteEngineFiles(const TempDir& engine)
    {
        engine.WriteFile("Content/Models/Default/cube.obj", "o cube\n");
        engine.WriteFile("Content/Models/Default/sphere.obj", "o sphere\n");
        engine.WriteFile("Content/Textures/Default/cells.png", "cells");
        for (const char* graph : { "scene", "game", "result" })
            engine.WriteFile(std::string("HedgehogEngine/HedgehogRenderer/assets/Graphs/") + graph + ".graph",
                             "version: 2\npasses:\n  - type: Ui\n    name: Ui\n");
    }

    // A project with one scene, the files the engine always loads (the project is its own engine
    // here, as in the dev tree), and assets nothing references.
    struct FixtureProject
    {
        TempDir Project;
        TempDir Out;

        FixtureProject()
        {
            Project.WriteFile("Project.yaml", "name: Fixture\nstartup_scene: assets://Scenes/Level.yaml\n");
            Project.WriteFile("Assets/Scenes/Level.yaml", LEVEL);
            Project.WriteFile("Assets/Models/crate.obj", "o crate\n");
            Project.WriteFile("Assets/Materials/Crate.material", "Type: 0\nBaseColor: Textures/crate.png\nTransparency: 1\n");
            Project.WriteFile("Assets/Textures/crate.png", "crate");
            WriteEngineFiles(Project);
            // Not referenced: never packaged.
            Project.WriteFile("Assets/Textures/unused.png", "unused");
            Project.WriteFile("Assets/Scenes/Other.yaml", "Scene name: Other\nScene: []\n");
        }

        // Every file under Out, relative and with forward slashes, sorted.
        std::vector<std::string> PackagedFiles() const
        {
            std::vector<std::string> files;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(Out.Path()))
                if (entry.is_regular_file())
                    files.push_back(std::filesystem::relative(entry.path(), Out.Path()).generic_string());
            std::sort(files.begin(), files.end());
            return files;
        }
    };

    inline const std::vector<std::string> EXPECTED_PACKAGE = {
        "Assets/Materials/Crate.material",
        "Assets/Models/crate.obj",
        "Assets/Scenes/Level.yaml",
        "Assets/Textures/crate.png",
        "Content/Models/Default/cube.obj",
        "Content/Models/Default/sphere.obj",
        "Content/Textures/Default/cells.png",
        "HedgehogEngine/HedgehogRenderer/assets/Graphs/game.graph",
        "HedgehogEngine/HedgehogRenderer/assets/Graphs/result.graph",
        "HedgehogEngine/HedgehogRenderer/assets/Graphs/scene.graph",
        "Project.yaml",
        "manifest.yaml",
    };
}
