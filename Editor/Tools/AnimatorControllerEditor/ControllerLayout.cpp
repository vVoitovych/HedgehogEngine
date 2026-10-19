#include "ControllerLayout.hpp"

namespace Editor
{
    std::string MakeStateNodeKey(std::string_view stateName)
    {
        return "state:" + std::string(stateName);
    }

    void RenameStateNode(GraphLayout& layout, std::string_view oldName, std::string_view newName)
    {
        auto node = layout.extract(MakeStateNodeKey(oldName));
        if (!node)
            return;
        node.key() = MakeStateNodeKey(newName);
        layout.insert_or_assign(node.key(), node.mapped());
    }

    void RemoveStateNode(GraphLayout& layout, std::string_view stateName)
    {
        layout.erase(MakeStateNodeKey(stateName));
    }
}
