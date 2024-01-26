#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <memory>

namespace Renderer
{
    class RenderContext;

    class InitPass
    {
    public:
        InitPass(const std::unique_ptr<RenderContext>& context);
        ~InitPass() = default;

        void Render(std::unique_ptr<RenderContext>& context);

        void Cleanup(const std::unique_ptr<RenderContext>& context);

    };

}

