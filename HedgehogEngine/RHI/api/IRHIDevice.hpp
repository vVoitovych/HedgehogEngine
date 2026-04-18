#pragma once

#include "RHITypes.hpp"
#include "IRHIBuffer.hpp"
#include "IRHITexture.hpp"
#include "IRHISampler.hpp"
#include "IRHIShader.hpp"
#include "IRHIDescriptor.hpp"
#include "IRHIRenderPass.hpp"
#include "IRHIFramebuffer.hpp"
#include "IRHIPipeline.hpp"
#include "IRHICommandList.hpp"
#include "IRHISwapchain.hpp"
#include "IRHISyncPrimitive.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace RHI
{

class IRHIDevice
{
public:
    virtual ~IRHIDevice() = default;

    IRHIDevice(const IRHIDevice&)            = delete;
    IRHIDevice& operator=(const IRHIDevice&) = delete;
    IRHIDevice(IRHIDevice&&)                 = delete;
    IRHIDevice& operator=(IRHIDevice&&)      = delete;

    // ── Resource creation (factory methods) ───────────────────────────────────

    virtual std::unique_ptr<IRHIBuffer> CreateBuffer(
        size_t      size,
        BufferUsage usage,
        MemoryUsage memUsage) const = 0;

    virtual std::unique_ptr<IRHITexture> CreateTexture(const TextureDesc& desc) const = 0;

    virtual std::unique_ptr<IRHISampler> CreateSampler(const SamplerDesc& desc) const = 0;

    virtual std::unique_ptr<IRHIShader>  CreateShader(
        const std::string& filePath,
        ShaderStage        stage) const = 0;

    // ── Descriptor resources ─────────────────────────────────────────────────

    virtual std::unique_ptr<IRHIDescriptorSetLayout> CreateDescriptorSetLayout(
        const std::vector<DescriptorBinding>& bindings) const = 0;

    virtual std::unique_ptr<IRHIDescriptorPool> CreateDescriptorPool(
        uint32_t                     maxSets,
        const std::vector<PoolSize>& poolSizes) const = 0;

    // Allocates a descriptor set from pool conforming to layout.
    virtual std::unique_ptr<IRHIDescriptorSet> AllocateDescriptorSet(
        const IRHIDescriptorPool&      pool,
        const IRHIDescriptorSetLayout& layout) const = 0;

    // ── Render pipeline ───────────────────────────────────────────────────────

    virtual std::unique_ptr<IRHIRenderPass>  CreateRenderPass(const RenderPassDesc& desc) const = 0;

    virtual std::unique_ptr<IRHIFramebuffer> CreateFramebuffer(const FramebufferDesc& desc) const = 0;

    virtual std::unique_ptr<IRHIPipeline>    CreateGraphicsPipeline(
        const GraphicsPipelineDesc& desc) const = 0;

    // ── Command recording ─────────────────────────────────────────────────────

    virtual std::unique_ptr<IRHICommandList> CreateCommandList() const = 0;

    // ── Presentation ─────────────────────────────────────────────────────────

    // Creates a swapchain for the window that was passed to Create().
    virtual std::unique_ptr<IRHISwapchain> CreateSwapchain(
        uint32_t width,
        uint32_t height) const = 0;

    // ── Synchronisation ───────────────────────────────────────────────────────

    virtual std::unique_ptr<IRHIFence>     CreateFence(bool signaled = false) const = 0;
    virtual std::unique_ptr<IRHISemaphore> CreateSemaphore() const = 0;

    // ── Submission ────────────────────────────────────────────────────────────

    virtual void SubmitCommandList(
        const IRHICommandList&             commandList,
        const std::vector<IRHISemaphore*>& waitSemaphores,
        const std::vector<IRHISemaphore*>& signalSemaphores,
        IRHIFence*                         signalFence = nullptr) = 0;

    // Record, submit, and wait for a one-shot command list.
    // Convenience wrapper for texture uploads / layout transitions.
    virtual void ExecuteImmediately(std::function<void(IRHICommandList&)> func) const = 0;

    virtual void WaitIdle() const = 0;

    // Returns the best depth format supported by the device.
    // Preference order: D32Float → D24UnormS8Uint → D16Unorm.
    virtual Format GetPreferredDepthFormat() const = 0;

    // ── Backend factory ───────────────────────────────────────────────────────

    // nativeWindowHandle: GLFWwindow* on GLFW-based platforms.
    // The backend creates its surface from this handle at construction time.
    static std::unique_ptr<IRHIDevice> Create(void* nativeWindowHandle);

protected:
    IRHIDevice() = default;
};

} // namespace RHI
