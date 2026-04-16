#pragma once

#include "RHITypes.hpp"

#include <cstdint>

namespace RHI
{

class IRHITexture;
class IRHISemaphore;

class IRHISwapchain
{
public:
    virtual ~IRHISwapchain() = default;

    IRHISwapchain(const IRHISwapchain&)            = delete;
    IRHISwapchain& operator=(const IRHISwapchain&) = delete;
    IRHISwapchain(IRHISwapchain&&)                 = delete;
    IRHISwapchain& operator=(IRHISwapchain&&)      = delete;

    virtual uint32_t GetImageCount() const = 0;
    virtual Format   GetFormat()     const = 0;
    virtual uint32_t GetWidth()      const = 0;
    virtual uint32_t GetHeight()     const = 0;

    // Returns the back-buffer texture for the given swapchain image index.
    // Ownership stays with the swapchain; do NOT store the pointer across frames.
    virtual IRHITexture& GetTexture(uint32_t index) = 0;

    // Acquire the next available image; signals signalSemaphore when ready.
    // Returns the image index to use for rendering and presentation.
    virtual uint32_t AcquireNextImage(IRHISemaphore& signalSemaphore) = 0;

    // Present image[imageIndex] after waitSemaphore is signaled.
    virtual void Present(uint32_t imageIndex, IRHISemaphore& waitSemaphore) = 0;

    // Recreate internal swapchain resources after a window resize.
    virtual void Resize(uint32_t width, uint32_t height) = 0;

protected:
    IRHISwapchain() = default;
};

} // namespace RHI
