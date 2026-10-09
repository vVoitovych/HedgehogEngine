#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Containers/Mesh.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"

#include <cmath>
#include <filesystem>
#include <memory>
#include <string>

using HedgehogEngine::Mesh;

TEST_CASE("Mesh tangents - every vertex of the default cube and the skinned strip has a unit tangent orthogonal to "
          "its normal")
{
    // tests/ -> HedgehogEngine/ -> HedgehogEngine/ -> repository root.
    const std::filesystem::path root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
    FS::FileSystemManager files;
    auto                  fs = std::make_unique<FS::FileSystem>();
    fs->RegisterPath("engine://", root);
    fs->RegisterPath("assets://", root / "Projects/FeatureTest/Assets");
    REQUIRE(files.Register(std::move(fs)));

    for (const char* path : { "engine://Content/Models/Default/cube.obj", "Models/Animated/TwoBoneStrip.gltf" })
    {
        CAPTURE(path);
        Mesh mesh;
        REQUIRE(mesh.LoadData(path, files));
        const auto& tangents = mesh.GetTangents();
        const auto& normals  = mesh.GetNormals();
        REQUIRE(tangents.size() == mesh.GetPositions().size());
        for (size_t i = 0; i < tangents.size(); ++i)
        {
            const HM::Vector4& t = tangents[i];
            const HM::Vector3& n = normals[i];
            CHECK(std::abs(t.x() * t.x() + t.y() * t.y() + t.z() * t.z() - 1.0f) < 1e-4f);
            CHECK(std::abs(t.x() * n.x() + t.y() * n.y() + t.z() * n.z()) < 1e-3f);
            CHECK((t.w() == 1.0f || t.w() == -1.0f));
        }

        mesh.ClearData();
        CHECK(mesh.GetTangents().empty());
    }
}
