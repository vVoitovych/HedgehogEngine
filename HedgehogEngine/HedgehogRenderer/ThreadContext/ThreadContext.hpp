#pragma once

#include "HedgehogCommon/api/RendererSettings.hpp"

#include <memory>
#include <vector>

namespace RHI
{
    class IRHIDevice;
    class IRHICommandList;
    class IRHIFence;
    class IRHISemaphore;
}

namespace Renderer
{
    class ThreadContext
    {
    public:
        explicit ThreadContext(RHI::IRHIDevice& device);
        ~ThreadContext();

        ThreadContext(const ThreadContext&)            = delete;
        ThreadContext& operator=(const ThreadContext&) = delete;

        void Cleanup(RHI::IRHIDevice& device);

        void     NextFrame();
        uint32_t GetFrameIndex() const;

        RHI::IRHICommandList& GetCommandList();
        RHI::IRHIFence&       GetFence();
        RHI::IRHISemaphore&   GetImageAvailableSemaphore();
        RHI::IRHISemaphore&   GetRenderFinishedSemaphore();

    private:
        std::vector<std::unique_ptr<RHI::IRHICommandList>> m_CommandLists;
        std::vector<std::unique_ptr<RHI::IRHIFence>>       m_Fences;
        std::vector<std::unique_ptr<RHI::IRHISemaphore>>   m_ImageAvailableSemaphores;
        std::vector<std::unique_ptr<RHI::IRHISemaphore>>   m_RenderFinishedSemaphores;

        uint32_t m_FrameIndex = 0;
    };
}
