#include "doctest/doctest/doctest.h"

#include "Panels/TextSearch.hpp"

using Editor::ContainsIgnoringCase;

TEST_CASE("ContainsIgnoringCase finds a pattern anywhere in the text, in any case")
{
    CHECK(ContainsIgnoringCase("MainCamera", "camera"));
    CHECK(ContainsIgnoringCase("MainCamera", "MAIN"));
    CHECK(ContainsIgnoringCase("MainCamera", "nCa"));
    CHECK(ContainsIgnoringCase("MainCamera", "MainCamera"));
}

TEST_CASE("An empty pattern matches every text, the empty one included")
{
    CHECK(ContainsIgnoringCase("Lantern", ""));
    CHECK(ContainsIgnoringCase("", ""));
}

TEST_CASE("ContainsIgnoringCase reports no match when the pattern is absent or longer than the text")
{
    CHECK_FALSE(ContainsIgnoringCase("Lantern", "lamp"));
    CHECK_FALSE(ContainsIgnoringCase("room", "rooms"));
    CHECK_FALSE(ContainsIgnoringCase("", "a"));
}
