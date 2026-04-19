#pragma once

#include "RHITypes.hpp"

#include <optional>
#include <vector>

namespace RHI
{

struct RenderPassDesc
{
    std::vector<AttachmentDesc>     m_ColorAttachments;
    std::optional<AttachmentDesc>   m_DepthAttachment;
};

class IRHIRenderPass
{
public:
    virtual ~IRHIRenderPass() = default;

    IRHIRenderPass(const IRHIRenderPass&)            = delete;
    IRHIRenderPass& operator=(const IRHIRenderPass&) = delete;
    IRHIRenderPass(IRHIRenderPass&&)                 = delete;
    IRHIRenderPass& operator=(IRHIRenderPass&&)      = delete;

protected:
    IRHIRenderPass() = default;
};

} // namespace RHI
