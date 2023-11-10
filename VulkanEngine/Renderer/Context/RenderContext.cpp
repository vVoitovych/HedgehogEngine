#include "RenderContext.hpp"

#include <stdexcept>

namespace Renderer
{
    void RenderContext::Initialize(Device& device)
    {
       // mBackBuffers.Initialize()
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            mCommandBuffers[i].Initialize(device);
        }
    }

    void RenderContext::Cleanup(Device& device)
    {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            mCommandBuffers[i].Cleanup(device);
        }
    }

    CommandBuffer& RenderContext::GetCommandBuffer(size_t index)
    {
        if (index >= MAX_FRAMES_IN_FLIGHT)
        {
            throw std::runtime_error("Validation layers requested, but not available!");
        }
        return mCommandBuffers[index];
    }

    VkExtent2D RenderContext::GetExtend() const
    {
        return mExtend;
    }

    uint32_t RenderContext::GetCurrentFrame() const
    {
        return mCurrentFrame;
    }

    void RenderContext::NextFrame()
    {
        mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

}

