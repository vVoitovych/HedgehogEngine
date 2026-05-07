#pragma once

#include <optional>
#include <string>
#include <vector>

namespace Renderer
{
    struct AttachmentConfig
    {
        std::string m_Resource;
        std::string m_LoadOp;
        std::string m_StoreOp;
        std::string m_InitialLayout;
        std::string m_FinalLayout;
    };

    struct NodeConfig
    {
        std::string                     m_Name;
        std::string                     m_Type;
        std::string                     m_Shader;
        std::vector<AttachmentConfig>   m_ColorAttachments;
        std::optional<AttachmentConfig> m_DepthAttachment;
        std::vector<std::string>        m_InputResources;
        // Transition node fields
        std::string m_Resource;
        std::string m_FromLayout;
        std::string m_ToLayout;
    };

} // namespace Renderer
