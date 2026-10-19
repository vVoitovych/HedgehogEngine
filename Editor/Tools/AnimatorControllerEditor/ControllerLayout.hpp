#pragma once

#include "Tools/RenderGraphEditor/GraphLayoutFile.hpp"

#include <string>
#include <string_view>

// Where the animator controller editor left its nodes. A controller's layout is a GraphLayout,
// kept beside it as "<name>.animctrl.layout" (GetGraphLayoutFile, LoadGraphLayout and
// SaveGraphLayout), so the .animctrl stays free of editor data. Its nodes are each state, by
// name, plus Entry and Any State.
namespace Editor
{
    inline constexpr std::string_view ENTRY_NODE_KEY     = "entry";
    inline constexpr std::string_view ANY_STATE_NODE_KEY = "any";

    // "state:<name>".
    [[nodiscard]] std::string MakeStateNodeKey(std::string_view stateName);

    // A renamed state keeps its place; a deleted one's place goes. Nothing for a state the
    // layout does not hold.
    void RenameStateNode(GraphLayout& layout, std::string_view oldName, std::string_view newName);
    void RemoveStateNode(GraphLayout& layout, std::string_view stateName);
}
