#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <memory>

namespace Renderer
{
    class Device;
    class RenderContext;
    class SwapChain;

    class PresentPass
    {
    public:
        PresentPass(const std::unique_ptr<Device>& device, const std::unique_ptr<SwapChain>& swapChain);
        ~PresentPass() = default;

        void Render(std::unique_ptr<RenderContext>& renderContext);

        void Cleanup(const std::unique_ptr<Device>& device);
        void SetSwapChain(const std::unique_ptr<SwapChain>& swapChain);
    private:
        std::optional<VkDevice> mDevice;
        std::optional<VkSwapchainKHR> mSwapChain;
        std::optional<VkQueue> mGraphicQueue;
        std::optional<VkQueue> mPresentQueue;

    };


}


