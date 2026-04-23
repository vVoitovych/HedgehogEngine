#include "HedgehogRenderer/ThreadContext/ThreadContext.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "Logger/api/Logger.hpp"

namespace Renderer
{
    ThreadContext::ThreadContext(RHI::IRHIDevice& device)
    {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            m_CommandLists.push_back(device.CreateCommandList());
            m_Fences.push_back(device.CreateFence(/*signaled=*/true));
            m_ImageAvailableSemaphores.push_back(device.CreateSemaphore());
            m_RenderFinishedSemaphores.push_back(device.CreateSemaphore());
        }

        LOGINFO("Thread context initialized");
    }

    ThreadContext::~ThreadContext()
    {
    }

    void ThreadContext::Cleanup(RHI::IRHIDevice& device)
    {
        device.WaitIdle();
        m_CommandLists.clear();
        m_Fences.clear();
        m_ImageAvailableSemaphores.clear();
        m_RenderFinishedSemaphores.clear();

        LOGINFO("Thread context cleaned");
    }

    void ThreadContext::NextFrame()
    {
        m_FrameIndex = (m_FrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    uint32_t ThreadContext::GetFrameIndex() const
    {
        return m_FrameIndex;
    }

    RHI::IRHICommandList& ThreadContext::GetCommandList()
    {
        return *m_CommandLists[m_FrameIndex];
    }

    RHI::IRHIFence& ThreadContext::GetFence()
    {
        return *m_Fences[m_FrameIndex];
    }

    RHI::IRHISemaphore& ThreadContext::GetImageAvailableSemaphore()
    {
        return *m_ImageAvailableSemaphores[m_FrameIndex];
    }

    RHI::IRHISemaphore& ThreadContext::GetRenderFinishedSemaphore()
    {
        return *m_RenderFinishedSemaphores[m_FrameIndex];
    }
}
