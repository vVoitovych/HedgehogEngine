#pragma once

#include <string>

namespace Renderer
{
    struct NodeConfig
    {
        std::string m_Name;
        std::string m_Type;
        std::string m_Shader;      // RenderPass: relative path to .shader file
        std::string m_Resource;    // Transition: texture name from ResourceManager
        std::string m_FromLayout;  // Transition: source ImageLayout name
        std::string m_ToLayout;    // Transition: destination ImageLayout name
    };

} // namespace Renderer
