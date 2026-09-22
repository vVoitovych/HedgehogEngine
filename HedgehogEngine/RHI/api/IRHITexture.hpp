#pragma once

#include "RHITypes.hpp"

namespace RHI
{

class IRHITexture
{
public:
    virtual ~IRHITexture() = default;

    IRHITexture(const IRHITexture&)            = delete;
    IRHITexture& operator=(const IRHITexture&) = delete;
    IRHITexture(IRHITexture&&)                 = delete;
    IRHITexture& operator=(IRHITexture&&)      = delete;

    virtual uint32_t GetWidth()  const = 0;
    virtual uint32_t GetHeight() const = 0;
    virtual Format   GetFormat() const = 0;

    // The full description this texture was created from (or, for a swapchain image, a
    // description synthesized to match it) — what the graph reasons about for an imported
    // texture, since there is no TextureDesc on hand for those otherwise.
    virtual const TextureDesc& GetDesc() const = 0;

protected:
    IRHITexture() = default;
};

} // namespace RHI
