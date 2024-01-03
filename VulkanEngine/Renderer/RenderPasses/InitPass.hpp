#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <memory>

namespace Renderer
{
    class Device;
    class SwapChain;
    class RenderContext;

    class InitPass
    {
    public:
        InitPass(const std::unique_ptr<Device>& device, const std::unique_ptr<SwapChain>& swapChain);
        ~InitPass() = default;

        void Render(std::unique_ptr<RenderContext>& renderContext);

        void Cleanup(const std::unique_ptr<Device>& device);

        void SetSwapChain(const std::unique_ptr<SwapChain>& swapChain);

    private:
        std::optional<VkDevice> mDevice;
        std::optional<VkSwapchainKHR> mSwapChain;
    };

}

