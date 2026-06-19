#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include <optional>
#include <string>

namespace HedgehogEngine
{
HH_BEGIN_COMPONENT(RenderComponent)
    HH_PROP_NAMED(bool,        m_IsVisible, "Visible",  true,          None)
    HH_PROP_NAMED(std::string, m_Material,  "Material", std::string{}, None)

    std::optional<uint64_t> m_MaterialIndex; // runtime-only
HH_END_COMPONENT(RenderComponent)
}
