#pragma once

#include <cstdint>
#include <vector>

namespace RHI
{

class IRHIRenderPass;
class IRHITexture;

struct FramebufferDesc
{
    const IRHIRenderPass*             m_RenderPass      = nullptr;
    std::vector<const IRHITexture*>   m_ColorAttachments;
    const IRHITexture*                m_DepthAttachment = nullptr;
    uint32_t                          m_Width           = 0;
    uint32_t                          m_Height          = 0;
};

class IRHIFramebuffer
{
public:
    virtual ~IRHIFramebuffer() = default;

    IRHIFramebuffer(const IRHIFramebuffer&)            = delete;
    IRHIFramebuffer& operator=(const IRHIFramebuffer&) = delete;
    IRHIFramebuffer(IRHIFramebuffer&&)                 = delete;
    IRHIFramebuffer& operator=(IRHIFramebuffer&&)      = delete;

    virtual uint32_t GetWidth()  const = 0;
    virtual uint32_t GetHeight() const = 0;

protected:
    IRHIFramebuffer() = default;
};

} // namespace RHI
