#pragma once

#include <string>
#include <optional>

namespace Scene
{
    class RenderComponent
    {
    public:
        bool                    m_IsVisible     = true;
        std::string             m_Material;
        std::optional<uint64_t> m_MaterialIndex;
    };
}
