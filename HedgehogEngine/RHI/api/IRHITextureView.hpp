#pragma once

#include "RHITypes.hpp"

namespace RHI
{

// A view over an explicit mip/array-layer range of an IRHITexture (IRHIDevice::CreateTextureView).
// Does not own the texture — the texture must outlive every view created over it.
class IRHITextureView
{
public:
    virtual ~IRHITextureView() = default;

    IRHITextureView(const IRHITextureView&)            = delete;
    IRHITextureView& operator=(const IRHITextureView&) = delete;
    IRHITextureView(IRHITextureView&&)                 = delete;
    IRHITextureView& operator=(IRHITextureView&&)      = delete;

    // The range this view was created with, already resolved against the source texture
    // (REMAINING_MIP_LEVELS / REMAINING_ARRAY_LAYERS replaced with concrete counts).
    virtual const TextureSubresourceRange& GetRange() const = 0;

protected:
    IRHITextureView() = default;
};

} // namespace RHI
