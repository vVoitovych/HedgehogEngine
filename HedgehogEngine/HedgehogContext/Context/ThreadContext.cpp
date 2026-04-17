#include "ThreadContext.hpp"
#include "VulkanContext.hpp"
#include "EngineContext.hpp"
#include "FrameContext.hpp"

#include "HedgehogWrappers/Wrappeers/Commands/CommandBuffer.hpp"
#include "HedgehogWrappers/Wrappeers/SyncObjects/SyncObject.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorAllocator.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorSetLayout.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/UBO.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Buffer/Buffer.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorSet.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorLayoutBuilder.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"

#include "HedgehogContext/Containers/LightContainer/LightContainer.hpp"
#include "HedgehogCommon/api/EngineDebugBreak.hpp"
#include "HedgehogCommon/api/RendererSettings.hpp"

#include "Logger/api/Logger.hpp"

#include <stdexcept>

namespace Context
{
    ThreadContext::ThreadContext(const VulkanContext& vulkanContext)
    {
        // Legacy Wrappers command buffers and sync objects.
        m_CommandBuffers.clear();
        m_SyncObjects.clear();
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            Wrappers::CommandBuffer commandBuffer(vulkanContext.GetDevice());
            m_CommandBuffers.push_back(std::move(commandBuffer));
            Wrappers::SyncObject syncObject(vulkanContext.GetDevice());
            m_SyncObjects.push_back(std::move(syncObject));
        }

        // New RHI command lists and sync primitives.
        auto& rhiDevice = vulkanContext.GetRHIDevice();
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            m_CommandLists.push_back(rhiDevice.CreateCommandList());
            m_Fences.push_back(rhiDevice.CreateFence(/*signaled=*/true));
            m_ImageAvailableSemaphores.push_back(rhiDevice.CreateSemaphore());
            m_RenderFinishedSemaphores.push_back(rhiDevice.CreateSemaphore());
        }

        LOGINFO("Thread context Initialized");
    }

    ThreadContext::~ThreadContext()
    {
    }

    void ThreadContext::Cleanup(const VulkanContext& vulkanContext)
    {
        // Destroy RHI objects first (device must be idle before destruction).
        vulkanContext.GetRHIDevice().WaitIdle();
        m_CommandLists.clear();
        m_Fences.clear();
        m_ImageAvailableSemaphores.clear();
        m_RenderFinishedSemaphores.clear();

        // Destroy legacy Wrappers objects.
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            m_CommandBuffers[i].Cleanup(vulkanContext.GetDevice());
            m_SyncObjects[i].Cleanup(vulkanContext.GetDevice());
        }
        m_CommandBuffers.clear();
        m_SyncObjects.clear();

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

    Wrappers::CommandBuffer& ThreadContext::GetCommandBuffer()
    {
        return m_CommandBuffers[m_FrameIndex];
    }

    Wrappers::SyncObject& ThreadContext::GetSyncObject()
    {
        return m_SyncObjects[m_FrameIndex];
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


