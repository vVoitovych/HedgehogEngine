#include "HedgehogRenderer/Graph/GraphReference.hpp"

#include "doctest/doctest/doctest.h"

#include <string>

using namespace Renderer;

TEST_CASE("A graph reference is a name unless it looks like a path")
{
    for (const char* name : { "game", "scene", "result", "my_graph-2" })
    {
        CAPTURE(name);
        CHECK(ClassifyGraphReference(name) == GraphReferenceKind::Name);
    }
    for (const char* file : { "assets://Graphs/bloom.graph", "engine://x", "D:/Graphs/bloom.graph",
                              "D:\\Graphs\\bloom.graph", "Graphs/bloom", "bloom.graph" })
    {
        CAPTURE(file);
        CHECK(ClassifyGraphReference(file) == GraphReferenceKind::File);
    }
    CHECK(IsVirtualGraphPath("assets://Graphs/bloom.graph"));
    CHECK_FALSE(IsVirtualGraphPath("D:/Graphs/bloom.graph"));
}

TEST_CASE("Normalizing gives every spelling of one file the same key, and leaves names alone")
{
    CHECK(NormalizeGraphReference("game") == "game");

    const std::string key = NormalizeGraphReference("D:/Graphs/bloom.graph");
    CHECK(key == "d:/Graphs/bloom.graph");
    CHECK(NormalizeGraphReference("D:\\Graphs\\bloom.graph") == key);
    CHECK(NormalizeGraphReference("d:/Graphs/./old/../bloom.graph") == key);

    // A virtual path keeps its mount's "//", and only its separators change.
    CHECK(NormalizeGraphReference("assets://Graphs\\bloom.graph") == "assets://Graphs/bloom.graph");
}
