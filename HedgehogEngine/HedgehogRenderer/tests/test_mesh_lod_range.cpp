#include "doctest/doctest/doctest.h"

#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"

using Renderer::GetLodRange;
using Renderer::MeshDrawRange;

TEST_CASE("A mesh without levels of detail draws its own range at every level")
{
    MeshDrawRange mesh;
    mesh.FirstIndex = 120;
    mesh.IndexCount = 36;
    for (uint32_t lod = 0; lod < 6; ++lod)
    {
        const auto range = GetLodRange(mesh, lod);
        CHECK(range.FirstIndex == 120);
        CHECK(range.IndexCount == 36);
    }
}

TEST_CASE("A level of detail past the mesh's coarsest is clamped to it")
{
    MeshDrawRange mesh;
    mesh.FirstIndex   = 0;
    mesh.IndexCount   = 900;
    mesh.LodRanges[0] = { 0, 900 };
    mesh.LodRanges[1] = { 900, 450 };
    mesh.LodRanges[2] = { 1350, 222 };
    mesh.LodCount     = 3;

    CHECK(GetLodRange(mesh, 0).IndexCount == 900);
    CHECK(GetLodRange(mesh, 1).FirstIndex == 900);
    CHECK(GetLodRange(mesh, 1).IndexCount == 450);
    CHECK(GetLodRange(mesh, 2).FirstIndex == 1350);
    CHECK(GetLodRange(mesh, 3).FirstIndex == 1350);
    CHECK(GetLodRange(mesh, 99).IndexCount == 222);
}
