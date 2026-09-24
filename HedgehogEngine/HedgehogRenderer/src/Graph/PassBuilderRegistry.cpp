#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

#include <cassert>

namespace Renderer
{
    bool PassBuilderRegistry::Register(std::string type, PassTypeInfo info)
    {
        assert(info.Build != nullptr && "PassBuilderRegistry::Register: a pass type needs a build function.");
        return m_Types.emplace(std::move(type), std::move(info)).second;
    }

    const PassTypeInfo* PassBuilderRegistry::Find(std::string_view type) const
    {
        const auto it = m_Types.find(std::string(type));
        return it != m_Types.end() ? &it->second : nullptr;
    }
}
