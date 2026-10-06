#include "doctest/doctest/doctest.h"

#include "Tools/PluginNameCheck.hpp"

#include <string>
#include <vector>

using HedgehogSettings::PluginEntry;

TEST_CASE("CheckNewPluginName - a DLL name not listed yet can be added")
{
    const std::vector<PluginEntry> listed = { { "Spinner", true } };
    CHECK(Editor::CheckNewPluginName("Physics", listed).empty());
    CHECK(Editor::CheckNewPluginName("My_Plugin-2", listed).empty());
    CHECK(Editor::CheckNewPluginName("Physics", {}).empty());
}

TEST_CASE("CheckNewPluginName - an empty, invalid or listed name is refused with a reason")
{
    const std::vector<PluginEntry> listed = { { "Spinner", true }, { "Off", false } };
    CHECK(Editor::CheckNewPluginName("", listed).find("Type") != std::string::npos);
    const std::string tooLong(65, 'a');
    for (const std::string bad : { std::string("My Plugin"), std::string("../Spinner"), std::string("Spinner.dll"), std::string("a/b"), tooLong })
        CHECK(Editor::CheckNewPluginName(bad, listed).find("1 to 64") != std::string::npos);
    CHECK(Editor::CheckNewPluginName("Spinner", listed) == "The project lists it already.");
    CHECK(Editor::CheckNewPluginName("SPINNER", listed) == "The project lists it already.");
    CHECK(Editor::CheckNewPluginName("off", listed) == "The project lists it already.");
}