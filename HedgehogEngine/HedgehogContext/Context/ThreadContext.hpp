#pragma once

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogContext/Containers/LightContainer/Light.hpp"

#include <memory>
#include <vector>

namespace Wrappers
{
    class CommandBuffer;
    class SyncObject;
}

namespace RHI
{
    class IRHICommandList;
    class IRHIFence;
    class IRHISemaphore;
}

namespace Context
{
    class VulkanContext;
    class EngineContext;
    class FrameContext;

    class ThreadContext
    {
    public:
        ThreadContext(const VulkanContext& vulkanContext);
        ~ThreadContext();

        ThreadContext(const ThreadContext&) = delete;
        ThreadContext& operator=(const ThreadContext&) = delete;

        void Cleanup(const VulkanContext& vulkanContext);

        void NextFrame();
        uint32_t GetFrameIndex() const;

        // Legacy Wrappers API (kept until all render passes are migrated).
        Wrappers::CommandBuffer& GetCommandBuffer();
        Wrappers::SyncObject&    GetSyncObject();

        // New RHI API.
        RHI::IRHICommandList& GetCommandList();
        RHI::IRHIFence&       GetFence();
        RHI::IRHISemaphore&   GetImageAvailableSemaphore();
        RHI::IRHISemaphore&   GetRenderFinishedSemaphore();

    private:
        // Legacy Wrappers objects (removed once all render passes are migrated).
        std::vector<Wrappers::CommandBuffer> m_CommandBuffers;
        std::vector<Wrappers::SyncObject>    m_SyncObjects;

        // RHI objects — authoritative command recording and sync going forward.
        std::vector<std::unique_ptr<RHI::IRHICommandList>> m_CommandLists;
        std::vector<std::unique_ptr<RHI::IRHIFence>>       m_Fences;
        std::vector<std::unique_ptr<RHI::IRHISemaphore>>   m_ImageAvailableSemaphores;
        std::vector<std::unique_ptr<RHI::IRHISemaphore>>   m_RenderFinishedSemaphores;

        uint32_t m_FrameIndex = 0;
    };

}



