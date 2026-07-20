#pragma once

#include <cstdint>
#include <vector>

namespace RHI
{

class IRHIRenderPass;
class IRHITexture;

struct FramebufferDesc
{
    const IRHIRenderPass*             RenderPass      = nullptr;
    std::vector<const IRHITexture*>   ColorAttachments;
    const IRHITexture*                DepthAttachment = nullptr;
    uint32_t                          Width           = 0;
    uint32_t                          Height          = 0;
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
