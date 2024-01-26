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
        PresentPass(const std::unique_ptr<RenderContext>& context);
        ~PresentPass() = default;

        void Render(std::unique_ptr<RenderContext>& context);

        void Cleanup(const std::unique_ptr<RenderContext>& context);

    };


}


